#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "db.h"
#include "prescription.h"
#include "common.h"
#include "medicine.h"

// 安全返回字符串
static void copy_str(char *dst,size_t size,const char *src)
{
    if(dst == NULL || size == 0) return;
    snprintf(dst,size,"%s",(src != NULL) ? src : "");
}

static int is_safe_token(const char *s)
{
    size_t i, len;

    if (s == NULL) {
        return 0;
    }
    len = strlen(s);
    if (len == 0 || len > 20) {
        return 0;
    }
    for (i = 0; i < len; i++) {
        char c = s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_')) {
            return 0;
        }
    }
    return 1;
}

static int is_safe_text(const char *s)
{
    if (s == NULL) {
        return 1;               /* 当空串处理 */
    }
    for (; *s != '\0'; s++) {
        if (*s == '\'' || *s == '\\') {
            return 0;
        }
    }
    return 1;
}

static int field_int(const char *name)
{
    const char *v = db_get_field_by_name(name);
    return (v != NULL) ? atoi(v) : 0;
}

static double field_double(const char *name)
{
    const char *v = db_get_field_by_name(name);
    return (v != NULL) ? atof(v) : 0.0;
}

// 把金额换算成分
static long to_cents(double yuan)
{
    return (long)(yuan * 100.0 + 0.5);
}

double presc_calc_subtotal(int quantity,double unit_price)
{
    if(quantity <= 0 || unit_price <0.0){
        return 0.0;
    }
    return (double)(to_cents(unit_price) * (long)quantity) / 100.0;
}

// 计算金额
double presc_calc_total(const PrescDetail *items,int n)
{
    long total_cents = 0;
    if (items == NULL || n<= 0){
        return 0.0;
    }
    for (int i=0;i<n;i++){
        total_cents += to_cents(
            presc_calc_subtotal(items[i].quantity, items[i].unit_price));
    }
    return (double)total_cents / 100.0;
}

// 根据id查询处方记录的头
int presc_get_by_id(int presc_id, Prescription *out)
{
    char sql[512];

    if (presc_id <= 0 || out == NULL) {
        return ERR_PARAM;
    }

    memset(out, 0, sizeof(*out));

    snprintf(sql, sizeof(sql),
             "SELECT p.presc_id, p.visit_id, p.patient_id, p.doctor_id, "
             "p.diagnosis, p.status, p.create_time, "
             "COALESCE(SUM(d.subtotal), 0) AS total_amount "
             "FROM t_prescription p "
             "LEFT JOIN t_presc_detail d ON d.presc_id = p.presc_id "
             "WHERE p.presc_id = %d "
             "GROUP BY p.presc_id", presc_id);

    if (db_query(sql) != ERR_OK) {
        db_free_result();
        return ERR_DB_EXEC;
    }

    if (db_fetch_row() != 1) {
        db_free_result();
        return ERR_NOT_FOUND;
    }

    out->presc_id = field_int("presc_id");
    {
        const char *v = db_get_field_by_name("visit_id");
        out->visit_id = (v != NULL) ? atoi(v) : 0;
    }

    copy_str(out->patient_id, sizeof(out->patient_id),
             db_get_field_by_name("patient_id"));
    copy_str(out->doctor_id,  sizeof(out->doctor_id),
             db_get_field_by_name("doctor_id"));
    copy_str(out->diagnosis,  sizeof(out->diagnosis),
             db_get_field_by_name("diagnosis"));
    copy_str(out->create_time, sizeof(out->create_time),
             db_get_field_by_name("create_time"));

    out->status = field_int("status");
    out->total_amount = field_double("total_amount");

    db_free_result();
    return ERR_OK;
}

// 获取处方信息根据处方号
int presc_get_details(int presc_id, PrescDetail *list, int max)
{
    char sql[512];
    int  n = 0;

    if (presc_id <= 0 || list == NULL || max <= 0) {
        return ERR_PARAM;
    }

    snprintf(sql, sizeof(sql),
             "SELECT d.detail_id, d.med_id, m.med_name, "
             "d.quantity, d.dosage, d.unit_price, d.subtotal "
             "FROM t_presc_detail d "
             "LEFT JOIN t_medicine m ON d.med_id = m.med_id "
             "WHERE d.presc_id = %d "
             "ORDER BY d.detail_id", presc_id);

    if (db_query(sql) != ERR_OK) {
        db_free_result();
        return ERR_DB_EXEC;
    }

    while (db_fetch_row() == 1) {
        if (n >= max) {
            break;
        }

        memset(&list[n], 0, sizeof(list[n]));

        list[n].detail_id = field_int("detail_id");
        copy_str(list[n].med_id,   sizeof(list[n].med_id),
                 db_get_field_by_name("med_id"));
        copy_str(list[n].med_name, sizeof(list[n].med_name),
                 db_get_field_by_name("med_name"));
        copy_str(list[n].dosage,   sizeof(list[n].dosage),
                 db_get_field_by_name("dosage"));

        list[n].quantity   = field_int("quantity");
        list[n].unit_price = field_double("unit_price");
        list[n].subtotal   = field_double("subtotal");

        n++;
    }

    db_free_result();
    return n;
}

int presc_add(int visit_id, const char *patient_id, const char *doctor_id,
              const char *diagnosis, PrescDetail *items, int n_items,
              int *out_presc_id)
{
    char       sql[1024];
    const char *diag;
    int        i, j, r;
    int        presc_id = 0;

    if (out_presc_id != NULL) {
        *out_presc_id = 0;
    }


    if (!is_safe_token(patient_id) || !is_safe_token(doctor_id)) {
        return ERR_PARAM;
    }
    if (items == NULL || n_items <= 0 || n_items > PRESC_MAX_DETAILS) {
        return ERR_PARAM;
    }

    diag = (diagnosis != NULL) ? diagnosis : "";
    if (strlen(diag) > 200 || !is_safe_text(diag)) {
        return ERR_PARAM;
    }

    for (i = 0; i < n_items; i++) {
        Medicine med;

        if (!is_safe_token(items[i].med_id) || items[i].quantity <= 0) {
            return ERR_PARAM;
        }
        if (strlen(items[i].dosage) > 100 || !is_safe_text(items[i].dosage)) {
            return ERR_PARAM;
        }

        for (j = 0; j < i; j++) {
            if (strcmp(items[i].med_id, items[j].med_id) == 0) {
                return ERR_PARAM;
            }
        }

        r = med_get_by_id(items[i].med_id, &med);
        if (r != ERR_OK) {
            return r;
        }
        if (med.status != 1) {
            return ERR_NOT_FOUND;
        }

        items[i].unit_price = med.price;
        items[i].subtotal   = presc_calc_subtotal(items[i].quantity, med.price);
    }


    if (db_begin() != ERR_OK) {
        return ERR_DB_EXEC;
    }

    if (visit_id > 0) {
        snprintf(sql, sizeof(sql),
                 "INSERT INTO t_prescription "
                 "(visit_id, patient_id, doctor_id, diagnosis) "
                 "VALUES (%d, '%s', '%s', '%s')",
                 visit_id, patient_id, doctor_id, diag);
    } else {
        snprintf(sql, sizeof(sql),
                 "INSERT INTO t_prescription "
                 "(visit_id, patient_id, doctor_id, diagnosis) "
                 "VALUES (NULL, '%s', '%s', '%s')",
                 patient_id, doctor_id, diag);
    }

    if (db_execute(sql) < 0) {
        int e = db_errno();
        db_rollback();
        if (e == MYSQL_ER_NO_REFERENCED) {
            return ERR_NOT_FOUND;     
        }
        return ERR_DB_EXEC;
    }

    presc_id = db_last_insert_id();
    if (presc_id <= 0) {
        db_rollback();
        return ERR_DB_EXEC;
    }

    for (i = 0; i < n_items; i++) {
        snprintf(sql, sizeof(sql),
                 "INSERT INTO t_presc_detail "
                 "(presc_id, med_id, quantity, dosage, unit_price, subtotal) "
                 "VALUES (%d, '%s', %d, '%s', %.2f, %.2f)",
                 presc_id, items[i].med_id, items[i].quantity,
                 items[i].dosage, items[i].unit_price, items[i].subtotal);

        if (db_execute(sql) < 0) {
            int e = db_errno();
            db_rollback();
            if (e == MYSQL_ER_NO_REFERENCED) {
                return ERR_NOT_FOUND;
            }
            return ERR_DB_EXEC;
        }

        r = med_stock_out(items[i].med_id, items[i].quantity);
        if (r != ERR_OK) {
            db_rollback();            
            return r;
        }
    }

    if (db_commit() != ERR_OK) {
        db_rollback();
        return ERR_DB_EXEC;
    }

    if (out_presc_id != NULL) {
        *out_presc_id = presc_id;
    }
    return ERR_OK;
}

// 判断处方存不存在
int presc_exists(int presc_id)
{
    char sql[128];

    if (presc_id <= 0) {
        return ERR_PARAM;
    }

    snprintf(sql, sizeof(sql),
             "SELECT 1 FROM t_prescription WHERE presc_id = %d", presc_id);

    if (db_query(sql) != ERR_OK) {
        db_free_result();
        return ERR_DB_EXEC;       
    }
    if (db_fetch_row() != 1) {
        db_free_result();
        return 0;                 
    }
    db_free_result();
    return 1;
}