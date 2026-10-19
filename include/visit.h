#ifndef VISIT_H
#define VISIT_H
#include "member_b.h"
typedef struct {
    int visit_id, appoint_id, heart_rate;
    char patient_id[21], doctor_id[11], visit_time[24];
    char chief_complaint[8001], present_illness[8001], blood_pressure[41];
    double temp;
} VisitRecord;
typedef struct {
    int appoint_id;
    char patient_id[21], patient_name[201], appoint_date[11], time_slot[41], symptom[801];
} PendingAppointment;
typedef struct {
    const char *chief_complaint, *present_illness, *blood_pressure;
    double temp;
    int heart_rate;
} VisitInput;
int appointment_query_by_doctor_today(const UserSession *actor, PendingAppointment *list, int capacity, int offset);
int visit_add(const UserSession *actor, int appoint_id, const VisitInput *input, int *out_visit_id);
/* 接诊唯一实现；不能先独立将预约标记为已就诊。 */
int appointment_take(const UserSession *actor, int appoint_id, const VisitInput *input, int *out_visit_id);
int visit_query_by_id(const UserSession *actor, int visit_id, VisitRecord *out);
/* 管理员查任意患者，医生仅查自己接诊的记录，患者仅查本人。 */
int visit_query_by_patient(const UserSession *actor, const char *patient_id, VisitRecord *list, int capacity, int offset);
int visit_query_by_doctor(const UserSession *actor, const char *doctor_id, VisitRecord *list, int capacity, int offset);
int visit_query_all(const UserSession *actor, VisitRecord *list, int capacity, int offset);
#endif
