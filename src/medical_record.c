#include <stdlib.h>
#include <string.h>
#include "medical_record.h"
#include "b_db.h"

static int valid_input(const MedicalRecordInput *m)
{
    return m && common_valid_text(m->allergy_history,200,0) &&
        common_valid_text(m->past_history,200,0) && common_valid_text(m->family_history,200,0);
}
int medical_record_add(const UserSession *s,int vid,const MedicalRecordInput *m,int *out_id)
{
    BRows r={0}; BParam p[6]; int rc,id=0;
    char patient[21];
    if(vid<1 || !out_id || !valid_input(m)) return ERR_PARAM;
    *out_id=0;
    if((rc=b_check_session(s))<0) return rc;
    if(s->role!=ROLE_DOCTOR) return ERR_NO_PERMISSION;
    if((rc=db_begin())<0) return rc;
    p[0]=bp_int(vid);
    rc=bdb_query("SELECT patient_id,doctor_id FROM t_visit_record WHERE visit_id=? FOR UPDATE",p,1,&r);
    if(rc<0) goto done;
    if(!r.count) { rc=ERR_NOT_FOUND; goto done; }
    if(!r.row[0][1] || strcmp(s->user_id,r.row[0][1])) { rc=ERR_NO_PERMISSION; goto done; }
    common_copy(patient,sizeof(patient),r.row[0][0]);
    if(!patient[0]) { rc=ERR_INVALID_STATE; goto done; }
    bdb_free(&r);
    p[0]=bp_text(patient); p[1]=bp_text(s->user_id); p[2]=bp_int(vid);
    p[3]=bp_text(m->allergy_history); p[4]=bp_text(m->past_history); p[5]=bp_text(m->family_history);
    rc=bdb_exec("INSERT INTO t_medical_record(patient_id,doctor_id,visit_id,allergy_history,past_history,family_history,create_time) "
        "VALUES(?,?,?,?,?,?,NOW())",p,6,NULL,&id);
done:
    bdb_free(&r); rc=bdb_finish(rc);
    if(rc==ERR_OK) *out_id=id;
    return rc;
}
int medical_record_update(const UserSession *s,int rid,const MedicalRecordInput *m)
{
    BRows r={0}; BParam p[6]; int rc;
    long long affected;
    if(rid<1 || !valid_input(m)) return ERR_PARAM;
    if((rc=b_check_session(s))<0) return rc;
    if(s->role!=ROLE_DOCTOR) return ERR_NO_PERMISSION;
    if((rc=db_begin())<0) return rc;
    p[0]=bp_int(rid);
    rc=bdb_query("SELECT doctor_id,(create_time<=NOW() AND NOW()<DATE_ADD(create_time,INTERVAL 24 HOUR)) "
        "FROM t_medical_record WHERE record_id=? FOR UPDATE",p,1,&r);
    if(rc<0) goto done;
    if(!r.count) { rc=ERR_NOT_FOUND; goto done; }
    if(!r.row[0][0] || strcmp(s->user_id,r.row[0][0])) { rc=ERR_NO_PERMISSION; goto done; }
    if(!r.row[0][1] || atoi(r.row[0][1])!=1) { rc=B_ERR_EXPIRED; goto done; }
    bdb_free(&r);
    p[0]=bp_text(m->allergy_history); p[1]=bp_text(m->past_history); p[2]=bp_text(m->family_history);
    p[3]=bp_int(rid); p[4]=bp_text(s->user_id);
    rc=bdb_exec("UPDATE t_medical_record SET allergy_history=?,past_history=?,family_history=?,update_time=NOW() "
        "WHERE record_id=? AND doctor_id=? AND create_time<=NOW() AND NOW()<DATE_ADD(create_time,INTERVAL 24 HOUR)",
        p,5,&affected,NULL);
    /* affected=0 也可能是内容和秒级更新时间都未变化，因此重新确认时间资格。 */
    if(rc==ERR_OK && affected==0) {
        p[0]=bp_int(rid);
        rc=bdb_query("SELECT record_id FROM t_medical_record WHERE record_id=? "
            "AND create_time<=NOW() AND NOW()<DATE_ADD(create_time,INTERVAL 24 HOUR)",p,1,&r);
        if(rc==ERR_OK && !r.count) rc=B_ERR_EXPIRED;
    }
done:
    bdb_free(&r); return bdb_finish(rc);
}
static void read_record(char **r,MedicalRecord *m)
{
    memset(m,0,sizeof(*m)); m->record_id=atoi(r[0]); m->visit_id=r[3] ? atoi(r[3]) : 0;
    common_copy(m->patient_id,sizeof(m->patient_id),r[1]); common_copy(m->doctor_id,sizeof(m->doctor_id),r[2]);
    common_copy(m->allergy_history,sizeof(m->allergy_history),r[4]);
    common_copy(m->past_history,sizeof(m->past_history),r[5]); common_copy(m->family_history,sizeof(m->family_history),r[6]);
    common_copy(m->create_time,sizeof(m->create_time),r[7]); common_copy(m->update_time,sizeof(m->update_time),r[8]);
}
static int query_records(const UserSession *s,int id,const char *patient,MedicalRecord *list,int cap,int off)
{
    BRows r={0}; BParam p[8]; size_t i; int rc;
    if(!list || cap<1 || cap>B_PAGE_MAX || off<0) return ERR_PARAM;
    if((rc=b_check_session(s))<0) return rc;
    if(s->role==ROLE_PATIENT) {
        if(patient && strcmp(s->user_id,patient)) return ERR_NO_PERMISSION;
        patient=s->user_id;
    }
    p[0]=bp_int(id); p[1]=bp_int(id); p[2]=bp_int(patient!=NULL); p[3]=bp_text(patient);
    p[4]=bp_int(s->role==ROLE_DOCTOR); p[5]=bp_text(s->user_id); p[6]=bp_int(cap); p[7]=bp_int(off);
    rc=bdb_query("SELECT record_id,patient_id,doctor_id,visit_id,allergy_history,past_history,family_history,create_time,update_time "
        "FROM t_medical_record WHERE (?=0 OR record_id=?) AND (?=0 OR patient_id=?) AND (?=0 OR doctor_id=?) "
        "ORDER BY record_id DESC LIMIT ? OFFSET ?",p,8,&r);
    if(rc<0) return rc;
    for(i=0;i<r.count;i++) read_record(r.row[i],&list[i]);
    rc=(int)r.count; bdb_free(&r); return rc;
}
int medical_record_query_by_id(const UserSession *s,int id,MedicalRecord *out)
{
    int rc;
    if(id<1 || !out) return ERR_PARAM;
    memset(out,0,sizeof(*out)); rc=query_records(s,id,NULL,out,1,0);
    return rc==1 ? ERR_OK : (rc==0 ? ERR_NOT_FOUND : rc);
}
int medical_record_query_by_patient(const UserSession *s,const char *id,MedicalRecord *l,int n,int off)
{
    if(!common_valid_text(id,20,1)) return ERR_PARAM;
    return query_records(s,0,id,l,n,off);
}
int medical_record_query_all(const UserSession *s,MedicalRecord *l,int n,int off)
{ return query_records(s,0,NULL,l,n,off); }
