#ifndef MEDICINE_H
#define MEDICINE_H

#include "common.h"
#include <mysql/mysql.h>

typedef struct{
    char med_id[16];  // 药品编号
    char med_name[64];  // 药名
    char catagory[32];  // 分类
    char unit[16];  // 单位（盒、瓶、支）
    double price;  // 价格
    int stock;  // 库存数
    int status;  // 1= 有效 0= 停用
} Medicine;

int med_query_all(Medicine *list,int max);
int med_exists(const char *med_id);
int med_get_by_id(const char *med_id,Medicine *out);
int med_add(const Medicine *med);

#endif