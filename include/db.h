#ifndef DB_H
#define DB_H

#include <mysql/mysql.h>
#include "common.h"


int  db_connect(const char *host, const char *user,
                const char *pass, const char *dbname);
void db_close(void);
const char *db_error(void);      

int  db_query(const char *sql);        
           

int  db_begin(void);
int  db_commit(void);
int  db_rollback(void);

#endif
