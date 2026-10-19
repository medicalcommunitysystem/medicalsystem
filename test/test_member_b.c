/* 仅在专用测试库运行；禁止指向项目库。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "doctor.h"
#include "visit.h"
#include "medical_record.h"
#include "b_db.h"
#include "patient.h"
#include "appointment.h"
#include "prescription.h"

static int checks=0,failures=0;
#define CHECK(label, condition) do { ++checks; if(condition) printf("PASS %s\n",label); \
    else { ++failures; printf("FAIL %s (%s / %s)\n",label,bdb_error(),db_error()); } } while(0)
#define EXEC(sql) do { int e=db_execute(sql); if(e<0) { \
    fprintf(stderr,"Fixture SQL failed: %s\n%s\n",sql,db_error()); db_close(); return 2; } } while(0)
static int scalar(const char *sql)
{
    BRows r={0}; int value=-999;
    if(bdb_query(sql,NULL,0,&r)==ERR_OK && r.count && r.row[0][0]) value=atoi(r.row[0][0]);
    bdb_free(&r); return value;
}
static const char *env_or(const char *key,const char *fallback)
{ const char *s=getenv(key); return s ? s : fallback; }
int main(void)
{
    const char *dbname=getenv("B_DB_NAME");
    UserSession admin={0},doctor={0},other={0},patient={0},patient2={0},bad={0};
    DoctorInput d={"张'医生","男","主治医师","全科","13800000001","b_doctor_1","Case123"};
    Doctor doctors[10],found;
    VisitInput vi={"头痛 ' OR 1=1 --","一天，无其他不适","120/80",36.5,75};
    MedicalRecordInput mi={"","无","无"};
    PendingAppointment pending[10];
    VisitRecord visit;
    MedicalRecord record;
    char did[11],second[11],unused[11],unicode_name[201];
    int rc,vid=0,rid=0,out=0,i;
    if(!dbname || strncmp(dbname,"member_b_test_",14) || strcmp(env_or("B_TEST_ALLOW_WRITE",""),"YES")) {
        fprintf(stderr,"Refusing: use a fresh member_b_test_* database and B_TEST_ALLOW_WRITE=YES.\n"); return 2;
    }
    rc=db_connect(env_or("B_DB_HOST","localhost"),env_or("B_DB_USER","root"),env_or("B_DB_PASSWORD",""),dbname);
    if(rc<0) { fprintf(stderr,"Connect: %s\n",db_error()); return 2; }
    if(scalar("SELECT COUNT(*) FROM t_doctor")!=0 || scalar("SELECT COUNT(*) FROM t_patient")!=0) {
        fprintf(stderr,"Refusing nonempty test fixtures. Recreate only your disposable test database.\n"); db_close(); return 2;
    }
    CHECK("UTF8 Chinese accepted",common_valid_text("张医生",3,1));
    CHECK("UTF8 char limit",!common_valid_text("张医生",2,1));
    CHECK("invalid UTF8 rejected",!common_valid_text("\xc0\x80",20,1));
    CHECK("required empty rejected",!common_valid_text("",20,1));
    CHECK("optional empty accepted",common_valid_text("",20,0));
    CHECK("null rejected",!common_valid_text(NULL,20,0));
    CHECK("initialize legacy admin",auth_initialize_password(ROLE_ADMIN,"admin","123456")==AUTH_OK);
    CHECK("admin auth",b_login(ROLE_ADMIN,"admin","123456",&admin)==ERR_OK);
    CHECK("bad auth",b_login(ROLE_ADMIN,"admin","bad",&bad)==B_ERR_AUTH);
    CHECK("login injection rejected",b_login(ROLE_ADMIN,"' OR 1=1 --","123456",&bad)==B_ERR_AUTH);
    CHECK("doctor auto id",doctor_add(&admin,&d,did)==ERR_OK && !strcmp(did,"D001"));
    CHECK("doctor duplicate username",doctor_add(&admin,&d,unused)==ERR_DB_DUP);
    d.username="admin";
    CHECK("cross admin username duplicate",doctor_add(&admin,&d,unused)==ERR_DB_DUP);
    d.username="b_doctor_2";
    CHECK("doctor second id",doctor_add(&admin,&d,second)==ERR_OK && !strcmp(second,"D002"));
    CHECK("doctor login",b_login(ROLE_DOCTOR,"b_doctor_1","Case123",&doctor)==ERR_OK);
    CHECK("case-sensitive password",b_login(ROLE_DOCTOR,"b_doctor_1","case123",&bad)==B_ERR_AUTH);
    CHECK("second doctor login",b_login(ROLE_DOCTOR,"b_doctor_2","Case123",&other)==ERR_OK);
    EXEC("INSERT INTO t_patient(patient_id,name,id_card,username,password,status) VALUES"
        "('PTEST1','测试患者','111111111111111111','b_patient_1','Patient123',1),"
        "('PTEST2','另一个患者','222222222222222222','b_patient_2','Patient123',1),('PTEST3','回滚测试患者','333333333333333333','b_patient_3','Patient123',1)");
    CHECK("initialize patient1",auth_initialize_password(ROLE_PATIENT,"b_patient_1","Patient123")==AUTH_OK);
    CHECK("initialize patient2",auth_initialize_password(ROLE_PATIENT,"b_patient_2","Patient123")==AUTH_OK);
    CHECK("patient login",b_login(ROLE_PATIENT,"b_patient_1","Patient123",&patient)==ERR_OK);
    CHECK("patient2 login",b_login(ROLE_PATIENT,"b_patient_2","Patient123",&patient2)==ERR_OK);
    d.username="b_patient_1";
    CHECK("cross patient username duplicate",doctor_add(&admin,&d,unused)==ERR_DB_DUP);
    CHECK("patient cannot add doctor",doctor_add(&patient,&d,unused)==ERR_NO_PERMISSION);
    CHECK("admin lists doctors",doctor_query_all(&admin,0,doctors,10,0)==2);
    CHECK("page limit",doctor_query_all(&admin,0,doctors,101,0)==ERR_PARAM);
    CHECK("pagination",doctor_query_all(&admin,0,doctors,1,1)==1 && !strcmp(doctors[0].doctor_id,"D002"));
    CHECK("quoted name round trip",doctor_query_by_id(&admin,did,&found)==ERR_OK && !strcmp(found.name,"张'医生"));
    CHECK("doctor sees only self",doctor_query_all(&doctor,0,doctors,10,0)==1 && !strcmp(doctors[0].doctor_id,did));
    CHECK("doctor cannot see peer detail",doctor_query_by_id(&doctor,second,&found)==ERR_NO_PERMISSION);
    d.username=NULL; d.password=NULL; d.phone="10086";
    CHECK("self profile update",doctor_update(&doctor,did,&d)==ERR_OK);
    d.title="主任";
    CHECK("self title privilege blocked",doctor_update(&doctor,did,&d)==ERR_NO_PERMISSION);
    CHECK("admin title update",doctor_update(&admin,did,&d)==ERR_OK);
    CHECK("same-value update succeeds",doctor_update(&admin,did,&d)==ERR_OK);
    CHECK("patient cannot update doctor",doctor_update(&patient,did,&d)==ERR_NO_PERMISSION);
    CHECK("doctor cannot disable",doctor_delete(&doctor,second)==ERR_NO_PERMISSION);
    CHECK("admin disable",doctor_delete(&admin,second)==ERR_OK);
    CHECK("disable idempotent",doctor_delete(&admin,second)==ERR_OK);
    CHECK("inactive login denied",b_login(ROLE_DOCTOR,"b_doctor_2","Case123",&bad)==B_ERR_AUTH);
    CHECK("stale inactive session denied",b_check_session(&other)==ERR_NO_PERMISSION);
    CHECK("patient active list only",doctor_query_all(&patient,0,doctors,10,0)==1);
    CHECK("patient inactive detail hidden",doctor_query_by_id(&patient,second,&found)==ERR_NOT_FOUND);
    EXEC("UPDATE t_doctor SET status=1,gender=NULL,phone=NULL WHERE doctor_id='D002'");
    CHECK("null fields safe",doctor_query_by_id(&admin,second,&found)==ERR_OK && !found.phone[0] && !found.gender[0]);
    for(i=0;i<50;i++) memcpy(unicode_name+i*3,"张",3);
    unicode_name[150]='\0'; d.name=unicode_name; d.title="主治医师";
    CHECK("50 Chinese characters accepted",doctor_update(&admin,second,&d)==ERR_OK);
    CHECK("Chinese roundtrip no byte truncation",doctor_query_by_id(&admin,second,&found)==ERR_OK && !strcmp(found.name,unicode_name));
    EXEC("INSERT INTO t_appointment(appoint_id,patient_id,doctor_id,appoint_date,time_slot,status) VALUES"
        "(101,'PTEST1','D001',CURDATE(),'上午',0),"
        "(102,'PTEST1','D002',CURDATE(),'上午',0),"
        "(103,'PTEST1','D001',DATE_ADD(CURDATE(),INTERVAL 1 DAY),'上午',0),"
        "(104,'PTEST1','D001',CURDATE(),'下午',2),"
        "(105,'PTEST3','D001',CURDATE(),'下午',0),"
        "(106,'PTEST2','D002',CURDATE(),'上午',0),"
        "(107,'PTEST2','D001',CURDATE(),'下午',0)");
    CHECK("pending doctor today only",appointment_query_by_doctor_today(&doctor,pending,10,0)==3);
    CHECK("patient cannot list pending",appointment_query_by_doctor_today(&patient,pending,10,0)==ERR_NO_PERMISSION);
    CHECK("patient cannot take visit",visit_add(&patient,101,&vi,&out)==ERR_NO_PERMISSION);
    CHECK("wrong doctor appointment",visit_add(&doctor,102,&vi,&out)==ERR_NO_PERMISSION);
    CHECK("future appointment rejected",visit_add(&doctor,103,&vi,&out)==ERR_INVALID_STATE);
    CHECK("cancelled appointment rejected",visit_add(&doctor,104,&vi,&out)==ERR_INVALID_STATE);
    CHECK("missing appointment",visit_add(&doctor,99999,&vi,&out)==ERR_NOT_FOUND);
    vi.blood_pressure="oops";
    CHECK("invalid vitals rejected",visit_add(&doctor,101,&vi,&out)==ERR_PARAM);
    vi.blood_pressure="120/80";
    CHECK("atomic visit success",visit_add(&doctor,101,&vi,&vid)==ERR_OK && vid>0);
    CHECK("appointment state committed",scalar("SELECT status FROM t_appointment WHERE appoint_id=101")==1);
    CHECK("repeated take rejected",appointment_take(&doctor,101,&vi,&out)==ERR_INVALID_STATE);
    CHECK("one visit per appointment",scalar("SELECT COUNT(*) FROM t_visit_record WHERE appoint_id=101")==1);
    CHECK("patient owns visit",visit_query_by_id(&patient,vid,&visit)==ERR_OK && !strcmp(visit.patient_id,"PTEST1"));
    CHECK("chief complaint preserved",!strcmp(visit.chief_complaint,vi.chief_complaint));
    CHECK("other patient cannot read",visit_query_by_id(&patient2,vid,&visit)==ERR_NOT_FOUND);
    CHECK("other doctor cannot read",visit_query_by_id(&other,vid,&visit)==ERR_NOT_FOUND);
    CHECK("patient cannot query someone else",visit_query_by_patient(&patient,"PTEST2",&visit,1,0)==ERR_NO_PERMISSION);
    CHECK("patient cannot query doctor list",visit_query_by_doctor(&patient,"D001",&visit,1,0)==ERR_NO_PERMISSION);
    EXEC("CREATE TRIGGER b_fail_update BEFORE UPDATE ON t_appointment FOR EACH ROW "
        "BEGIN IF OLD.appoint_id=105 THEN SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='forced rollback test'; END IF; END");
    CHECK("forced appointment write failure",visit_add(&doctor,105,&vi,&out)<0);
    CHECK("failed visit insert rolled back",scalar("SELECT COUNT(*) FROM t_visit_record WHERE appoint_id=105")==0);
    CHECK("failed appointment state unchanged",scalar("SELECT status FROM t_appointment WHERE appoint_id=105")==0);
    EXEC("DROP TRIGGER b_fail_update");
    CHECK("retry after rollback succeeds",visit_add(&doctor,105,&vi,&out)==ERR_OK);
    CHECK("nested transaction preserved",db_begin()==ERR_OK && visit_add(&doctor,107,&vi,&out)==ERR_DB_BEGIN);
    CHECK("outer rollback",db_rollback()==ERR_OK);
    CHECK("other patient fixture visit",visit_add(&other,106,&vi,&out)==ERR_OK);
    CHECK("admin sees all visits",visit_query_all(&admin,&visit,1,0)==1);
    CHECK("patient list filtered",visit_query_all(&patient2,&visit,1,0)==1 && !strcmp(visit.patient_id,"PTEST2"));
    CHECK("wrong doctor cannot write medical record",medical_record_add(&other,vid,&mi,&out)==ERR_NO_PERMISSION);
    CHECK("patient cannot write medical record",medical_record_add(&patient,vid,&mi,&out)==ERR_NO_PERMISSION);
    CHECK("missing visit record",medical_record_add(&doctor,99999,&mi,&out)==ERR_NOT_FOUND);
    CHECK("empty allergy accepted",medical_record_add(&doctor,vid,&mi,&rid)==ERR_OK && rid>0);
    CHECK("one medical record per visit",medical_record_add(&doctor,vid,&mi,&out)==ERR_DB_DUP);
    CHECK("medical record visible to owner",medical_record_query_by_id(&patient,rid,&record)==ERR_OK && !record.allergy_history[0]);
    CHECK("medical record hidden from other patient",medical_record_query_by_id(&patient2,rid,&record)==ERR_NOT_FOUND);
    CHECK("medical record hidden from other doctor",medical_record_query_by_id(&other,rid,&record)==ERR_NOT_FOUND);
    CHECK("medical query self restriction",medical_record_query_by_patient(&patient,"PTEST2",&record,1,0)==ERR_NO_PERMISSION);
    mi.allergy_history="青霉素 ' test";
    CHECK("author can update within24h",medical_record_update(&doctor,rid,&mi)==ERR_OK);
    CHECK("same content update succeeds",medical_record_update(&doctor,rid,&mi)==ERR_OK);
    CHECK("admin read only",medical_record_update(&admin,rid,&mi)==ERR_NO_PERMISSION);
    CHECK("other doctor cannot modify",medical_record_update(&other,rid,&mi)==ERR_NO_PERMISSION);
    EXEC("UPDATE t_medical_record SET create_time=DATE_SUB(NOW(),INTERVAL 25 HOUR),update_time=NOW()");
    CHECK("recent update does not reset24h",medical_record_update(&doctor,rid,&mi)==B_ERR_EXPIRED);
    EXEC("UPDATE t_medical_record SET create_time=DATE_SUB(NOW(),INTERVAL 24 HOUR)");
    CHECK("exact24h boundary locked",medical_record_update(&doctor,rid,&mi)==B_ERR_EXPIRED);
    EXEC("UPDATE t_medical_record SET create_time=DATE_ADD(NOW(),INTERVAL 1 HOUR)");
    CHECK("future creation time locked",medical_record_update(&doctor,rid,&mi)==B_ERR_EXPIRED);
    mi.allergy_history=NULL;
    CHECK("null allergy rejected",medical_record_update(&doctor,rid,&mi)==ERR_PARAM);
    EXEC("UPDATE t_patient SET status=0 WHERE patient_id='PTEST2'");
    CHECK("inactive patient rejected",b_check_session(&patient2)==ERR_NO_PERMISSION);
    CHECK("inactive patient cannot visit",visit_add(&doctor,107,&vi,&out)==ERR_INVALID_STATE);
    CHECK("inactive patient login rejected",b_login(ROLE_PATIENT,"b_patient_2","Patient123",&bad)==B_ERR_AUTH);
    EXEC("INSERT INTO t_doctor(doctor_id,name,username,password,status) VALUES('D999','max','b_max','x',1)");
    d.username="b_overflow"; d.password="x"; d.name="overflow";
    CHECK("D999 exhaustion rejected",doctor_add(&admin,&d,unused)==B_ERR_LIMIT);
    CHECK("overflow did not insert",scalar("SELECT COUNT(*) FROM t_doctor WHERE username='b_overflow'")==0);
    EXEC("DELETE FROM t_doctor WHERE doctor_id='D999'");
    CHECK("new doctor password hashed",scalar("SELECT password LIKE 'pbkdf2-sha256$%' FROM t_doctor WHERE doctor_id='D001'")==1);
    {
        UserSession lock_session={0};
        DoctorInput lock_input={"Lock Test","","","","","b_lock_doctor","LockPass"};
        CHECK("create doctor for lock test",doctor_add(&admin,&lock_input,unused)==ERR_OK);
        for(i=0;i<4;i++) CHECK("wrong password counted by A",b_login(ROLE_DOCTOR,"b_lock_doctor","wrong",&lock_session)==B_ERR_AUTH);
        CHECK("fifth wrong password locks",b_login(ROLE_DOCTOR,"b_lock_doctor","wrong",&lock_session)==AUTH_ERR_LOCKED);
        CHECK("correct password blocked while locked",b_login(ROLE_DOCTOR,"b_lock_doctor","LockPass",&lock_session)==AUTH_ERR_LOCKED);
    }
    {
        Patient p={0}; UserSession ps={0}; BRows dates={0};
        char today[11]={0}; int aid=0,v=0,m=0,prescription=0,stock_before;
        PrescDetail item={0}; Prescription prescriptions[4];
        MedicalRecordInput history={"","无","无"};
        common_copy(p.name,sizeof(p.name),"全流程测试患者");
        common_copy(p.gender,sizeof(p.gender),"男"); p.age=30;
        common_copy(p.id_card,sizeof(p.id_card),"444444444444444444");
        common_copy(p.username,sizeof(p.username),"b_full_patient");
        CHECK("A register patient",patient_register(&p,"FlowPass")==ERR_OK);
        CHECK("A auth registered patient",auth_login(&ps,ROLE_PATIENT,"b_full_patient","FlowPass")==AUTH_OK);
        CHECK("database date available",bdb_query("SELECT DATE_FORMAT(CURDATE(),'%Y-%m-%d')",NULL,0,&dates)==ERR_OK && dates.count==1);
        if(dates.count) common_copy(today,sizeof(today),dates.row[0][0]);
        bdb_free(&dates);
        CHECK("A books B doctor",appointment_book(&ps,did,today,"上午","头痛",&aid)==ERR_OK);
        CHECK("B takes A appointment",visit_add(&doctor,aid,&vi,&v)==ERR_OK);
        stock_before=scalar("SELECT stock FROM t_medicine WHERE med_id='M001'");
        common_copy(item.med_id,sizeof(item.med_id),"M001"); item.quantity=2;
        common_copy(item.dosage,sizeof(item.dosage),"测试用法");
        CHECK("C prescribes B visit",presc_add(v,p.patient_id,did,"测试诊断",&item,1,&prescription)==ERR_OK);
        CHECK("C stock deducted",scalar("SELECT stock FROM t_medicine WHERE med_id='M001'")==stock_before-2);
        CHECK("B writes medical record",medical_record_add(&doctor,v,&history,&m)==ERR_OK);
        CHECK("patient sees own visit",visit_query_by_id(&ps,v,&visit)==ERR_OK);
        CHECK("patient sees own medical record",medical_record_query_by_id(&ps,m,&record)==ERR_OK);
        CHECK("patient prescription query",presc_query_by_patient(p.patient_id,prescriptions,4)==1 && prescriptions[0].presc_id==prescription);
    }
    db_close();
    printf("RESULT: %d checks, %d failures\n",checks,failures);
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
