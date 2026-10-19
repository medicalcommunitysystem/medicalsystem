#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "visit.h"
#include "b_db.h"

static int valid_visit(const VisitInput *v)
{
    int systolic,diastolic,used=0;
    if(!v || !common_valid_text(v->chief_complaint,2000,1) ||
        !common_valid_text(v->present_illness,2000,0) || !common_valid_text(v->blood_pressure,10,1) ||
        !isfinite(v->temp) || v->temp<25 || v->temp>45 || v->heart_rate<1 || v->heart_rate>300) return 0;
    if(sscanf(v->blood_pressure,"%d/%d%n",&systolic,&diastolic,&used)!=2 || v->blood_pressure[used]) return 0;
    return systolic>=40 && systolic<=300 && diastolic>=20 && diastolic<=200 && systolic>diastolic;
}
int appointment_query_by_doctor_today(const UserSession *s, PendingAppointment *list, int cap, int offset)
{
    BRows r={0}; BParam p[3]; size_t i; int rc;
    if(!list || cap<1 || cap>B_PAGE_MAX || offset<0) return ERR_PARAM;
    if((rc=b_check_session(s))<0) return rc;
    if(s->role!=ROLE_DOCTOR) return ERR_NO_PERMISSION;
    p[0]=bp_text(s->user_id); p[1]=bp_int(cap); p[2]=bp_int(offset);
    rc=bdb_query("SELECT a.appoint_id,a.patient_id,p.name,a.appoint_date,a.time_slot,a.symptom "
        "FROM t_appointment a JOIN t_patient p ON p.patient_id=a.patient_id "
        "WHERE a.doctor_id=? AND a.appoint_date=CURDATE() AND a.status=0 AND p.status=1 "
        "ORDER BY a.appoint_id LIMIT ? OFFSET ?",p,3,&r);
    if(rc<0) return rc;
    for(i=0;i<r.count;i++) {
        PendingAppointment *d=&list[i]; char **a=r.row[i]; memset(d,0,sizeof(*d));
        d->appoint_id=atoi(a[0]); common_copy(d->patient_id,sizeof(d->patient_id),a[1]);
        common_copy(d->patient_name,sizeof(d->patient_name),a[2]);
        common_copy(d->appoint_date,sizeof(d->appoint_date),a[3]);
        common_copy(d->time_slot,sizeof(d->time_slot),a[4]); common_copy(d->symptom,sizeof(d->symptom),a[5]);
    }
    rc=(int)r.count; bdb_free(&r); return rc;
}
int visit_add(const UserSession *s, int aid, const VisitInput *v, int *out_id)
{
    BRows r={0}; BParam p[10]; int rc,new_id=0;
    long long affected=0;
    char patient_id[21];
    if(!out_id || aid<1 || !valid_visit(v)) return ERR_PARAM;
    *out_id=0;
    if((rc=b_check_session(s))<0) return rc;
    if(s->role!=ROLE_DOCTOR) return ERR_NO_PERMISSION;
    if((rc=db_begin())<0) return rc;
    p[0]=bp_int(aid);
    rc=bdb_query("SELECT a.patient_id,a.doctor_id,a.status,(a.appoint_date=CURDATE()),p.status "
        "FROM t_appointment a JOIN t_patient p ON p.patient_id=a.patient_id "
        "WHERE a.appoint_id=? FOR UPDATE",p,1,&r);
    if(rc<0) goto done;
    if(!r.count) { rc=ERR_NOT_FOUND; goto done; }
    if(strcmp(r.row[0][1],s->user_id)) { rc=ERR_NO_PERMISSION; goto done; }
    if(!r.row[0][2] || atoi(r.row[0][2])!=0 || !r.row[0][3] || atoi(r.row[0][3])!=1 ||
        !r.row[0][4] || atoi(r.row[0][4])!=1) { rc=ERR_INVALID_STATE; goto done; }
    common_copy(patient_id,sizeof(patient_id),r.row[0][0]); bdb_free(&r);
    p[0]=bp_text(patient_id); p[1]=bp_text(s->user_id); p[2]=bp_int(aid);
    p[3]=bp_text(v->chief_complaint); p[4]=bp_text(v->present_illness); p[5]=bp_double(v->temp);
    p[6]=bp_text(v->blood_pressure); p[7]=bp_int(v->heart_rate);
    rc=bdb_exec("INSERT INTO t_visit_record(patient_id,doctor_id,appoint_id,chief_complaint,present_illness,temp,blood_pressure,heart_rate) "
        "VALUES(?,?,?,?,?,?,?,?)",p,8,NULL,&new_id);
    if(rc<0) goto done;
    p[0]=bp_int(aid); p[1]=bp_text(s->user_id);
    rc=bdb_exec("UPDATE t_appointment SET status=1 WHERE appoint_id=? AND doctor_id=? AND status=0",p,2,&affected,NULL);
    if(rc==ERR_OK && affected!=1) rc=ERR_INVALID_STATE;
done:
    bdb_free(&r); rc=bdb_finish(rc);
    if(rc==ERR_OK) *out_id=new_id;
    return rc;
}
int appointment_take(const UserSession *s, int aid, const VisitInput *v, int *id)
{ return visit_add(s,aid,v,id); }

static void read_visit(char **r, VisitRecord *v)
{
    memset(v,0,sizeof(*v)); v->visit_id=atoi(r[0]);
    common_copy(v->patient_id,sizeof(v->patient_id),r[1]); common_copy(v->doctor_id,sizeof(v->doctor_id),r[2]);
    v->appoint_id=r[3] ? atoi(r[3]) : 0; common_copy(v->visit_time,sizeof(v->visit_time),r[4]);
    common_copy(v->chief_complaint,sizeof(v->chief_complaint),r[5]);
    common_copy(v->present_illness,sizeof(v->present_illness),r[6]); v->temp=r[7] ? atof(r[7]) : 0;
    common_copy(v->blood_pressure,sizeof(v->blood_pressure),r[8]); v->heart_rate=r[9] ? atoi(r[9]) : 0;
}
static int query_visits(const UserSession *s, int id, const char *patient, const char *doctor,
                        VisitRecord *list,int cap,int offset)
{
    BRows r={0}; BParam p[10]; size_t i; int rc;
    if(!list || cap<1 || cap>B_PAGE_MAX || offset<0) return ERR_PARAM;
    if((rc=b_check_session(s))<0) return rc;
    if(s->role==ROLE_PATIENT) {
        if(patient && strcmp(patient,s->user_id)) return ERR_NO_PERMISSION;
        patient=s->user_id;
    }
    if(s->role==ROLE_DOCTOR) {
        if(doctor && strcmp(doctor,s->user_id)) return ERR_NO_PERMISSION;
        doctor=s->user_id;
    }
    p[0]=bp_int(id); p[1]=bp_int(id); p[2]=bp_int(patient!=NULL); p[3]=bp_text(patient);
    p[4]=bp_int(doctor!=NULL); p[5]=bp_text(doctor); p[6]=bp_int(cap); p[7]=bp_int(offset);
    rc=bdb_query("SELECT visit_id,patient_id,doctor_id,appoint_id,visit_time,chief_complaint,present_illness,temp,blood_pressure,heart_rate "
        "FROM t_visit_record WHERE (?=0 OR visit_id=?) AND (?=0 OR patient_id=?) AND (?=0 OR doctor_id=?) "
        "ORDER BY visit_id DESC LIMIT ? OFFSET ?",p,8,&r);
    if(rc<0) return rc;
    for(i=0;i<r.count;i++) {
        if ((r.row[i][5] && strlen(r.row[i][5])>=sizeof(list[i].chief_complaint)) ||
            (r.row[i][6] && strlen(r.row[i][6])>=sizeof(list[i].present_illness))) {
            bdb_free(&r); return B_ERR_LIMIT;
        }
        read_visit(r.row[i],&list[i]);
    }
    rc=(int)r.count; bdb_free(&r); return rc;
}
int visit_query_by_id(const UserSession *s,int id,VisitRecord *out)
{
    int rc;
    if(id<1 || !out) return ERR_PARAM;
    memset(out,0,sizeof(*out)); rc=query_visits(s,id,NULL,NULL,out,1,0);
    return rc==1 ? ERR_OK : (rc==0 ? ERR_NOT_FOUND : rc);
}
int visit_query_by_patient(const UserSession *s,const char *id,VisitRecord *l,int n,int off)
{
    if(!common_valid_text(id,20,1)) return ERR_PARAM;
    return query_visits(s,0,id,NULL,l,n,off);
}
int visit_query_by_doctor(const UserSession *s,const char *id,VisitRecord *l,int n,int off)
{
    if(!common_valid_text(id,10,1)) return ERR_PARAM;
    if(s && s->role==ROLE_PATIENT) return ERR_NO_PERMISSION;
    return query_visits(s,0,NULL,id,l,n,off);
}
int visit_query_all(const UserSession *s,VisitRecord *l,int n,int off)
{ return query_visits(s,0,NULL,NULL,l,n,off); }
