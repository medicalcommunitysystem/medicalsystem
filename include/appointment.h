#ifndef APPOINTMENT_H
#define APPOINTMENT_H
#include "session.h"

typedef struct {
    int appoint_id;
    char patient_id[21];
    char doctor_id[11];
    char appoint_date[11];
    char time_slot[41];
    char symptom[801];
    int status;
    char create_time[20];
} Appointment;

enum { APPOINTMENT_PENDING=0, APPOINTMENT_VISITED=1, APPOINTMENT_CANCELLED=2 };
typedef struct {
    char doctor_id[11];
    char name[201];
    char dept[201];
    char title[81];
} AppointmentDoctor;

// 查询本人预约首页
int appointment_query_my(const UserSession *session, Appointment *list, int max);
// 分页查询本人预约
int appointment_query_my_page(const UserSession *session, Appointment *list, int max, int offset);
// 取消本人待就诊预约
int appointment_cancel_my(const UserSession *session,int appoint_id);
// 为当前患者创建预约
int appointment_book(const UserSession *session, const char *doctor_id, const char *date, const char *time_slot, const char *symptom, int *out_appoint_id);
// 查询医生今日待就诊预约首页
int appointment_query_doctor_today(const UserSession *session, Appointment *list, int max);
// 分页查询医生今日待就诊预约
int appointment_query_doctor_today_page(const UserSession *session, Appointment *list, int max, int offset);
// 管理员分页查询全部预约
int appointment_query_all(const UserSession *session, Appointment *list, int max, int offset);
// 分页查询可预约医生
int appointment_query_available_doctors(const UserSession *session,AppointmentDoctor *list, int max, int offset);
#endif
