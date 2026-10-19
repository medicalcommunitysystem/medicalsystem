#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif
#include "patient.h"
#include "auth.h"
#include "db.h"
#include "member_a_internal.h"

#define PATIENT_COLUMNS "patient_id,name,gender,age,id_card,phone,address,username,create_time,status"

// 检查患者基本资料
static int profile_valid(const Patient *patient)
{
    return patient != NULL && patient->age>=0 &&
        a_text_valid(patient->name,sizeof patient->name,50,1) &&
        a_text_valid(patient->gender,sizeof patient->gender,4,0) &&
        a_text_valid(patient->id_card,sizeof patient->id_card,18,1) &&
        a_text_valid(patient->phone,sizeof patient->phone,20,0) &&
        a_text_valid(patient->address,sizeof patient->address,200,0);
}

// 检查患者注册信息
int patient_validate_registration(const Patient *patient, const char *password)
{
    if (!profile_valid(patient) || password == NULL || password[0]=='\0' ||
        strlen(password)>AUTH_PASSWORD_MAX_BYTES ||
        !a_text_valid(patient->username,sizeof patient->username,USERNAME_MAX_CHARS,1)) {
        return ERR_PARAM;
    }
    return ERR_OK;
}

// 检查管理员权限和账号状态
static int require_admin(const UserSession *session)
{
    int rc=auth_require_role(session,ROLE_ADMIN);
    return rc==AUTH_OK ? auth_require_active(session) : rc;
}

// 将当前查询行转换为患者资料
static int read_patient(Patient *out)
{
    Patient item={0};
    int rc=a_copy_text(item.patient_id,sizeof item.patient_id,db_get_field(0),1);
    if (rc==ERR_OK) { rc=a_copy_text(item.name,sizeof item.name,db_get_field(1),1); }
    if (rc==ERR_OK) { rc=a_copy_text(item.gender,sizeof item.gender,db_get_field(2),0); }
    if (rc==ERR_OK && db_get_field(3)!=NULL) { rc=a_parse_int(db_get_field(3),0,INT_MAX,&item.age); }
    if (rc==ERR_OK) { rc=a_copy_text(item.id_card,sizeof item.id_card,db_get_field(4),1); }
    if (rc==ERR_OK) { rc=a_copy_text(item.phone,sizeof item.phone,db_get_field(5),0); }
    if (rc==ERR_OK) { rc=a_copy_text(item.address,sizeof item.address,db_get_field(6),0); }
    if (rc==ERR_OK) { rc=a_copy_text(item.username,sizeof item.username,db_get_field(7),1); }
    if (rc==ERR_OK) { rc=a_copy_text(item.create_time,sizeof item.create_time,db_get_field(8),0); }
    if (rc==ERR_OK) { rc=a_parse_int(db_get_field(9),0,1,&item.status); }
    if (rc==ERR_OK) { *out=item; }
    return rc;
}

// 按编号读取患者资料
static int get_patient(const char *id, Patient *out)
{
    const char *values[]={id};
    int rc;
    if (out == NULL || id == NULL || id[0]=='\0' || strlen(id)>20) { return ERR_PARAM; }
    rc=db_query_params("SELECT " PATIENT_COLUMNS " FROM t_patient WHERE patient_id=?",values,1);
    if (rc!=ERR_OK) { return rc; }
    rc=db_fetch_row();
    if (rc==1) { rc=read_patient(out); }
    else if (rc==0) { rc=ERR_NOT_FOUND; }
    db_free_result();
    return rc;
}

// 将数据库重复错误转为业务错误码
static int db_write_error(int rc)
{
    return rc<0 && db_errno()==MYSQL_ER_DUP_ENTRY ? ERR_DB_DUP : rc;
}

// 生成患者编号并保存注册资料
static int create_patient(Patient *patient, const char *password)
{
    Patient saved={0};
    char hash[AUTH_HASH_CAPACITY]={0},id[21],age[16];
    int rc,sequence,year;
    if (patient_validate_registration(patient,password)!=ERR_OK) { return ERR_PARAM; }
    rc=auth_username_exists(patient->username);
    if (rc!=0) { return rc==1 ? ERR_DB_DUP : rc; }
    if (auth_hash_password(password,hash,sizeof hash)!=0) { return ERR_INVALID_STATE; }
    rc=db_begin();
    if (rc!=ERR_OK) { goto done; }
    rc=db_execute("UPDATE t_member_a_sequence SET next_value=LAST_INSERT_ID(next_value+1) "
                  "WHERE sequence_name='patient' AND next_value<99999999");
    if (rc<0) { goto rollback; }
    if (rc!=1) { rc=ERR_LIMIT; goto rollback; }
    sequence=db_last_insert_id();
    rc=db_query("SELECT YEAR(CURRENT_DATE())");
    if (rc!=ERR_OK) { goto rollback; }
    rc=db_fetch_row();
    if (rc!=1) { rc=ERR_DB_RESULT; goto rollback; }
    rc=a_parse_int(db_get_field(0),1000,9999,&year);
    db_free_result();
    if (rc!=ERR_OK || sequence<1) { rc=ERR_DB_RESULT; goto rollback; }
    snprintf(id,sizeof id,"P%04d%08d",year,sequence);
    snprintf(age,sizeof age,"%d",patient->age);
    {
        const char *values[]={id,patient->name,patient->gender,age,patient->id_card,
            patient->phone,patient->address,patient->username,hash};
        rc=db_execute_params("INSERT INTO t_patient(patient_id,name,gender,age,id_card,"
            "phone,address,username,password,status) VALUES(?,?,?,?,?,?,?,?,?,1)",values,9);
    }
    if (rc<0) { rc=db_write_error(rc); goto rollback; }
    if (rc!=1) { rc=ERR_DB_RESULT; goto rollback; }
    rc=get_patient(id,&saved);
    if (rc!=ERR_OK) { goto rollback; }
    rc=db_commit();
    if (rc!=ERR_OK) { goto rollback; }
    *patient=saved;
    goto done;
rollback:
    if (db_rollback()!=ERR_OK) { rc=ERR_DB_ROLLBACK; }
done:
    auth_secure_clear(hash,sizeof hash);
    return rc;
}

// 患者自助注册
int patient_register(Patient *patient, const char *password)
{
    return create_patient(patient,password);
}

// 管理员新增患者
int patient_admin_add(const UserSession *session, Patient *patient, const char *password)
{
    int rc=require_admin(session);
    return rc==AUTH_OK ? create_patient(patient,password) : rc;
}

// 根据当前会话查询本人档案
int patient_get_my(const UserSession *session, Patient *out)
{
    int rc=auth_require_role(session,ROLE_PATIENT);
    if (rc==AUTH_OK) { rc=auth_require_active(session); }
    return rc==AUTH_OK ? get_patient(session->user_id,out) : rc;
}

// 修改本人的电话和地址
int patient_update_my_contact(const UserSession *session, const char *phone, const char *address)
{
    Patient original;
    const char *values[3];
    int rc=patient_get_my(session,&original);
    if (rc!=ERR_OK) { return rc; }
    if (phone == NULL || address == NULL ||
        !a_text_valid(phone,strlen(phone)+1,20,0) || strlen(phone)>=sizeof original.phone ||
        !a_text_valid(address,strlen(address)+1,200,0) || strlen(address)>=sizeof original.address) {
        return ERR_PARAM;
    }
    values[0]=phone; values[1]=address; values[2]=session->user_id;
    rc=db_execute_params("UPDATE t_patient SET phone=?,address=? "
                         "WHERE patient_id=? AND status=1",values,3);
    if (rc<0) { return rc; }
    if (rc>1) { return ERR_DB_RESULT; }
    // 资料未变化时，受影响行数可能为零
    if (rc==0) { return patient_get_my(session,&original); }
    return ERR_OK;
}

// 管理员按编号查询患者
int patient_admin_query(const UserSession *session, const char *id, Patient *out)
{
    int rc=require_admin(session);
    return rc==AUTH_OK ? get_patient(id,out) : rc;
}

// 管理员分页查询患者
int patient_query_all(const UserSession *session, Patient *list, int max, int offset)
{
    char sql[512];
    int count=0,rc=require_admin(session);
    if (rc!=AUTH_OK) { return rc; }
    if (list == NULL || max<1 || max>1000 || offset<0) { return ERR_PARAM; }
    snprintf(sql,sizeof sql,"SELECT " PATIENT_COLUMNS
        " FROM t_patient ORDER BY patient_id LIMIT %d OFFSET %d",max,offset);
    rc=db_query(sql);
    if (rc!=ERR_OK) { return rc; }
    while (count<max && (rc=db_fetch_row())==1) {
        rc=read_patient(&list[count]);
        if (rc!=ERR_OK) { break; }
        ++count;
    }
    db_free_result();
    return rc<0 ? rc : count;
}

// 管理员修改患者基本资料
int patient_admin_update(const UserSession *session, const Patient *patient)
{
    Patient original;
    char age[16];
    int rc=require_admin(session);
    if (rc!=AUTH_OK) { return rc; }
    if (!profile_valid(patient) ||
        !a_text_valid(patient->patient_id,sizeof patient->patient_id,20,1)) { return ERR_PARAM; }
    rc=get_patient(patient->patient_id,&original);
    if (rc!=ERR_OK) { return rc; }
    snprintf(age,sizeof age,"%d",patient->age);
    {
        const char *values[]={patient->name,patient->gender,age,patient->id_card,
            patient->phone,patient->address,patient->patient_id};
        rc=db_execute_params("UPDATE t_patient SET name=?,gender=?,age=?,id_card=?,"
                             "phone=?,address=? WHERE patient_id=?",values,7);
    }
    if (rc<0) { return db_write_error(rc); }
    return rc<=1 ? ERR_OK : ERR_DB_RESULT;
}

// 注销患者账号，保留历史资料
int patient_deactivate(const UserSession *session, const char *id)
{
    Patient original;
    const char *values[]={id};
    int rc=patient_admin_query(session,id,&original);
    if (rc!=ERR_OK) { return rc; }
    rc=db_execute_params("UPDATE t_patient SET status=0 WHERE patient_id=?",values,1);
    return rc<0 ? rc : (rc<=1 ? ERR_OK : ERR_DB_RESULT);
}

// 转义并写入一个导出字段
static int csv_cell(FILE *file, const char *text)
{
    if (text == NULL) { text=""; }
    if (fputc('"',file)==EOF) { return ERR_IO; }
    // 避免表格软件将文本当作公式
    if (text[0]!='\0' && ((unsigned char)text[0]<=32 || strchr("=+-@",text[0])!=NULL)) {
        if (fputc('\'',file)==EOF) { return ERR_IO; }
    }
    for (;*text!='\0';++text) {
        if (*text=='"' && fputc('"',file)==EOF) { return ERR_IO; }
        if (fputc((unsigned char)*text,file)==EOF) { return ERR_IO; }
    }
    return fputc('"',file)==EOF ? ERR_IO : ERR_OK;
}

// 创建导出文件，不覆盖已有文件
static FILE *open_export_file(const char *path)
{
    int descriptor;
    FILE *file;
#ifdef _WIN32
    int size=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,NULL,0);
    wchar_t *wide;
    if (size==0) { return NULL; }
    wide=calloc((size_t)size,sizeof *wide);
    if (wide==NULL) { return NULL; }
    if (!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,wide,size)) {
        free(wide); return NULL;
    }
    descriptor=_wopen(wide,_O_CREAT|_O_EXCL|_O_WRONLY|_O_BINARY,_S_IREAD|_S_IWRITE);
    free(wide);
    if (descriptor<0) { return NULL; }
    file=_fdopen(descriptor,"wb");
    if (file==NULL) { _close(descriptor); }
#else
    descriptor=open(path,O_CREAT|O_EXCL|O_WRONLY,S_IRUSR|S_IWUSR);
    if (descriptor<0) { return NULL; }
    file=fdopen(descriptor,"w");
    if (file==NULL) { close(descriptor); }
#endif
    return file;
}

// 管理员导出患者资料，不含密码和身份证号
int patient_export(const UserSession *session, const char *path, int *exported)
{
    static const int columns[]={0,1,2,3,5,6,7,8,9};
    FILE *file;
    int count=0,rc=require_admin(session);
    if (exported!=NULL) { *exported=0; }
    if (rc!=AUTH_OK) { return rc; }
    if (path == NULL || path[0]=='\0' || exported == NULL) { return ERR_PARAM; }
    rc=db_query("SELECT " PATIENT_COLUMNS " FROM t_patient ORDER BY patient_id");
    if (rc!=ERR_OK) { return rc; }
    file=open_export_file(path);
    if (file == NULL) { db_free_result(); return ERR_IO; }
    if (fputs("patient_id,name,gender,age,phone,address,username,create_time,status\n",file)==EOF) {
        rc=ERR_IO; goto done;
    }
    while ((rc=db_fetch_row())==1) {
        for (size_t i=0;i<sizeof columns/sizeof columns[0];++i) {
            rc=csv_cell(file,db_get_field(columns[i]));
            if (rc!=ERR_OK || fputc(i==8 ? '\n' : ',',file)==EOF) { rc=ERR_IO; goto done; }
        }
        if (count==INT_MAX) { rc=ERR_LIMIT; goto done; }
        ++count;
    }
done:
    db_free_result();
    if (fclose(file)!=0 && rc>=0) { rc=ERR_IO; }
    if (rc<0) { return rc; }
    *exported=count;
    return ERR_OK;
}
