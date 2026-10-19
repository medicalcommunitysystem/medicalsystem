#ifndef B_DB_H
#define B_DB_H
#include "db.h"
#include "member_b.h"

/* 新模块使用预处理语句；不改变队友 db_query/db_execute 的行为。
 * 与 db_begin/db_commit 共用 db.c 中的同一个连接。 */
typedef enum { BP_TEXT, BP_INT, BP_DOUBLE } BParamKind;
typedef struct { BParamKind kind; const char *text; long long number; double real; } BParam;
typedef struct { size_t count, columns; char ***row; } BRows;
static inline BParam bp_text(const char *s) { BParam p = {BP_TEXT,s,0,0}; return p; }
static inline BParam bp_int(long long n) { BParam p = {BP_INT,NULL,n,0}; return p; }
static inline BParam bp_double(double x) { BParam p = {BP_DOUBLE,NULL,0,x}; return p; }
int bdb_query(const char *sql, BParam *params, size_t n, BRows *out);
int bdb_exec(const char *sql, BParam *params, size_t n, long long *affected, int *insert_id);
void bdb_free(BRows *rows);
const char *bdb_error(void);
/* 成功提交；提交失败时尝试回滚。任何失败回滚都优先报告回滚本身失败。 */
int bdb_finish(int code);
#endif
