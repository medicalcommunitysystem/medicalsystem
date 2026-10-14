#include <stdio.h>
#include <string.h>
#include <mysql/mysql.h>
#include "db.h"
#include "common.h"

static MYSQL *g_conn = NULL;
static char g_errbuf[512];

static void set_error(const char *msg)
{
    const char *str;
    if(msg != NULL){
        str = msg;
    }else{
        str = "(unknown error)";
    }
    snprintf(g_errbuf,sizeof(g_errbuf),"%s",str);
}

int db_connect(const char *host,const char *user,const char *pass,const char *dbname)
{
    if(host == NULL || user == NULL || pass == NULL || dbname == NULL || 
        host[0] == '\0'|| user[0] == '\0' || dbname[0] == '\0'){
            set_error("db_connect: 参数错误");
            return ERR_PARAM;
        }

    if (g_conn != NULL) {
        db_close();
    }

    g_conn = mysql_init(NULL);
    if(g_conn == NULL) {
        set_error("db_connect: mysql_init 失败");
        return ERR_DB_CONN;
    }

    if (mysql_options(g_conn,MYSQL_SET_CHARSET_NAME,"utf8mb4") != 0){
        set_error("db_connect: 设置 utf8mb4 失败");
        mysql_close(g_conn);
        g_conn = NULL;
        return ERR_DB_CONN;
    }

    if (mysql_real_connect(g_conn,host,user,pass,dbname,0,NULL,0) == NULL){
        set_error(mysql_error(g_conn));
        mysql_close(g_conn);
        g_conn = NULL;
        return ERR_DB_CONN;
    }

    if (mysql_query(g_conn,"SET NAMES utf8mb4") != 0){
        set_error(mysql_error(g_conn));
        mysql_close(g_conn);
        g_conn = NULL;
        return ERR_DB_CONN;
    }

    g_errbuf[0] = '\0';
    return ERR_OK;
}

void db_close(void)
{
    if(g_conn != NULL){
        mysql_close(g_conn);
        g_conn = NULL;
    }
}

const char *db_error(void)
{
    return g_errbuf;
}