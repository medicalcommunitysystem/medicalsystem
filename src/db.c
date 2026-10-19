#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
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
static MYSQL_STMT *g_stmt = NULL;
static MYSQL_BIND *g_stmt_bind = NULL;
static char **g_stmt_row = NULL;
static unsigned long *g_stmt_lengths = NULL;
static bool *g_stmt_nulls = NULL;
static bool *g_stmt_errors = NULL;

static void set_error(const char *msg)
{
    snprintf(g_errbuf, sizeof(g_errbuf), "%s",
             (msg != NULL) ? msg : "(unknown error)");
}


int db_connect(const char *host, const char *user,
               const char *pass, const char *dbname)
{
    return db_connect_port(host, user, pass, dbname, 0);
}

int db_connect_port(const char *host, const char *user, const char *pass,
                    const char *dbname, unsigned int port)
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

    if (port > 65535 ||
        mysql_real_connect(g_conn, host, user, pass, dbname, port, NULL, 0) == NULL) {
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
    if (g_stmt_bind != NULL) {
        for (unsigned int i = 0; i < g_fields_n; ++i) {
            free(g_stmt_bind[i].buffer);
        }
    }
    free(g_stmt_bind);
    free(g_stmt_row);
    free(g_stmt_lengths);
    free(g_stmt_nulls);
    free(g_stmt_errors);
    g_stmt_bind = NULL;
    g_stmt_row = NULL;
    g_stmt_lengths = NULL;
    g_stmt_nulls = NULL;
    g_stmt_errors = NULL;
    if (g_stmt != NULL) {
        mysql_stmt_close(g_stmt);
        g_stmt = NULL;
    }
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

    if (g_stmt != NULL) {
        int rc = mysql_stmt_fetch(g_stmt);
        g_row = NULL;
        if (rc == MYSQL_NO_DATA) {
            return 0;
        }
        if (rc != 0) {
            set_error("prepared result fetch failed");
            g_errno = mysql_stmt_errno(g_stmt);
            return ERR_DB_RESULT;
        }
        for (unsigned int i = 0; i < g_fields_n; ++i) {
            if (g_stmt_nulls[i]) {
                g_stmt_row[i] = NULL;
            } else {
                if (g_stmt_errors[i] ||
                    g_stmt_lengths[i] >= g_stmt_bind[i].buffer_length) {
                    set_error("prepared result was truncated");
                    return ERR_DB_RESULT;
                }
                g_stmt_row[i] = g_stmt_bind[i].buffer;
                g_stmt_row[i][g_stmt_lengths[i]] = '\0';
            }
        }
        g_row = g_stmt_row;
        return 1;
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
    my_ulonglong count = g_stmt != NULL
        ? mysql_stmt_num_rows(g_stmt) : mysql_num_rows(g_res);
    return count > INT_MAX ? ERR_DB_RESULT : (int)count;
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

static int db_prepare(const char *sql, const char *const *values, size_t count)
{
    MYSQL_BIND *bindings = NULL;
    int result = ERR_DB_EXEC;

    if (sql == NULL || sql[0] == '\0' || (count != 0 && values == NULL)) {
        return ERR_PARAM;
    }
    if (g_conn == NULL) {
        return ERR_DB_CONN;
    }
    db_free_result();
    g_stmt = mysql_stmt_init(g_conn);
    if (g_stmt == NULL) {
        return ERR_NOMEM;
    }
    if (mysql_stmt_prepare(g_stmt, sql, (unsigned long)strlen(sql)) != 0) {
        goto failed;
    }
    if (mysql_stmt_param_count(g_stmt) != count) {
        result = ERR_PARAM;
        goto failed;
    }
    if (count != 0) {
        bindings = calloc(count, sizeof *bindings);
        if (bindings == NULL) {
            result = ERR_NOMEM;
            goto failed;
        }
        for (size_t i = 0; i < count; ++i) {
            bindings[i].buffer_type = values[i] == NULL
                ? MYSQL_TYPE_NULL : MYSQL_TYPE_STRING;
            bindings[i].buffer = (void *)values[i];
            bindings[i].buffer_length = values[i] == NULL
                ? 0 : (unsigned long)strlen(values[i]);
        }
        if (mysql_stmt_bind_param(g_stmt, bindings) != 0) {
            goto failed;
        }
    }
    if (mysql_stmt_execute(g_stmt) != 0) {
        goto failed;
    }
    free(bindings);
    g_errno = 0;
    g_errbuf[0] = '\0';
    return ERR_OK;

failed:
    g_errno = mysql_stmt_errno(g_stmt);
    // 错误信息不包含参数或密码
    set_error("prepared statement failed");
    free(bindings);
    db_free_result();
    return result;
}

int db_execute_params(const char *sql, const char *const *values, size_t count)
{
    int rc = db_prepare(sql, values, count);
    my_ulonglong affected;
    if (rc != ERR_OK) {
        return rc;
    }
    if (mysql_stmt_field_count(g_stmt) != 0) {
        db_free_result();
        return ERR_PARAM;
    }
    affected = mysql_stmt_affected_rows(g_stmt);
    db_free_result();
    if (affected == (my_ulonglong)-1 || affected > INT_MAX) {
        return ERR_DB_RESULT;
    }
    return (int)affected;
}

int db_query_params(const char *sql, const char *const *values, size_t count)
{
    int rc = db_prepare(sql, values, count);
    bool update_max_length = true;
    if (rc != ERR_OK) {
        return rc;
    }
    if (mysql_stmt_attr_set(g_stmt, STMT_ATTR_UPDATE_MAX_LENGTH,
                            &update_max_length) != 0 ||
        mysql_stmt_store_result(g_stmt) != 0) {
        goto failed;
    }
    g_res = mysql_stmt_result_metadata(g_stmt);
    if (g_res == NULL) {
        goto failed;
    }
    g_fields = mysql_fetch_fields(g_res);
    g_fields_n = mysql_num_fields(g_res);
    g_stmt_bind = calloc(g_fields_n, sizeof *g_stmt_bind);
    g_stmt_row = calloc(g_fields_n, sizeof *g_stmt_row);
    g_stmt_lengths = calloc(g_fields_n, sizeof *g_stmt_lengths);
    g_stmt_nulls = calloc(g_fields_n, sizeof *g_stmt_nulls);
    g_stmt_errors = calloc(g_fields_n, sizeof *g_stmt_errors);
    if (g_stmt_bind == NULL || g_stmt_row == NULL ||
        g_stmt_lengths == NULL || g_stmt_nulls == NULL || g_stmt_errors == NULL) {
        db_free_result();
        return ERR_NOMEM;
    }
    for (unsigned int i = 0; i < g_fields_n; ++i) {
        unsigned long size = g_fields[i].max_length + 1;
        if (size == 0 || size > 1024 * 1024) {
            goto failed;
        }
        g_stmt_bind[i].buffer_type = MYSQL_TYPE_STRING;
        g_stmt_bind[i].buffer = calloc(size, 1);
        g_stmt_bind[i].buffer_length = size;
        g_stmt_bind[i].length = &g_stmt_lengths[i];
        g_stmt_bind[i].is_null = &g_stmt_nulls[i];
        g_stmt_bind[i].error = &g_stmt_errors[i];
        if (g_stmt_bind[i].buffer == NULL) {
            db_free_result();
            return ERR_NOMEM;
        }
    }
    if (mysql_stmt_bind_result(g_stmt, g_stmt_bind) != 0) {
        goto failed;
    }
    return ERR_OK;

failed:
    g_errno = mysql_stmt_errno(g_stmt);
    set_error("prepared result setup failed");
    db_free_result();
    return ERR_DB_RESULT;
}
