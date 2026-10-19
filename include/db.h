#ifndef DB_H
#define DB_H

#include <stddef.h>
#include <mysql/mysql.h>
#include "common.h"


int  db_connect(const char *host, const char *user,
                const char *pass, const char *dbname);
// 按指定端口连接数据库
int db_connect_port(const char *host, const char *user, const char *pass, const char *dbname, unsigned int port);
void db_close(void);
const char *db_error(void);            
           
int db_execute(const char *sql);
int db_last_insert_id(void);

void db_free_result(void);
int db_query(const char *sql);
// 绑定参数后执行查询
int db_query_params(const char *sql, const char *const *values, size_t count);
// 绑定参数后执行写操作
int db_execute_params(const char *sql, const char *const *values, size_t count);
int db_fetch_row(void);
const char *db_get_field(int col);
const char *db_get_field_by_name(const char *name);
int db_row_count(void);
int db_field_count(void);

int  db_begin(void);
int  db_commit(void);
int  db_rollback(void);
int db_errno(void);

#endif
