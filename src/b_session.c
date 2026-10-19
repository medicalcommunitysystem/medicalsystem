#include "b_db.h"

int b_check_session(const UserSession *s)
{
    AuthResult rc=auth_require_active(s);
    if(rc==AUTH_OK) return ERR_OK;
    return rc==AUTH_ERR_DATABASE ? ERR_DB_EXEC : ERR_NO_PERMISSION;
}

int b_login(UserRole role,const char *username,const char *password,UserSession *out)
{
    AuthResult rc;
    if(!out) return ERR_PARAM;
    session_init(out);
    rc=auth_login(out,role,username,password);
    if(rc==AUTH_OK) return ERR_OK;
    if(rc==AUTH_ERR_INVALID_CREDENTIALS || rc==AUTH_ERR_PASSWORD_SETUP || rc==AUTH_ERR_DISABLED) return B_ERR_AUTH;
    return (int)rc;
}
