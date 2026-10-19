#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "appointment.h"
#include "auth.h"
#include "db.h"
#include "member_a_internal.h"

#define APPOINTMENT_COLUMNS "appoint_id,patient_id,doctor_id,appoint_date,time_slot,symptom,status,create_time"

// 检查角色权限和账号状态
static int authorize(const UserSession *session, UserRole role)
{
    int rc=auth_require_role(session,role);
    return rc==AUTH_OK ? auth_require_active(session) : rc;
}

// 将当前查询行转换为预约记录
static int appointment_read_current_row(Appointment *out)
{
    Appointment item={0};
    int rc=a_parse_int(db_get_field(0),1,INT_MAX,&item.appoint_id);
    if (rc==ERR_OK) { rc=a_copy_text(item.patient_id,sizeof item.patient_id,db_get_field(1),1); }
    if (rc==ERR_OK) { rc=a_copy_text(item.doctor_id,sizeof item.doctor_id,db_get_field(2),1); }
    if (rc==ERR_OK) { rc=a_copy_text(item.appoint_date,sizeof item.appoint_date,db_get_field(3),0); }
    if (rc==ERR_OK) { rc=a_copy_text(item.time_slot,sizeof item.time_slot,db_get_field(4),0); }
    if (rc==ERR_OK) { rc=a_copy_text(item.symptom,sizeof item.symptom,db_get_field(5),0); }
    if (rc==ERR_OK) { rc=a_parse_int(db_get_field(6),0,2,&item.status); }
    if (rc==ERR_OK) { rc=a_copy_text(item.create_time,sizeof item.create_time,db_get_field(7),0); }
    if (rc==ERR_OK) { *out=item; }
    return rc;
}

// 执行查询并读取预约列表
static int query_list(const char *sql, const char *const *values, size_t n, Appointment *list, int max)
{
    int count=0,rc=db_query_params(sql,values,n);
    if (rc!=ERR_OK) { return rc; }
    while (count<max && (rc=db_fetch_row())==1) {
        rc=appointment_read_current_row(&list[count]);
        if (rc!=ERR_OK) { break; }
        ++count;
    }
    db_free_result();
    return rc<0 ? rc : count;
}

// 查询本人预约首页
int appointment_query_my(const UserSession *session, Appointment *list, int max)
{
    return appointment_query_my_page(session,list,max,0);
}

// 分页查询当前患者的预约
int appointment_query_my_page(const UserSession *session, Appointment *list, int max, int offset)
{
    char sql[512];
    const char *values[1];
    int rc=authorize(session,ROLE_PATIENT);
    if (rc!=AUTH_OK) { return rc; }
    if (list == NULL || max<1 || max>1000 || offset<0) { return ERR_PARAM; }
    snprintf(sql,sizeof sql,"SELECT " APPOINTMENT_COLUMNS
             " FROM t_appointment WHERE patient_id=? ORDER BY appoint_id DESC LIMIT %d OFFSET %d",max,offset);
    values[0]=session->user_id;
    return query_list(sql,values,1,list,max);
}

// 查询医生今日待就诊预约首页
int appointment_query_doctor_today(const UserSession *session, Appointment *list, int max)
{
    return appointment_query_doctor_today_page(session,list,max,0);
}

// 分页查询医生今日待就诊预约
int appointment_query_doctor_today_page(const UserSession *session, Appointment *list, int max, int offset)
{
    char sql[512];
    const char *values[1];
    int rc=authorize(session,ROLE_DOCTOR);
    if (rc!=AUTH_OK) { return rc; }
    if (list == NULL || max<1 || max>1000 || offset<0) { return ERR_PARAM; }
    snprintf(sql,sizeof sql,"SELECT " APPOINTMENT_COLUMNS
        " FROM t_appointment WHERE doctor_id=? AND appoint_date=CURRENT_DATE() "
        "AND status=0 ORDER BY appoint_id LIMIT %d OFFSET %d",max,offset);
    values[0]=session->user_id;
    return query_list(sql,values,1,list,max);
}

// 管理员分页查询全部预约
int appointment_query_all(const UserSession *session, Appointment *list, int max, int offset)
{
    char sql[512];
    int rc=authorize(session,ROLE_ADMIN);
    if (rc!=AUTH_OK) { return rc; }
    if (list == NULL || max<1 || max>1000 || offset<0) { return ERR_PARAM; }
    snprintf(sql,sizeof sql,"SELECT " APPOINTMENT_COLUMNS
             " FROM t_appointment ORDER BY appoint_id DESC LIMIT %d OFFSET %d",max,offset);
    return query_list(sql,NULL,0,list,max);
}

// 分页查询可预约医生
int appointment_query_available_doctors(const UserSession *session, AppointmentDoctor *list, int max, int offset)
{
    char sql[256];
    int count=0,rc=authorize(session,ROLE_PATIENT);
    if (rc!=AUTH_OK) { return rc; }
    if (list == NULL || max<1 || max>1000 || offset<0) { return ERR_PARAM; }
    snprintf(sql,sizeof sql,"SELECT doctor_id,name,dept,title FROM t_doctor "
             "WHERE status=1 ORDER BY doctor_id LIMIT %d OFFSET %d",max,offset);
    rc=db_query(sql);
    if (rc!=ERR_OK) { return rc; }
    while (count<max && (rc=db_fetch_row())==1) {
        AppointmentDoctor item={0};
        rc=a_copy_text(item.doctor_id,sizeof item.doctor_id,db_get_field(0),1);
        if (rc==ERR_OK) { rc=a_copy_text(item.name,sizeof item.name,db_get_field(1),1); }
        if (rc==ERR_OK) { rc=a_copy_text(item.dept,sizeof item.dept,db_get_field(2),0); }
        if (rc==ERR_OK) { rc=a_copy_text(item.title,sizeof item.title,db_get_field(3),0); }
        if (rc!=ERR_OK) { break; }
        list[count++]=item;
    }
    db_free_result();
    return rc<0 ? rc : count;
}

// 锁定患者记录并检查账号状态
static int lock_patient(const char *id)
{
    const char *values[]={id};
    int status,rc=db_query_params("SELECT status FROM t_patient WHERE patient_id=? FOR UPDATE",values,1);
    if (rc!=ERR_OK) { return rc; }
    rc=db_fetch_row();
    if (rc!=1) { db_free_result(); return rc<0 ? rc : ERR_NOT_FOUND; }
    rc=a_parse_int(db_get_field(0),0,1,&status);
    db_free_result();
    return rc!=ERR_OK ? rc : (status==1 ? ERR_OK : AUTH_ERR_DISABLED);
}

// 检查医生和日期后为当前患者预约
int appointment_book(const UserSession *session, const char *doctor_id, const char *date, const char *time_slot, const char *symptom, int *out_appoint_id)
{
    int rc=authorize(session,ROLE_PATIENT),status,created_id;
    if (rc!=AUTH_OK) { return rc; }
    if (out_appoint_id == NULL || doctor_id == NULL || strlen(doctor_id)>10 ||
        !a_text_valid(doctor_id,strlen(doctor_id)+1,10,1) || !a_date_valid(date) ||
        time_slot == NULL || !a_text_valid(time_slot,strlen(time_slot)+1,10,1) ||
        symptom == NULL || !a_text_valid(symptom,strlen(symptom)+1,200,0)) { return ERR_PARAM; }
    rc=db_begin();
    if (rc!=ERR_OK) { return rc; }
    rc=lock_patient(session->user_id);
    if (rc!=ERR_OK) { goto rollback; }
    {
        const char *values[]={doctor_id};
        rc=db_query_params("SELECT status FROM t_doctor WHERE doctor_id=? FOR SHARE",values,1);
    }
    if (rc!=ERR_OK) { goto rollback; }
    rc=db_fetch_row();
    if (rc!=1) { rc=rc<0 ? rc : ERR_NOT_FOUND; goto rollback; }
    rc=a_parse_int(db_get_field(0),0,1,&status);
    db_free_result();
    if (rc!=ERR_OK) { goto rollback; }
    if (status!=1) { rc=ERR_INVALID_STATE; goto rollback; }
    {
        const char *values[]={date};
        rc=db_query_params("SELECT CAST(? AS DATE)>=CURRENT_DATE()",values,1);
    }
    if (rc!=ERR_OK) { goto rollback; }
    if (db_fetch_row()!=1 || a_parse_int(db_get_field(0),0,1,&status)!=ERR_OK) {
        rc=ERR_DB_RESULT; goto rollback;
    }
    db_free_result();
    if (status!=1) { rc=ERR_PARAM; goto rollback; }
    {
        const char *values[]={session->user_id,doctor_id,date,time_slot,symptom};
        rc=db_execute_params("INSERT INTO t_appointment(patient_id,doctor_id,appoint_date,"
                             "time_slot,symptom,status) VALUES(?,?,?,?,?,0)",values,5);
    }
    if (rc<0) {
        if (db_errno()==MYSQL_ER_DUP_ENTRY) { rc=ERR_DB_DUP; }
        goto rollback;
    }
    if (rc!=1 || (created_id=db_last_insert_id())<1) { rc=ERR_DB_RESULT; goto rollback; }
    rc=db_commit();
    if (rc!=ERR_OK) { goto rollback; }
    *out_appoint_id=created_id;
    return ERR_OK;
rollback:
    return db_rollback()==ERR_OK ? rc : ERR_DB_ROLLBACK;
}

// 取消本人待就诊预约
int appointment_cancel_my(const UserSession *session, int appoint_id)
{
    char id[16];
    const char *values[2];
    int rc=authorize(session,ROLE_PATIENT);
    if (rc!=AUTH_OK) { return rc; }
    if (appoint_id<=0) { return ERR_PARAM; }
    snprintf(id,sizeof id,"%d",appoint_id);
    values[0]=id; values[1]=session->user_id;
    rc=db_execute_params("UPDATE t_appointment SET status=2 "
                         "WHERE appoint_id=? AND patient_id=? AND status=0",values,2);
    if (rc<0) { return rc; }
    return rc==1 ? ERR_OK : (rc==0 ? ERR_NOT_FOUND : ERR_DB_RESULT);
}
