#ifndef MEDICINE_H
#define MEDICINE_H

#include "common.h"

typedef struct{
    char med_id[21];  // 药品编号
    char med_name[51];  // 药名
    char spec[51];  // 规格
    char unit[11];  // 单位（盒、瓶、支）
    double price;  // 价格
    int stock;  // 库存数
    int status;  // 1= 有效 0= 停用
} Medicine;

int med_query_all(Medicine *list,int max);
int med_exists(const char *med_id);
int med_get_by_id(const char *med_id,Medicine *out);
int med_add(const Medicine *med);
int med_update(const Medicine *med);
int med_delete(const char *med_id);
int med_get_stock(const char *med_id, int *stock);
int med_stock_in(const char *med_id,int qty);
int med_stock_out(const char *med_id,int qty);
int med_exists_active(const char *med_id);        

#endif