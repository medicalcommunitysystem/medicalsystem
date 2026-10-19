#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "b_db.h"

/* 适配 A 的参数绑定接口，不取得底层连接、不另建结果集或事务连接。 */
static char last_error[512];
const char *bdb_error(void) { return last_error; }
static int map_error(int rc)
{
    if(rc>=0) { last_error[0]='\0'; return rc; }
    snprintf(last_error,sizeof(last_error),"%s",db_error());
    if(db_errno()==MYSQL_ER_DUP_ENTRY) return ERR_DB_DUP;
    if(db_errno()==MYSQL_ER_NO_REFERENCED) return ERR_NOT_FOUND;
    return rc;
}
static int parameters(const char *sql,BParam *p,size_t *n,char query[4096],
                      const char *values[32],char numbers[32][80])
{
    const char *suffix=" LIMIT ? OFFSET ?";
    size_t i,len;
    if(!sql || *n>32 || (*n && !p)) return ERR_PARAM;
    len=strlen(sql);
    if(len>=4096) return B_ERR_LIMIT;
    memcpy(query,sql,len+1);
    /* MySQL 的 LIMIT 不能绑定文本；只格式化验证过的整数，用户文本始终绑定。 */
    if(len>=strlen(suffix) && !strcmp(sql+len-strlen(suffix),suffix)) {
        if(*n<2 || p[*n-2].kind!=BP_INT || p[*n-1].kind!=BP_INT ||
            p[*n-2].number<1 || p[*n-2].number>B_PAGE_MAX || p[*n-1].number<0) return ERR_PARAM;
        snprintf(query+len-strlen(suffix),4096-len+strlen(suffix)," LIMIT %lld OFFSET %lld",
            p[*n-2].number,p[*n-1].number);
        *n-=2;
    }
    for(i=0;i<*n;i++) {
        if(p[i].kind==BP_TEXT) values[i]=p[i].text;
        else {
            if(p[i].kind==BP_INT) snprintf(numbers[i],80,"%lld",p[i].number);
            else if(p[i].kind==BP_DOUBLE && isfinite(p[i].real)) snprintf(numbers[i],80,"%.17g",p[i].real);
            else return ERR_PARAM;
            values[i]=numbers[i];
        }
    }
    return ERR_OK;
}
void bdb_free(BRows *r)
{
    size_t i,j;
    if(!r) return;
    for(i=0;r->row && i<r->count;i++) {
        if(!r->row[i]) continue;
        for(j=0;j<r->columns;j++) free(r->row[i][j]);
        free(r->row[i]);
    }
    free(r->row); memset(r,0,sizeof(*r));
}
int bdb_query(const char *sql,BParam *p,size_t n,BRows *out)
{
    char query[4096],numbers[32][80];
    const char *values[32]={0};
    int rc,count,cols;
    size_t i,j;
    if(!out) return ERR_PARAM;
    memset(out,0,sizeof(*out));
    if((rc=parameters(sql,p,&n,query,values,numbers))<0) return rc;
    if((rc=db_query_params(query,values,n))<0) return map_error(rc);
    count=db_row_count(); cols=db_field_count();
    if(count<0 || count>10000 || cols<1 || cols>64) { rc=B_ERR_LIMIT; goto done; }
    out->count=(size_t)count; out->columns=(size_t)cols;
    out->row=calloc(out->count ? out->count : 1,sizeof(*out->row));
    if(!out->row) { rc=ERR_NOMEM; goto done; }
    for(i=0;i<out->count;i++) {
        rc=db_fetch_row();
        if(rc!=1) { rc=rc<0 ? rc : ERR_DB_RESULT; goto done; }
        out->row[i]=calloc(out->columns,sizeof(char *));
        if(!out->row[i]) { rc=ERR_NOMEM; goto done; }
        for(j=0;j<out->columns;j++) {
            const char *s=db_get_field((int)j);
            size_t len;
            if(!s) continue;
            len=strlen(s);
            out->row[i][j]=malloc(len+1);
            if(!out->row[i][j]) { rc=ERR_NOMEM; goto done; }
            memcpy(out->row[i][j],s,len+1);
        }
    }
    rc=ERR_OK;
done:
    if(rc<0) map_error(rc);
    else last_error[0]='\0';
    db_free_result();
    if(rc<0) bdb_free(out);
    return rc;
}
int bdb_exec(const char *sql,BParam *p,size_t n,long long *affected,int *insert_id)
{
    char query[4096],numbers[32][80];
    const char *values[32]={0}; int rc,id;
    if((rc=parameters(sql,p,&n,query,values,numbers))<0) return rc;
    rc=db_execute_params(query,values,n);
    if(rc<0) return map_error(rc);
    if(affected) *affected=rc;
    if(insert_id) {
        id=db_last_insert_id();
        if(id<1) return ERR_DB_RESULT;
        *insert_id=id;
    }
    last_error[0]='\0'; return ERR_OK;
}
int bdb_finish(int code)
{
    int rc;
    if(code>=0) {
        rc=db_commit();
        if(rc>=0) return code;
        code=rc;
    }
    rc=db_rollback();
    return rc<0 ? rc : code;
}
