
#ifndef SESSION_H
#define SESSION_H
#include "common.h"
typedef enum
{
    ROLE_NONE = 0,
    ROLE_PATIENT,
    ROLE_DOCTOR,
    ROLE_ADMIN
} UserRole;
typedef struct
{
    int logged_in;
    UserRole role;
    char user_id[21];
    char username[USERNAME_CAPACITY];
} UserSession;

// 初始化会话
void session_init(UserSession *session);
// 退出登录并清空会话
void session_logout(UserSession *session);
// 判断是否已登录
int session_is_logged_in(const UserSession *session);

#endif
