#ifndef MEDICAL_RECORD_H
#define MEDICAL_RECORD_H
#include "member_b.h"
typedef struct {
    int record_id, visit_id;
    char patient_id[21], doctor_id[11];
    char allergy_history[801], past_history[801], family_history[801];
    char create_time[24], update_time[24];
} MedicalRecord;
/* 三项均需传字符串；过敏史空字符串表示无过敏，不允许 NULL 代替未填写。 */
typedef struct { const char *allergy_history, *past_history, *family_history; } MedicalRecordInput;
int medical_record_add(const UserSession *actor,int visit_id,const MedicalRecordInput *input,int *out_record_id);
int medical_record_update(const UserSession *actor,int record_id,const MedicalRecordInput *input);
int medical_record_query_by_id(const UserSession *actor,int record_id,MedicalRecord *out);
int medical_record_query_by_patient(const UserSession *actor,const char *patient_id,MedicalRecord *list,int capacity,int offset);
int medical_record_query_all(const UserSession *actor,MedicalRecord *list,int capacity,int offset);
#endif
