#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "menu.h"
#include "db.h"
#include "member_a_internal.h"
#ifdef _WIN32
#include <windows.h>
#endif

// 连接数据库并启动程序
int main(int argc, char **argv)
{
    const char *host=getenv("MEDICAL_DB_HOST"),*user=getenv("MEDICAL_DB_USER");
    const char *password=getenv("MEDICAL_DB_PASSWORD"),*name=getenv("MEDICAL_DB_NAME");
    const char *port_text=getenv("MEDICAL_DB_PORT");
    int port=3306,rc;
#ifdef _WIN32
    SetConsoleCP(CP_UTF8); SetConsoleOutputCP(CP_UTF8);
#endif
    if (argc>2 || (argc==2 && strcmp(argv[1],"--initialize-password")!=0)) {
        fputs("Usage: medical_system [--initialize-password]\n",stderr); return 2;
    }
    if (host == NULL) { host="127.0.0.1"; }
    if (name == NULL) { name="medical_system"; }
    if (user == NULL || user[0]=='\0' || password == NULL ||
        (port_text!=NULL && a_parse_int(port_text,1,65535,&port)!=ERR_OK)) {
        fputs("请设置 MEDICAL_DB_USER、MEDICAL_DB_PASSWORD；密码应由安全提示输入，不要写入源码。\n",stderr);
        return 2;
    }
    rc=db_connect_port(host,user,password,name,(unsigned int)port);
    if (rc!=ERR_OK) {
        fputs("数据库连接失败，请检查本机配置。\n",stderr); return 1;
    }
    rc=db_query("SELECT version FROM t_member_a_schema WHERE version=1");
    if (rc==ERR_OK && db_fetch_row()!=1) { rc=ERR_INVALID_STATE; }
    db_free_result();
    if (rc!=ERR_OK) {
        fputs("请先在目标库应用 sql/member_a_upgrade.sql；不要重新导入会清表的 init_db.sql。\n",stderr);
        db_close(); return 1;
    }
    if (argc==2) {
        rc=menu_initialize_password();
        rc=rc==AUTH_OK || rc==AUTH_ERR_NOT_LOGGED_IN ? 0 : 1;
    }
    else { rc=menu_application(); }
    db_close();
    return rc;
}
