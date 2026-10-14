#ifndef DB_H
#define DB_H

#include <mysql/mysql.h>

#define DB_OK             0
#define ERR_DB_CONN      -1   /* 连接失败 */
#define ERR_DB_EXEC      -2   /* SQL 执行失败 */
#define ERR_DB_PARAM     -3   /* 参数非法 */
#define ERR_DB_NOMEM     -4   /* 内存分配失败 */
#define ERR_DB_RESULT    -5   /* 结果集操作错误 */

/* ---------- 连接管理 ---------- */
int  db_connect(const char *host, const char *user,
                const char *pass, const char *dbname);
void db_close(void);
const char *db_error(void);        /* 取最近一次错误描述，用于打印 */

/* ---------- 执行：无结果集（INSERT/UPDATE/DELETE） ---------- */
int  db_execute(const char *sql);      /* 成功返回受影响行数(>=0)，失败返回错误码 */
int  db_last_insert_id(void);          /* 取最近一次 AUTO_INCREMENT 值 */

/* ---------- 执行：有结果集（SELECT） ---------- */
int  db_query(const char *sql);        /* 0 成功，失败返回错误码 */
int  db_row_count(void);               /* 结果集总行数 */
int  db_field_count(void);             /* 列数 */
int  db_fetch_row(void);               /* 取下一行：1=有数据 0=取完 负=错误 */
const char *db_get_field(int col);     /* 取当前行第 col 列（从 0 开始），NULL 表示 SQL NULL */
const char *db_get_field_by_name(const char *name);  /* 按列名取，更稳 */
void db_free_result(void);             /* 用完必须调用 */

/* ---------- 事务：接诊和开方都靠这三个 ---------- */
int  db_begin(void);
int  db_commit(void);
int  db_rollback(void);

#endif
