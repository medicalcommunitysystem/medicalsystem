#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "doctor.h"
#include "b_db.h"

static int valid_profile(const DoctorInput *d)
{
    return d && common_valid_text(d->name,50,1) && common_valid_text(d->gender,4,0) &&
        common_valid_text(d->title,20,0) && common_valid_text(d->dept,50,0) &&
        common_valid_text(d->phone,20,0);
}
static void read_doctor(char **r, Doctor *d)
{
    memset(d,0,sizeof(*d));
    common_copy(d->doctor_id,sizeof(d->doctor_id),r[0]);
    common_copy(d->name,sizeof(d->name),r[1]); common_copy(d->gender,sizeof(d->gender),r[2]);
    common_copy(d->title,sizeof(d->title),r[3]); common_copy(d->dept,sizeof(d->dept),r[4]);
    common_copy(d->phone,sizeof(d->phone),r[5]); d->status=r[6] ? atoi(r[6]) : 0;
}
int doctor_query_all(const UserSession *s, int active, Doctor *list, int cap, int offset)
{
    BRows r={0}; BParam p[5]; size_t i; int rc;
    if (!list || cap<1 || cap>B_PAGE_MAX || offset<0 || (active!=0 && active!=1)) return ERR_PARAM;
    if ((rc=b_check_session(s))<0) return rc;
    p[0]=bp_int(active || s->role==ROLE_PATIENT); p[1]=bp_int(s->role==ROLE_DOCTOR);
    p[2]=bp_text(s->user_id); p[3]=bp_int(cap); p[4]=bp_int(offset);
    rc=bdb_query("SELECT doctor_id,name,gender,title,dept,phone,status FROM t_doctor "
        "WHERE (?=0 OR status=1) AND (?=0 OR doctor_id=?) ORDER BY doctor_id LIMIT ? OFFSET ?",p,5,&r);
    if (rc<0) return rc;
    for(i=0;i<r.count;i++) read_doctor(r.row[i],&list[i]);
    rc=(int)r.count; bdb_free(&r); return rc;
}
int doctor_query_by_id(const UserSession *s, const char *id, Doctor *out)
{
    BRows r={0}; BParam p; int rc;
    if (!out || !common_valid_text(id,10,1)) return ERR_PARAM;
    memset(out,0,sizeof(*out));
    if ((rc=b_check_session(s))<0) return rc;
    if(s->role==ROLE_DOCTOR && strcmp(s->user_id,id)) return ERR_NO_PERMISSION;
    p=bp_text(id);
    rc=bdb_query("SELECT doctor_id,name,gender,title,dept,phone,status FROM t_doctor WHERE doctor_id=?",&p,1,&r);
    if(rc<0) return rc;
    if(!r.count) rc=ERR_NOT_FOUND;
    else if(s->role==ROLE_PATIENT && (!r.row[0][6] || atoi(r.row[0][6])!=1)) rc=ERR_NOT_FOUND;
    else { read_doctor(r.row[0],out); rc=ERR_OK; }
    bdb_free(&r); return rc;
}
int doctor_add(const UserSession *s, const DoctorInput *d, char out_id[11])
{
    BRows r={0}; BParam p[9]; int rc,number; char id[11];
    char hash[AUTH_HASH_CAPACITY]={0};
    if (!out_id || !valid_profile(d) || !common_valid_text(d->username,50,1) ||
        !d->password || !d->password[0] || strlen(d->password)>AUTH_PASSWORD_MAX_BYTES) return ERR_PARAM;
    out_id[0]='\0';
    if((rc=b_check_session(s))<0) return rc;
    if(s->role!=ROLE_ADMIN) return ERR_NO_PERMISSION;
    if((rc=db_begin())<0) return rc;
    if(auth_hash_password(d->password,hash,sizeof(hash))!=0) { rc=ERR_PARAM; goto done; }
    /* 序号行串行分配 D 编号；跨角色唯一由 A 的账号触发器兜底。 */
    rc=bdb_query("SELECT next_value FROM t_b_sequence WHERE name='doctor' FOR UPDATE",NULL,0,&r);
    if(rc<0) goto done;
    if(r.count!=1) { rc=ERR_INVALID_STATE; goto done; }
    bdb_free(&r);
    p[0]=bp_text(d->username); p[1]=p[0]; p[2]=p[0];
    rc=bdb_query("SELECT username FROM t_admin WHERE username=? UNION ALL "
        "SELECT username FROM t_doctor WHERE username=? UNION ALL "
        "SELECT username FROM t_patient WHERE username=?",p,3,&r);
    if(rc<0) goto done;
    if(r.count) { rc=ERR_DB_DUP; goto done; }
    bdb_free(&r);
    rc=bdb_exec("INSERT INTO t_b_sequence(name,next_value) "
        "SELECT 'doctor',COALESCE(MAX(CAST(SUBSTRING(doctor_id,2) AS UNSIGNED)),0)+1 FROM t_doctor "
        "WHERE doctor_id REGEXP '^D[0-9]+$' ON DUPLICATE KEY UPDATE next_value=GREATEST(next_value,VALUES(next_value))",
        NULL,0,NULL,NULL);
    if(rc<0) goto done;
    rc=bdb_query("SELECT next_value FROM t_b_sequence WHERE name='doctor' FOR UPDATE",NULL,0,&r);
    if(rc<0) goto done;
    if(r.count!=1) { rc=ERR_INVALID_STATE; goto done; }
    number=atoi(r.row[0][0]);
    if(number<1 || number>999) { rc=B_ERR_LIMIT; goto done; }
    snprintf(id,sizeof(id),"D%03d",number); bdb_free(&r);
    p[0]=bp_text(id); p[1]=bp_text(d->name); p[2]=bp_text(d->gender);
    p[3]=bp_text(d->title); p[4]=bp_text(d->dept); p[5]=bp_text(d->phone);
    p[6]=bp_text(d->username); p[7]=bp_text(hash);
    rc=bdb_exec("INSERT INTO t_doctor(doctor_id,name,gender,title,dept,phone,username,password,status) "
        "VALUES(?,?,?,?,?,?,?,?,1)",p,8,NULL,NULL);
    if(rc<0) goto done;
    rc=bdb_exec("UPDATE t_b_sequence SET next_value=next_value+1 WHERE name='doctor'",NULL,0,NULL,NULL);
done:
    auth_secure_clear(hash,sizeof(hash));
    bdb_free(&r); rc=bdb_finish(rc);
    if(rc==ERR_OK) common_copy(out_id,11,id);
    return rc;
}
int doctor_delete(const UserSession *s, const char *id)
{
    BRows r={0}; BParam p; int rc;
    if(!common_valid_text(id,10,1)) return ERR_PARAM;
    if((rc=b_check_session(s))<0) return rc;
    if(s->role!=ROLE_ADMIN) return ERR_NO_PERMISSION;
    if((rc=db_begin())<0) return rc;
    p=bp_text(id);
    rc=bdb_query("SELECT status FROM t_doctor WHERE doctor_id=? FOR UPDATE",&p,1,&r);
    if(rc>=0) {
        if(!r.count) rc=ERR_NOT_FOUND;
        else rc=bdb_exec("UPDATE t_doctor SET status=0 WHERE doctor_id=?",&p,1,NULL,NULL);
    }
    bdb_free(&r); return bdb_finish(rc);
}
int doctor_update(const UserSession *s, const char *id, const DoctorInput *d)
{
    BRows r={0}; BParam p[7]; int rc;
    if(!common_valid_text(id,10,1) || !valid_profile(d)) return ERR_PARAM;
    if((rc=b_check_session(s))<0) return rc;
    if(s->role!=ROLE_ADMIN && !(s->role==ROLE_DOCTOR && !strcmp(s->user_id,id))) return ERR_NO_PERMISSION;
    if((rc=db_begin())<0) return rc;
    p[0]=bp_text(id);
    rc=bdb_query("SELECT title,dept FROM t_doctor WHERE doctor_id=? FOR UPDATE",p,1,&r);
    if(rc<0) goto done;
    if(!r.count) { rc=ERR_NOT_FOUND; goto done; }
    if(s->role==ROLE_DOCTOR && (strcmp(d->title,r.row[0][0] ? r.row[0][0] : "") ||
        strcmp(d->dept,r.row[0][1] ? r.row[0][1] : ""))) { rc=ERR_NO_PERMISSION; goto done; }
    p[0]=bp_text(d->name); p[1]=bp_text(d->gender); p[2]=bp_text(d->title);
    p[3]=bp_text(d->dept); p[4]=bp_text(d->phone); p[5]=bp_text(id);
    rc=bdb_exec("UPDATE t_doctor SET name=?,gender=?,title=?,dept=?,phone=? WHERE doctor_id=?",p,6,NULL,NULL);
done:
    bdb_free(&r); return bdb_finish(rc);
}
