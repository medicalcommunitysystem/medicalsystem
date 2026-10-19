#ifndef PATIENT_H
#define PATIENT_H
#include "common.h"
#include "session.h"
typedef struct {
    char patient_id[21];
    char name[201];
    char gender[17];
    int age;
    char id_card[19];
    char phone[21];
    char address[801];
    char username[USERNAME_CAPACITY];
    char create_time[20];
    int status;
} Patient;

// 检查患者注册信息
int patient_validate_registration(const Patient *patient,const char *password);
// 注册患者并生成编号
int patient_register(Patient *patient, const char *password);
// 管理员新增患者
int patient_admin_add(const UserSession *session, Patient *patient, const char *password);
// 查询本人档案
int patient_get_my(const UserSession *session, Patient *out);
// 修改本人联系方式
int patient_update_my_contact(const UserSession *session, const char *phone, const char *address);
// 管理员按编号查询患者
int patient_admin_query(const UserSession *session, const char *patient_id, Patient *out);
// 管理员分页查询患者
int patient_query_all(const UserSession *session, Patient *list, int max, int offset);
// 管理员修改患者档案
int patient_admin_update(const UserSession *session, const Patient *patient);
// 管理员注销患者账号
int patient_deactivate(const UserSession *session, const char *patient_id);
// 管理员导出患者资料，不覆盖已有文件
int patient_export(const UserSession *session, const char *path, int *exported);
#endif
