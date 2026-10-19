#ifndef AUTH_H
#define AUTH_H
#include <stddef.h>
#include "session.h"
#define AUTH_PASSWORD_MAX_BYTES 128
#define AUTH_HASH_CAPACITY 100

// 登录结果
typedef enum
{
    AUTH_OK = 0,
    AUTH_ERR_PARAM = -100,  //参数不合法
    AUTH_ERR_INVALID_CREDENTIALS = -101,  //账号或密码错误
    AUTH_ERR_DISABLED = -102, //账号被停用
    AUTH_ERR_LOCKED = -103, //账号被锁定
    AUTH_ERR_DATABASE = -104 ,//数据库错误
    AUTH_ERR_NOT_LOGGED_IN = -105,//未登录
    AUTH_ERR_FORBIDDEN = -106,//权限不足
    AUTH_ERR_PASSWORD_SETUP = -107
} AuthResult;

// 验证账号并保存登录会话
AuthResult auth_login(UserSession *session, UserRole role, const char *username, const char *password);
// 生成加盐密码哈希
int auth_hash_password(const char *password, char *output, size_t output_size);
// 验证密码是否匹配
int verify_password(const char *password, const char *stored_hash);
// 检查登录角色
AuthResult auth_require_role(const UserSession *session, UserRole required_role);
// 检查当前账号是否可用
AuthResult auth_require_active(const UserSession *session);
// 验证原密码后修改本人密码
AuthResult auth_change_password(const UserSession *session,const char *old_password, const char *new_password);
// 供数据库管理员初始化旧账号密码
AuthResult auth_initialize_password(UserRole role, const char *username,const char *new_password);
// 检查用户名是否已被占用
int auth_username_exists(const char *username);
// 清除内存中的敏感数据
void auth_secure_clear(void *memory, size_t size);
#endif
