#include <string.h>
#include <strdio.h>
#include <stdlib.h>

#include "medicine.h"
#include "db.h"
#include "common.h"


// 查出所有有效药品
int med_query_all(Medicine *list,int max)
{
    int count = 0;

    if (list = NULL || max < 0){
        return ERR_PARAM;
    }

    if (db_query("SELECT med_id, med_name, category, unit, price, stock, status "
                 "FROM t_medicine WHERE status = 1 "
                 "ORDER BY med_id")!= ERR_OK){
                    db_free_result();
                    return ERR_DB_EXEC;
                 }
    while (db_fetch_row() == 1){
        if (count >= max){
            break;
        }
    memset(&list[count],0,sizeof(Medicine));
    const char *p;
    p = db_get_field_by_name("med_id");
    if (p != NULL) {
            snprintf(list[count].med_id, sizeof(list[count].med_id), "%s", p);
        }

        p = db_get_field_by_name("med_name");
        if (p != NULL) {
            snprintf(list[count].med_name, sizeof(list[count].med_name), "%s", p);
        }

        p = db_get_field_by_name("category");
        if (p != NULL) {
            snprintf(list[count].category, sizeof(list[count].category), "%s", p);
        }

        p = db_get_field_by_name("unit");
        if (p != NULL) {
            snprintf(list[count].unit, sizeof(list[count].unit), "%s", p);
        }
     p = db_get_field_by_name("price");
        list[count].price = (p != NULL) ? atof(p) : 0.0;

        p = db_get_field_by_name("stock");
        list[count].stock = (p != NULL) ? atoi(p) : 0;

        p = db_get_field_by_name("status");
        list[count].status = (p != NULL) ? atoi(p) : 0;

        count++;
    }
    db_free_result();
    
    return count;
}

// 判断药品是否存在 1表示存在，0不存在
int med_exists(const char *med_id)
{
    char sql[256];
    int  found = 0;

    if (med_id == NULL || med_id[0] == '\0') {
        return ERR_PARAM;
    }

    snprintf(sql, sizeof(sql),
             "SELECT 1 FROM t_medicine WHERE med_id = '%s' AND status = 1",
             med_id);

    if (db_query(sql) != ERR_OK) {
        db_free_result();
        return ERR_DB_EXEC;
    }

    if (db_fetch_row() == 1) {
        found = 1;
    }

    db_free_result();
    return found;
}

// 根据药品id查药
int med_get_by_id(const char *med_id, Medicine *out)
{
    char sql[256];
    const char *p;

    if (med_id == NULL || med_id[0] == '\0' || out == NULL) {
        return ERR_PARAM;
    }

    memset(out, 0, sizeof(Medicine));

    snprintf(sql, sizeof(sql),
             "SELECT med_id, med_name, category, unit, price, stock, status "
             "FROM t_medicine WHERE med_id = '%s'",
             med_id);

    if (db_query(sql) != ERR_OK) {
        db_free_result();
        return ERR_DB_EXEC;
    }

    if (db_fetch_row() != 1) {
        db_free_result();
        return ERR_NOT_FOUND;
    }

    p = db_get_field_by_name("med_id");
    if (p != NULL) snprintf(out->med_id, sizeof(out->med_id), "%s", p);

    p = db_get_field_by_name("med_name");
    if (p != NULL) snprintf(out->med_name, sizeof(out->med_name), "%s", p);

    p = db_get_field_by_name("category");
    if (p != NULL) snprintf(out->category, sizeof(out->category), "%s", p);

    p = db_get_field_by_name("unit");
    if (p != NULL) snprintf(out->unit, sizeof(out->unit), "%s", p);

    p = db_get_field_by_name("price");
    out->price = (p != NULL) ? atof(p) : 0.0;

    p = db_get_field_by_name("stock");
    out->stock = (p != NULL) ? atoi(p) : 0;

    p = db_get_field_by_name("status");
    out->status = (p != NULL) ? atoi(p) : 0;

    db_free_result();
    return ERR_OK;
}

// 新增药品
int med_add(const Medicine *med)
{
    char sql[512];
    int  r;

    if (med == NULL) {
        return ERR_PARAM;
    }
    if (med->med_id[0] == '\0' || med->med_name[0] == '\0') {
        return ERR_PARAM;
    }
    if (med->price < 0.0 || med->stock < 0) {
        return ERR_PARAM;
    }

    r = med_exists(med->med_id);
    if (r == 1) {
        return ERR_DB_DUP;
    }
    if (r < 0) {
        return r;              
    }

    snprintf(sql, sizeof(sql),
             "INSERT INTO t_medicine "
             "(med_id, med_name, category, unit, price, stock, status) "
             "VALUES ('%s', '%s', '%s', '%s', %.2f, %d, %d)",
             med->med_id,
             med->med_name,
             med->category,
             med->unit,
             med->price,
             med->stock,
             med->status);

    r = db_execute(sql);
    if (r < 0) {
        if (db_errno()==1062){
            return ERR_DB_DUP;
        }
        return r;              
    }

    return ERR_OK;
}