#ifndef MEMBER_B_H
#define MEMBER_B_H

#include <stddef.h>
#include "common.h"
#include "auth.h"

/* B 模块错误码，保留队友已有 -1..-15 的数值。 */
#define B_ERR_LIMIT      -200
#define B_ERR_EXPIRED    -201
#define B_ERR_AUTH       -202
#define B_ERR_INPUT      -203
#define B_PAGE_MAX       100



/* 复用成员 A 的登录和会话；业务入口再次校验账号状态。 */
int b_login(UserRole role, const char *username, const char *password, UserSession *out);
int b_check_session(const UserSession *session);
const char *b_error_message(int code);

/* 所有字符串长度参数均含结尾 '\0'；输入超长拒绝，绝不静默截断。 */
int common_read_line(const char *prompt, char *out, size_t capacity);
int common_read_int(const char *prompt, int min, int max, int *out);
int common_read_double(const char *prompt, double min, double max, double *out);
int common_valid_text(const char *text, size_t max_chars, int required);
void common_copy(char *out, size_t capacity, const char *text);


/* 由 A 的角色菜单调用，不重新登录。 */
int menu_member_b(const UserSession *session);

#endif
