#ifndef DOCTOR_H
#define DOCTOR_H
#include "member_b.h"

/* UTF-8 每字符最多四字节；数组长度按数据库字符上限预留。 */
typedef struct {
    char doctor_id[11], name[201], gender[17], title[81], dept[201], phone[81];
    int status;
} Doctor;
typedef struct {
    const char *name, *gender, *title, *dept, *phone, *username, *password;
} DoctorInput;

int doctor_add(const UserSession *actor, const DoctorInput *input, char out_id[11]);
int doctor_delete(const UserSession *actor, const char *doctor_id);
/* 管理员修改资料；医生仅可修改本人姓名、性别、电话，职称/科室必须保持原值。 */
int doctor_update(const UserSession *actor, const char *doctor_id, const DoctorInput *input);
int doctor_query_by_id(const UserSession *actor, const char *doctor_id, Doctor *out);
/* 返回记录数量或负数错误码。管理员可选全部/在职；患者只看在职；医生只看本人。 */
int doctor_query_all(const UserSession *actor, int active_only, Doctor *list, int capacity, int offset);
#endif
