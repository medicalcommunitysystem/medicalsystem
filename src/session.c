#include <stddef.h>
#include <string.h>
#include "session.h"

// 初始化会话
void session_init(UserSession *session)
{
    if (session == NULL)
    {
        return;
    }

    memset(session, 0, sizeof *session);
}
// 判断是否已登录
int session_is_logged_in(const UserSession *session)
{
    if (session == NULL)
    {
        return 0;
    }

    return session->logged_in == 1;
}
// 退出登录并清空会话
void session_logout(UserSession *session)
{
    session_init(session);
}
