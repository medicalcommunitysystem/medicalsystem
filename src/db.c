#include <stdio.h>
#include <string.h>
#include <mysql/mysql.h>
#include "db.h"
#include "common.h"

static MYSQL *g_conn = NULL;
static char g_errbuf[512];
static MYSQL_RES *g_res = NULL;
static MYSQL_ROW g_row = NULL;
static int g_errno = 0;

// 将错误信息存入g_errbuf
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

// 建立数据库连接
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
        g_errno = mysql_errno(g_conn); 
        mysql_close(g_conn);
        g_conn = NULL;
        return ERR_DB_CONN;
    }
    g_errno = 0;
    g_errbuf[0] = '\0';
    return ERR_OK;
}

// 关闭数据库连接
void db_close(void)
{
    if(g_conn != NULL){
        mysql_close(g_conn);
        g_conn = NULL;
    }
}

// 返回errbuf
const char *db_error(void)
{
    return g_errbuf;
}

// 执行增删改的sql语句
int db_execute(const char *sql)
{
    my_ulonglong affected;
    if (sql == NULL || sql[0] == '\0'){
        set_error("db_execute: SQL 语句为空");
        return ERR_PARAM;
    }
    
    if (g_conn == NULL){
        set_error("db_execute: 数据库未连接");
        return ERR_DB_CONN;
    }

    if (mysql_query(g_conn,sql)!=0){
        set_error(mysql_error(g_conn));
        g_errno = mysql_errno(g_conn); 
        return ERR_DB_EXEC;
    }

    affected = mysql_affected_rows(g_conn);

    if(affected == (my_ulonglong)-1){
        set_error(mysql_error(g_conn));
        return ERR_DB_EXEC;
    }
    g_errno = 0;
    g_errbuf[0] = '\0';
    return (int)affected;
}

// 获取insert语句生成的内容
int db_last_insert_id(void)
{
    my_ulonglong id;

    if (g_conn == NULL){
        set_error("db_last_insert_id: 数据库未连接");
        return ERR_DB_CONN;
    }

    id = mysql_insert_id(g_conn);
    g_errbuf[0] = '\0';
    return (int)id;
}

// 释放res
void db_free_result(void)
{
    if (g_res != NULL){
        mysql_free_result(g_res);
        g_res = NULL;
    }
    g_row = NULL;
}

// 执行查询类sql语句，结果存到res里面
int db_query(const char *sql)
{
    if(sql == NULL || sql[0] == '\0'){
        set_error("db_query: SQL 语句为空");
        return ERR_PARAM;
    }
    if (g_conn == NULL){
        set_error("db_query: 数据库未连接");
        return ERR_DB_CONN;
    }

    db_free_result();

    if (mysql_query(g_conn,sql) != 0){
        set_error(mysql_error(g_conn));
        g_errno = mysql_errno(g_conn); 
        return ERR_DB_EXEC;
    }
    g_res = mysql_store_result(g_conn);

    if(g_res == NULL){
        if (mysql_field_count(g_conn) > 0){
            set_error(mysql_error(g_conn));
            return ERR_DB_EXEC;
        }
        g_errbuf[0] = '\0';
        return ERR_OK;
    }
    g_row = NULL;
    g_errno = 0;
    g_errbuf[0] = '\0';
    return ERR_OK;
}

// 从res里面取一行存入row，有为1，无为0
int db_fetch_row(void)
{
    if (g_res == NULL){
        set_error("db_fetch_row: 没有活动");
        return ERR_DB_RESULT;
    }
    g_row = mysql_fetch_row(g_res);

    if(g_row == NULL){
        if (mysql_errno(g_conn)!=0){
            set_error(mysql_error(g_conn));
            return ERR_DB_RESULT;
        }
        return 0;
    }
    return 1;
}

// 根据col获取row的值
const char *db_get_field(int col)
{
    if(g_row == NULL){
        return NULL;
    }
    if(col<0||col >= (int)mysql_num_fields(g_res)){
        return NULL;
    }
    return g_row[col];
}

// 根据name找对应的字符串
const char *db_get_field_by_name(const char *name)
{
    MYSQL_FIELD *fields;
    unsigned int nfields;
    unsigned int i;
    if (g_row == NULL || name == NULL){
        return NULL;
    }

    fields = mysql_fetch_fields(g_res);
    nfields = mysql_num_fields(g_res);
    for (i = 0;i<nfields;i++){
        if (strcmp(fields[i].name,name) == 0){
            return g_row[i];
        }
    }
    return NULL;
}

// 返回res行数
int db_row_count(void)
{
    if(g_res == NULL){
        return 0;
    }
    return (int)mysql_num_rows(g_res);
}

// 返回res字数
int db_field_count(void)
{
    if(g_res == NULL){
        return 0;
    }
    return (int)mysql_num_fields(g_res);
}

// 开启数据库事务
int db_begin(void)
{
    if(g_conn == NULL){
        set_error("db_begin: 数据库未连接");
        return ERR_DB_CONN;
    }
    db_free_result();

    if(mysql_query(g_conn,"START TRANSACTION") != 0){
        g_errno = mysql_errno(g_conn); 
        set_error(mysql_error(g_conn));
        return ERR_DB_BEGIN;
    }
    g_errno = 0;
    g_errbuf[0] = '\0';
    return ERR_OK;
}

// 提交数据库事务
int db_commit(void)
{
    if (g_conn == NULL) {
        set_error("db_commit: 数据库未连接");
        return ERR_DB_CONN;
    }

    db_free_result();

    if (mysql_query(g_conn, "COMMIT") != 0) {
        g_errno = mysql_errno(g_conn); 
        set_error(mysql_error(g_conn));
        return ERR_DB_COMMIT;
    }
    g_errno = 0;
    g_errbuf[0] = '\0';
    return ERR_OK;
}

// 回滚数据库事务
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
    g_errno = 0;
    g_errbuf[0] = '\0';
    return ERR_OK;
}

int db_errno(void)
{
    return g_errno;
}