#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#ifndef _WIN32
#include <termios.h>
#include <unistd.h>
#endif
#include "doctor.h"
#include "visit.h"
#include "medical_record.h"
#include "b_db.h"

static void report(int rc)
{
    if(rc>=0) puts("操作成功。");
    else {
        printf("失败 [%d]：%s\n",rc,b_error_message(rc));
        if(rc<=ERR_DB_CONN && rc>=ERR_DB_ROLLBACK)
            printf("数据库详情：%s / %s\n",bdb_error(),db_error());
    }
}
static int number(const char *prompt,int *out)
{ return common_read_int(prompt,1,INT_MAX,out); }
#define READ(prompt, buffer) do { int rr=common_read_line(prompt,buffer,sizeof(buffer)); if(rr<0) return rr; } while(0)

static int password_input(char *out,size_t n)
{
    int rc;
#ifndef _WIN32
    struct termios old,new_settings;
    int changed=0;
    if(isatty(STDIN_FILENO) && tcgetattr(STDIN_FILENO,&old)==0) {
        new_settings=old; new_settings.c_lflag &= (tcflag_t)~ECHO;
        if(tcsetattr(STDIN_FILENO,TCSAFLUSH,&new_settings)==0) changed=1;
    }
#endif
    rc=common_read_line("密码：",out,n);
#ifndef _WIN32
    if(changed) { tcsetattr(STDIN_FILENO,TCSAFLUSH,&old); putchar('\n'); }
#endif
    return rc;
}
static int list_doctors(const UserSession *s)
{
    Doctor list[10]; int off=0,n,i,next,rc;
    do {
        n=doctor_query_all(s,0,list,10,off);
        if(n<0) return n;
        puts("编号 | 姓名 | 性别 | 职称 | 科室 | 电话 | 状态");
        for(i=0;i<n;i++) printf("%s | %s | %s | %s | %s | %s | %s\n",list[i].doctor_id,list[i].name,
            list[i].gender,list[i].title,list[i].dept,list[i].phone,list[i].status==1?"在职":"离职");
        if(!n) puts("没有更多记录。");
        if(n<10) break;
        if((rc=common_read_int("1 下一页，0 返回：",0,1,&next))<0) return rc;
        off+=10;
    } while(next);
    return ERR_OK;
}
static int edit_doctor(const UserSession *s,int add)
{
    DoctorInput d;
    Doctor current;
    char id[11],name[201],gender[17],title[81],dept[201],phone[81],username[201],password[401];
    int rc;
    if(!add) {
        if(s->role==ROLE_DOCTOR) common_copy(id,sizeof(id),s->user_id);
        else { READ("医生编号：",id); }
        if((rc=doctor_query_by_id(s,id,&current))<0) return rc;
        printf("当前：%s %s %s %s %s\n",current.name,current.gender,current.title,current.dept,current.phone);
    }
    READ("姓名：",name); READ("性别（可空）：",gender);
    if(s->role==ROLE_DOCTOR) {
        common_copy(title,sizeof(title),current.title); common_copy(dept,sizeof(dept),current.dept);
    } else { READ("职称（可空）：",title); READ("科室（可空）：",dept); }
    READ("电话（可空）：",phone);
    d.name=name; d.gender=gender; d.title=title; d.dept=dept; d.phone=phone;
    d.username=NULL; d.password=NULL;
    if(add) {
        READ("登录账号：",username);
        if((rc=password_input(password,sizeof(password)))<0) return rc;
        d.username=username; d.password=password;
        rc=doctor_add(s,&d,id); memset(password,0,sizeof(password));
        if(rc==ERR_OK) printf("新增医生编号：%s\n",id);
        return rc;
    }
    return doctor_update(s,id,&d);
}
static int deactivate(const UserSession *s)
{
    char id[11]; int confirm,rc;
    READ("停用医生编号：",id);
    if((rc=common_read_int("停用后无法登录和被挂号选择。输入1确认，0取消：",0,1,&confirm))<0) return rc;
    return confirm ? doctor_delete(s,id) : ERR_OK;
}
static int pending(const UserSession *s)
{
    PendingAppointment list[10]; int off=0,n,i,next,rc;
    do {
        n=appointment_query_by_doctor_today(s,list,10,off);
        if(n<0) return n;
        for(i=0;i<n;i++) printf("预约%d | %s %s | %s %s | %s\n",list[i].appoint_id,list[i].patient_id,
            list[i].patient_name,list[i].appoint_date,list[i].time_slot,list[i].symptom);
        if(n<10) { if(!n) puts("没有更多待接诊预约。"); break; }
        if((rc=common_read_int("1 下一页，0 返回：",0,1,&next))<0) return rc;
        off+=10;
    } while(next);
    return ERR_OK;
}
static int take_visit(const UserSession *s)
{
    VisitInput v; char chief[8001],illness[8001],bp[41]; int aid,vid,rc;
    if((rc=number("预约ID：",&aid))<0) return rc;
    READ("主诉：",chief); READ("现病史（可空）：",illness);
    if((rc=common_read_double("体温 ℃（25～45）：",25,45,&v.temp))<0) return rc;
    READ("血压，如120/80：",bp);
    if((rc=common_read_int("心率（1～300）：",1,300,&v.heart_rate))<0) return rc;
    v.chief_complaint=chief; v.present_illness=illness; v.blood_pressure=bp;
    rc=visit_add(s,aid,&v,&vid);
    if(rc==ERR_OK) printf("接诊成功，就诊ID：%d\n",vid);
    return rc;
}
static int list_visits(const UserSession *s)
{
    VisitRecord *list=calloc(10,sizeof(*list));
    char patient[21]; int n,i,off=0,next,rc=ERR_OK;
    if(!list) return ERR_NOMEM;
    if(s->role==ROLE_PATIENT) common_copy(patient,sizeof(patient),s->user_id);
    else {
        rc=common_read_line("患者编号（留空查权限内全部）：",patient,sizeof(patient));
        if(rc<0) { free(list); return rc; }
    }
    do {
        n=patient[0] ? visit_query_by_patient(s,patient,list,10,off) : visit_query_all(s,list,10,off);
        if(n<0) { rc=n; break; }
        for(i=0;i<n;i++) {
            VisitRecord *v=&list[i];
            printf("\n就诊%d | 预约%d | 患者%s | 医生%s | %s\n主诉：%s\n现病史：%s\n体温%.1f 血压%s 心率%d\n",
                v->visit_id,v->appoint_id,v->patient_id,v->doctor_id,v->visit_time,v->chief_complaint,
                v->present_illness,v->temp,v->blood_pressure,v->heart_rate);
        }
        if(n<10) { if(!n) puts("没有更多就诊记录。"); break; }
        if((rc=common_read_int("1 下一页，0 返回：",0,1,&next))<0) break;
        off+=10;
    } while(next);
    free(list); return rc;
}
static int edit_record(const UserSession *s,int add)
{
    MedicalRecordInput m; char allergy[801],past[801],family[801]; int id,out,rc;
    if((rc=number(add?"就诊ID：":"病历ID：",&id))<0) return rc;
    READ("过敏史（直接回车表示无过敏）：",allergy);
    READ("既往史（可空）：",past); READ("家族史（可空）：",family);
    m.allergy_history=allergy; m.past_history=past; m.family_history=family;
    rc=add ? medical_record_add(s,id,&m,&out) : medical_record_update(s,id,&m);
    if(add && rc==ERR_OK) printf("新增病历ID：%d\n",out);
    return rc;
}
static int list_records(const UserSession *s)
{
    MedicalRecord list[10]; char patient[21]; int n,i,off=0,next,rc;
    if(s->role==ROLE_PATIENT) common_copy(patient,sizeof(patient),s->user_id);
    else { READ("患者编号（留空查权限内全部）：",patient); }
    do {
        n=patient[0] ? medical_record_query_by_patient(s,patient,list,10,off) : medical_record_query_all(s,list,10,off);
        if(n<0) return n;
        for(i=0;i<n;i++) {
            MedicalRecord *m=&list[i];
            printf("\n病历%d | 就诊%d | 患者%s | 医生%s\n创建%s | 更新%s\n过敏史：%s\n既往史：%s\n家族史：%s\n",
                m->record_id,m->visit_id,m->patient_id,m->doctor_id,m->create_time,m->update_time,
                m->allergy_history[0]?m->allergy_history:"无过敏",m->past_history,m->family_history);
        }
        if(n<10) { if(!n) puts("没有更多病历。"); break; }
        if((rc=common_read_int("1 下一页，0 返回：",0,1,&next))<0) return rc;
        off+=10;
    } while(next);
    return ERR_OK;
}
int menu_member_b(const UserSession *session)
{
    UserSession actor;
    int choice,rc;
    if((rc=b_check_session(session))<0) { report(rc); return rc; }
    actor=*session;
    for(;;) {
        puts("\n1 医生列表  2 就诊记录  3 病历查询  0 返回上级菜单");
        if(actor.role==ROLE_ADMIN) puts("4 新增医生  5 修改医生资料  6 停用医生");
        if(actor.role==ROLE_DOCTOR) puts("5 修改本人资料  7 当日待接诊  8 接诊  9 新增病历  10 修改病历");
        rc=common_read_int("选择：",0,10,&choice);
        if(rc==B_ERR_INPUT) { rc=ERR_OK; break; }
        if(rc<0) { report(rc); continue; }
        if(choice==0) break;
        if((rc=b_check_session(&actor))<0) { report(rc); break; }
        switch(choice) {
        case 1: rc=list_doctors(&actor); break;
        case 2: rc=list_visits(&actor); break;
        case 3: rc=list_records(&actor); break;
        case 4: rc=actor.role==ROLE_ADMIN ? edit_doctor(&actor,1) : ERR_NO_PERMISSION; break;
        case 5: rc=actor.role!=ROLE_PATIENT ? edit_doctor(&actor,0) : ERR_NO_PERMISSION; break;
        case 6: rc=actor.role==ROLE_ADMIN ? deactivate(&actor) : ERR_NO_PERMISSION; break;
        case 7: rc=actor.role==ROLE_DOCTOR ? pending(&actor) : ERR_NO_PERMISSION; break;
        case 8: rc=actor.role==ROLE_DOCTOR ? take_visit(&actor) : ERR_NO_PERMISSION; break;
        case 9: rc=actor.role==ROLE_DOCTOR ? edit_record(&actor,1) : ERR_NO_PERMISSION; break;
        case 10: rc=actor.role==ROLE_DOCTOR ? edit_record(&actor,0) : ERR_NO_PERMISSION; break;
        default: rc=ERR_PARAM;
        }
        report(rc);
        if(rc==B_ERR_INPUT) break;
    }
    return rc;
}
