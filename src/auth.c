#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "auth.h"
#include "db.h"
#include "member_a_internal.h"

typedef struct {
    char user_id[21];
    char username[USERNAME_CAPACITY];
    char password_hash[AUTH_HASH_CAPACITY];
    int status;
    int failures;
    int locked;
    int expired;
} AuthAccount;

// 检查角色是否有效
static int valid_role(UserRole role)
{
    return role==ROLE_PATIENT || role==ROLE_DOCTOR || role==ROLE_ADMIN;
}

// 检查用户名格式和长度
static int valid_username(const char *username)
{
    size_t size;
    if (username == NULL || (size=strlen(username))>=USERNAME_CAPACITY) { return 0; }
    return a_text_valid(username,size+1,USERNAME_MAX_CHARS,1);
}

// 检查密码是否为空或过长
static int valid_password(const char *password)
{
    return password != NULL && password[0]!='\0' &&
        strlen(password)<=AUTH_PASSWORD_MAX_BYTES;
}

// 检查登录状态和角色权限
AuthResult auth_require_role(const UserSession *session, UserRole role)
{
    if (session == NULL || !valid_role(role)) { return AUTH_ERR_PARAM; }
    if (!session_is_logged_in(session)) { return AUTH_ERR_NOT_LOGGED_IN; }
    return session->role==role ? AUTH_OK : AUTH_ERR_FORBIDDEN;
}

// 检查当前账号是否可用
AuthResult auth_require_active(const UserSession *session)
{
    char role[2];
    int status,rc;
    const char *values[2];
    if (session == NULL) { return AUTH_ERR_PARAM; }
    if (!session_is_logged_in(session)) { return AUTH_ERR_NOT_LOGGED_IN; }
    if (!valid_role(session->role) ||
        !a_text_valid(session->user_id,sizeof session->user_id,20,1)) {
        return AUTH_ERR_PARAM;
    }
    role[0]=(char)('0'+session->role); role[1]='\0';
    values[0]=role; values[1]=session->user_id;
    rc=db_query_params("SELECT status FROM t_auth_account WHERE role=? AND user_id=?",values,2);
    if (rc!=ERR_OK) { return AUTH_ERR_DATABASE; }
    rc=db_fetch_row();
    if (rc!=1) { db_free_result(); return rc<0 ? AUTH_ERR_DATABASE : AUTH_ERR_FORBIDDEN; }
    rc=a_parse_int(db_get_field(0),0,1,&status);
    db_free_result();
    if (rc!=ERR_OK) { return AUTH_ERR_DATABASE; }
    return status==1 ? AUTH_OK : AUTH_ERR_DISABLED;
}

// 检查用户名是否已被占用
int auth_username_exists(const char *username)
{
    const char *values[]={username};
    int exists,rc;
    if (!valid_username(username)) { return ERR_PARAM; }
    rc=db_query_params("SELECT EXISTS(SELECT 1 FROM t_auth_account WHERE username=?)",values,1);
    if (rc!=ERR_OK) { return rc; }
    rc=db_fetch_row();
    if (rc!=1) { db_free_result(); return rc<0 ? rc : ERR_DB_RESULT; }
    rc=a_parse_int(db_get_field(0),0,1,&exists);
    db_free_result();
    return rc==ERR_OK ? exists : rc;
}

// 按角色读取账号并锁定记录
static AuthResult load_account(UserRole role, const char *username, AuthAccount *out)
{
    const char *sql;
    const char *values[]={username};
    int rc;
    switch (role) {
    case ROLE_PATIENT:
        sql="SELECT p.patient_id,p.username,p.password,a.status,a.failed_attempts,"
            "COALESCE(a.locked_until>NOW(),0),"
            "COALESCE(a.locked_until<=NOW(),0) "
            "FROM t_patient p JOIN t_auth_account a ON a.role=1 AND a.user_id=p.patient_id "
            "WHERE p.username=? FOR UPDATE"; break;
    case ROLE_DOCTOR:
        sql="SELECT d.doctor_id,d.username,d.password,a.status,a.failed_attempts,"
            "COALESCE(a.locked_until>NOW(),0),"
            "COALESCE(a.locked_until<=NOW(),0) "
            "FROM t_doctor d JOIN t_auth_account a ON a.role=2 AND a.user_id=d.doctor_id "
            "WHERE d.username=? FOR UPDATE"; break;
    case ROLE_ADMIN:
        sql="SELECT m.admin_id,m.username,m.password,a.status,a.failed_attempts,"
            "COALESCE(a.locked_until>NOW(),0),"
            "COALESCE(a.locked_until<=NOW(),0) "
            "FROM t_admin m JOIN t_auth_account a ON a.role=3 AND a.user_id=m.admin_id "
            "WHERE m.username=? FOR UPDATE"; break;
    default: return AUTH_ERR_PARAM;
    }
    rc=db_query_params(sql,values,1);
    if (rc!=ERR_OK) { return AUTH_ERR_DATABASE; }
    rc=db_fetch_row();
    if (rc!=1) {
        db_free_result();
        return rc<0 ? AUTH_ERR_DATABASE : AUTH_ERR_INVALID_CREDENTIALS;
    }
    rc=a_copy_text(out->user_id,sizeof out->user_id,db_get_field(0),1);
    if (rc==ERR_OK) { rc=a_copy_text(out->username,sizeof out->username,db_get_field(1),1); }
    if (rc==ERR_OK) { rc=a_copy_text(out->password_hash,sizeof out->password_hash,db_get_field(2),0); }
    if (rc==ERR_OK) { rc=a_parse_int(db_get_field(3),0,1,&out->status); }
    if (rc==ERR_OK) { rc=a_parse_int(db_get_field(4),0,INT_MAX,&out->failures); }
    if (rc==ERR_OK) { rc=a_parse_int(db_get_field(5),0,1,&out->locked); }
    if (rc==ERR_OK) { rc=a_parse_int(db_get_field(6),0,1,&out->expired); }
    db_free_result();
    return rc==ERR_OK ? AUTH_OK : AUTH_ERR_DATABASE;
}

// 提交或回滚登录相关事务
static AuthResult finish(AuthResult result, int commit)
{
    if (commit) {
        if (db_commit()!=ERR_OK) {
            db_rollback();
            return AUTH_ERR_DATABASE;
        }
    } else if (db_rollback()!=ERR_OK) {
        return AUTH_ERR_DATABASE;
    }
    return result;
}

// 清空密码错误次数和锁定时间
static AuthResult reset_failures(const char *username)
{
    const char *values[]={username};
    int rc=db_execute_params("UPDATE t_auth_account SET failed_attempts=0,"
                             "locked_until=NULL WHERE username=?",values,1);
    return rc<0 ? AUTH_ERR_DATABASE : AUTH_OK;
}

// 验证账号并保存登录会话
AuthResult auth_login(UserSession *session, UserRole role, const char *username, const char *password)
{
    AuthAccount account={0};
    UserSession authenticated;
    AuthResult result;
    int verified;
    if (session == NULL || !valid_role(role) || !valid_username(username) ||
        !valid_password(password) || session_is_logged_in(session)) { return AUTH_ERR_PARAM; }
    if (db_begin()!=ERR_OK) { return AUTH_ERR_DATABASE; }
    result=load_account(role,username,&account);
    if (result!=AUTH_OK) { goto rejected; }
    if (account.status!=1) { result=AUTH_ERR_DISABLED; goto rejected; }
    if (account.locked) { result=AUTH_ERR_LOCKED; goto rejected; }
    verified=verify_password(password,account.password_hash);
    if (verified<0) { result=AUTH_ERR_PASSWORD_SETUP; goto rejected; }
    if (verified==0) {
        const char *values[]={account.username};
        const char *sql;
        int failures=account.expired ? 0 : account.failures;
        if (failures>=4) {
            sql="UPDATE t_auth_account SET failed_attempts=5,"
                "locked_until=DATE_ADD(NOW(),INTERVAL 10 MINUTE) WHERE username=?";
            result=AUTH_ERR_LOCKED;
        } else {
            sql=account.expired
                ? "UPDATE t_auth_account SET failed_attempts=1,locked_until=NULL WHERE username=?"
                : "UPDATE t_auth_account SET failed_attempts=failed_attempts+1 WHERE username=?";
            result=AUTH_ERR_INVALID_CREDENTIALS;
        }
        if (db_execute_params(sql,values,1)!=1) { result=AUTH_ERR_DATABASE; goto rejected; }
        auth_secure_clear(&account,sizeof account);
        return finish(result,1);
    }
    result=reset_failures(account.username);
    if (result!=AUTH_OK) { goto rejected; }
    session_init(&authenticated);
    memcpy(authenticated.user_id,account.user_id,sizeof authenticated.user_id);
    memcpy(authenticated.username,account.username,sizeof authenticated.username);
    authenticated.role=role;
    result=finish(AUTH_OK,1);
    auth_secure_clear(&account,sizeof account);
    if (result!=AUTH_OK) { return result; }
    authenticated.logged_in=1;
    *session=authenticated;
    return AUTH_OK;
rejected:
    auth_secure_clear(&account,sizeof account);
    return finish(result,0);
}

// 按角色更新数据库中的密码哈希
static AuthResult update_password(UserRole role, const char *user_id, const char *hash)
{
    const char *sql;
    const char *values[]={hash,user_id};
    switch (role) {
    case ROLE_PATIENT: sql="UPDATE t_patient SET password=? WHERE patient_id=?"; break;
    case ROLE_DOCTOR: sql="UPDATE t_doctor SET password=? WHERE doctor_id=?"; break;
    case ROLE_ADMIN: sql="UPDATE t_admin SET password=? WHERE admin_id=?"; break;
    default: return AUTH_ERR_PARAM;
    }
    return db_execute_params(sql,values,2)==1 ? AUTH_OK : AUTH_ERR_DATABASE;
}

// 验证原密码后修改本人密码
AuthResult auth_change_password(const UserSession *session, const char *old_password, const char *new_password)
{
    AuthAccount account={0};
    char hash[AUTH_HASH_CAPACITY]={0};
    AuthResult result;
    if (!valid_password(old_password) || !valid_password(new_password)) { return AUTH_ERR_PARAM; }
    result=auth_require_active(session);
    if (result!=AUTH_OK) { return result; }
    if (db_begin()!=ERR_OK) { return AUTH_ERR_DATABASE; }
    result=load_account(session->role,session->username,&account);
    if (result==AUTH_OK && strcmp(account.user_id,session->user_id)!=0) { result=AUTH_ERR_FORBIDDEN; }
    if (result==AUTH_OK && account.status!=1) { result=AUTH_ERR_DISABLED; }
    if (result==AUTH_OK && account.locked) { result=AUTH_ERR_LOCKED; }
    if (result==AUTH_OK) {
        int verified=verify_password(old_password,account.password_hash);
        result=verified==1 ? AUTH_OK : (verified==0 ? AUTH_ERR_INVALID_CREDENTIALS : AUTH_ERR_PASSWORD_SETUP);
    }
    if (result==AUTH_OK && auth_hash_password(new_password,hash,sizeof hash)!=0) { result=AUTH_ERR_DATABASE; }
    if (result==AUTH_OK) { result=update_password(session->role,session->user_id,hash); }
    if (result==AUTH_OK) { result=reset_failures(account.username); }
    auth_secure_clear(hash,sizeof hash);
    auth_secure_clear(&account,sizeof account);
    return finish(result,result==AUTH_OK);
}

// 供数据库管理员初始化旧账号密码
AuthResult auth_initialize_password(UserRole role, const char *username, const char *new_password)
{
    AuthAccount account={0};
    char hash[AUTH_HASH_CAPACITY]={0};
    AuthResult result;
    if (!valid_role(role) || !valid_username(username) || !valid_password(new_password)) { return AUTH_ERR_PARAM; }
    if (db_begin()!=ERR_OK) { return AUTH_ERR_DATABASE; }
    result=load_account(role,username,&account);
    if (result==AUTH_OK && strncmp(account.password_hash,"pbkdf2-",7)==0) { result=AUTH_ERR_FORBIDDEN; }
    if (result==AUTH_OK && auth_hash_password(new_password,hash,sizeof hash)!=0) { result=AUTH_ERR_DATABASE; }
    if (result==AUTH_OK) { result=update_password(role,account.user_id,hash); }
    if (result==AUTH_OK) { result=reset_failures(account.username); }
    auth_secure_clear(hash,sizeof hash);
    auth_secure_clear(&account,sizeof account);
    return finish(result,result==AUTH_OK);
}
