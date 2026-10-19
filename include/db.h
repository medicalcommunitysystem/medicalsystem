#ifndef DB_H
#define DB_H

#include <mysql/mysql.h>
#include "common.h"


int  db_connect(const char *host, const char *user,
                const char *pass, const char *dbname);
void db_close(void);
const char *db_error(void);      

int  db_query(const char *sql);        
           
int db_execute(const char *sql);
int db_last_insert_id(void);

void db_free_result(void);
int db_query(const char *sql);
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
