#include <stdio.h>
#include <string.h>
#include <mysql/mysql.h>
#include "db.h"
#include "common.h"

static MYSQL       *g_conn   = NULL;
static char         g_errbuf[512];
static MYSQL_RES   *g_res    = NULL;
static MYSQL_ROW    g_row    = NULL;
static MYSQL_FIELD *g_fields = NULL;   
static unsigned int g_fields_n = 0;
static int          g_errno  = 0;
static int          g_in_trans = 0;   

static void set_error(const char *msg)
{
    snprintf(g_errbuf, sizeof(g_errbuf), "%s",
             (msg != NULL) ? msg : "(unknown error)");
}


int db_connect(const char *host, const char *user,
               const char *pass, const char *dbname)
{
    if (host == NULL || user == NULL || pass == NULL || dbname == NULL ||
        host[0] == '\0' || user[0] == '\0' || dbname[0] == '\0') {
        set_error("db_connect: 参数错误");
        return ERR_PARAM;
    }

    if (g_conn != NULL) {
        db_close();
    }

    g_conn = mysql_init(NULL);
    if (g_conn == NULL) {
        set_error("db_connect: mysql_init 失败");
        return ERR_DB_CONN;
    }

    if (mysql_options(g_conn, MYSQL_SET_CHARSET_NAME, "utf8mb4") != 0) {
        set_error("db_connect: 设置 utf8mb4 失败");
        mysql_close(g_conn);
        g_conn = NULL;
        return ERR_DB_CONN;
    }

    if (mysql_real_connect(g_conn, host, user, pass, dbname, 0, NULL, 0) == NULL) {
        set_error(mysql_error(g_conn));
        mysql_close(g_conn);
        g_conn = NULL;
        return ERR_DB_CONN;
    }

    if (mysql_query(g_conn, "SET NAMES utf8mb4") != 0) {
        set_error(mysql_error(g_conn));
        g_errno = mysql_errno(g_conn);
        mysql_close(g_conn);
        g_conn = NULL;
        return ERR_DB_CONN;
    }

    g_errno = 0;
    g_errbuf[0] = '\0';
    return ERR_OK;
}

void db_close(void)
{
    if (g_conn != NULL) {
        db_free_result();     
        mysql_close(g_conn);
        g_conn = NULL;
    }
    g_in_trans = 0;
}

const char *db_error(void)
{
    return g_errbuf;
}


int db_execute(const char *sql)
{
    my_ulonglong affected;

    if (sql == NULL || sql[0] == '\0') {
        set_error("db_execute: SQL 语句为空");
        return ERR_PARAM;
    }
    if (g_conn == NULL) {
        set_error("db_execute: 数据库未连接");
        return ERR_DB_CONN;
    }

    db_free_result();         

    if (mysql_query(g_conn, sql) != 0) {
        set_error(mysql_error(g_conn));
        g_errno = mysql_errno(g_conn);
        return ERR_DB_EXEC;
    }

    affected = mysql_affected_rows(g_conn);
    if (affected == (my_ulonglong)-1) {
        set_error(mysql_error(g_conn));
        g_errno = mysql_errno(g_conn);
        return ERR_DB_EXEC;
    }

    g_errno = 0;
    g_errbuf[0] = '\0';
    return (int)affected;
}

int db_last_insert_id(void)
{
    if (g_conn == NULL) {
        set_error("db_last_insert_id: 数据库未连接");
        return ERR_DB_CONN;
    }
    g_errbuf[0] = '\0';
    return (int)mysql_insert_id(g_conn);
}


void db_free_result(void)
{
    if (g_res != NULL) {
        mysql_free_result(g_res);
        g_res = NULL;
    }
    g_fields   = NULL;
    g_fields_n = 0;
    g_row      = NULL;
}

int db_query(const char *sql)
{
    if (sql == NULL || sql[0] == '\0') {
        set_error("db_query: SQL 语句为空");
        return ERR_PARAM;
    }
    if (g_conn == NULL) {
        set_error("db_query: 数据库未连接");
        return ERR_DB_CONN;
    }

    db_free_result();

    if (mysql_query(g_conn, sql) != 0) {
        set_error(mysql_error(g_conn));
        g_errno = mysql_errno(g_conn);
        return ERR_DB_EXEC;
    }

    g_res = mysql_store_result(g_conn);
    if (g_res == NULL) {
        if (mysql_field_count(g_conn) > 0) {
            set_error(mysql_error(g_conn));
            g_errno = mysql_errno(g_conn);
            return ERR_DB_EXEC;
        }
        g_errno = 0;
        g_errbuf[0] = '\0';
        return ERR_OK;
    }

    g_row      = NULL;
    g_fields   = mysql_fetch_fields(g_res);
    g_fields_n = mysql_num_fields(g_res);
    g_errno = 0;
    g_errbuf[0] = '\0';
    return ERR_OK;
}

int db_fetch_row(void)
{
    if (g_res == NULL) {
        set_error("db_fetch_row: 没有活动的结果集");
        return ERR_DB_RESULT;
    }

    g_row = mysql_fetch_row(g_res);
    if (g_row == NULL) {
        if (mysql_errno(g_conn) != 0) {
            set_error(mysql_error(g_conn));
            g_errno = mysql_errno(g_conn);
            return ERR_DB_RESULT;
        }
        return 0;             
    }
    return 1;
}

const char *db_get_field(int col)
{
    if (g_row == NULL || g_res == NULL) {
        return NULL;
    }
    if (col < 0 || col >= (int)g_fields_n) {
        return NULL;
    }
    return g_row[col];
}

const char *db_get_field_by_name(const char *name)
{
    unsigned int i;

    if (g_row == NULL || g_res == NULL || name == NULL) {
        return NULL;
    }

    for (i = 0; i < g_fields_n; i++) {
        if (strcmp(g_fields[i].name, name) == 0) {
            return g_row[i];
        }
    }
    return NULL;
}

int db_row_count(void)
{
    if (g_res == NULL) {
        return 0;
    }
    return (int)mysql_num_rows(g_res);
}

int db_field_count(void)
{
    return (int)g_fields_n;
}



int db_begin(void)
{
    if (g_conn == NULL) {
        set_error("db_begin: 数据库未连接");
        return ERR_DB_CONN;
    }
    if (g_in_trans) {
        set_error("db_begin: 已在事务中，禁止嵌套（START TRANSACTION 会隐式提交外层事务）");
        return ERR_DB_BEGIN;
    }

    db_free_result();

    if (mysql_query(g_conn, "START TRANSACTION") != 0) {
        set_error(mysql_error(g_conn));
        g_errno = mysql_errno(g_conn);
        return ERR_DB_BEGIN;
    }

    g_in_trans = 1;
    g_errno = 0;
    g_errbuf[0] = '\0';
    return ERR_OK;
}

int db_commit(void)
{
    if (g_conn == NULL) {
        set_error("db_commit: 数据库未连接");
        return ERR_DB_CONN;
    }

    db_free_result();

    if (mysql_query(g_conn, "COMMIT") != 0) {
        set_error(mysql_error(g_conn));
        g_errno = mysql_errno(g_conn);
        return ERR_DB_COMMIT;
    }

    g_in_trans = 0;
    g_errno = 0;
    g_errbuf[0] = '\0';
    return ERR_OK;
}

int db_rollback(void)
{
    if (g_conn == NULL) {
        set_error("db_rollback: 数据库未连接");
        return ERR_DB_CONN;
    }

    db_free_result();

    if (mysql_query(g_conn, "ROLLBACK") != 0) {
        set_error(mysql_error(g_conn));
        g_errno = mysql_errno(g_conn);
        return ERR_DB_ROLLBACK;
    }

    g_in_trans = 0;
    g_errno = 0;
    g_errbuf[0] = '\0';
    return ERR_OK;
}

int db_errno(void)
{
    return g_errno;
}