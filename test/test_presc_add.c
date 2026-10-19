/* ============================================================
 * 文件名  ：test_presc_add.c
 * 功能    ：M05 开方功能测试
 *           金额计算、正常开方、单价快照、库存不足整单回滚、
 *           停用药拦截、参数校验、SQL 注入拦截
 * 作者    ：成员C
 * 创建日期：2026-10-19
 * ============================================================ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "common.h"
#include "db.h"
#include "medicine.h"
#include "prescription.h"

/* 连接配置 —— 按实际环境修改 */
#define DB_HOST   "127.0.0.1"
#define DB_USER   "meduser"
#define DB_PASS   "Med@123456"
#define DB_NAME   "medical_system"

#define MAX_CREATED 8

static int g_pass = 0;
static int g_fail = 0;

static void checkf(int cond, const char *fmt, ...)
{
    va_list ap;

    printf(cond ? "[PASS] " : "[FAIL] ");
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");

    if (cond) {
        g_pass++;
    } else {
        g_fail++;
    }
}

/* 金额比较统一换成"分"，避免浮点误差 */
static long cents(double v)
{
    return (long)(v * 100.0 + 0.5);
}

static int get_stock(const char *med_id)
{
    char sql[160];
    int  v = -1;

    snprintf(sql, sizeof(sql),
             "SELECT stock FROM t_medicine WHERE med_id = '%s'", med_id);

    if (db_query(sql) != ERR_OK) {
        db_free_result();
        return -1;
    }
    if (db_fetch_row() == 1) {
        v = atoi(db_get_field(0));
    }
    db_free_result();
    return v;
}

static int count_rows(const char *sql)
{
    int v = -1;

    if (db_query(sql) != ERR_OK) {
        db_free_result();
        return -1;
    }
    if (db_fetch_row() == 1) {
        v = atoi(db_get_field(0));
    }
    db_free_result();
    return v;
}

int main(void)
{
    PrescDetail   items[PRESC_MAX_DETAILS];
    PrescDetail   det[PRESC_MAX_DETAILS];
    Prescription  p;
    int  pid = 0;
    int  r;
    int  n;
    int  cnt_before, cnt_after;
    int  st_before, st_after;
    int  created[MAX_CREATED];
    int  n_created = 0;

    memset(created, 0, sizeof(created));

    /* ================= 连接 ================= */
    if (db_connect(DB_HOST, DB_USER, DB_PASS, DB_NAME) != ERR_OK) {
        printf("[FATAL] 连接数据库失败：%s\n", db_error());
        return 1;
    }
    printf("数据库连接成功\n");

    /* ==========================================================
     * 测试 0：金额计算（纯函数，不碰数据库）
     * ========================================================== */
    printf("\n===== 测试 0：金额计算 =====\n");

    checkf(cents(presc_calc_subtotal(2, 12.50)) == 2500,
           "小计 2 x 12.50 = 25.00（实际 %.2f）",
           presc_calc_subtotal(2, 12.50));
    checkf(cents(presc_calc_subtotal(3, 6.50)) == 1950,
           "小计 3 x 6.50 = 19.50（实际 %.2f）",
           presc_calc_subtotal(3, 6.50));
    checkf(presc_calc_subtotal(0, 12.50) == 0.0,
           "数量为 0 时小计返回 0");
    checkf(presc_calc_subtotal(-1, 12.50) == 0.0,
           "数量为负时小计返回 0");
    checkf(presc_calc_total(NULL, 0) == 0.0,
           "空明细时总额返回 0");

    memset(items, 0, sizeof(items));
    items[0].quantity   = 2;
    items[0].unit_price = 12.50;
    items[1].quantity   = 1;
    items[1].unit_price = 18.80;

    checkf(cents(presc_calc_total(items, 2)) == 4380,
           "总额 2x12.50 + 1x18.80 = 43.80（实际 %.2f）",
           presc_calc_total(items, 2));
    checkf(cents(items[0].subtotal) == 0,
           "presc_calc_total 不再回写 subtotal（实际 %.2f）",
           items[0].subtotal);

    /* ==========================================================
     * 前置条件：外键要求医生和患者必须存在
     * ========================================================== */
    printf("\n===== 前置条件检查 =====\n");

    checkf(count_rows("SELECT COUNT(*) FROM t_doctor WHERE doctor_id='D001'") == 1,
           "医生 D001 存在");
    checkf(count_rows("SELECT COUNT(*) FROM t_patient WHERE patient_id='P20260001'") == 1,
           "患者 P20260001 存在");

    if (g_fail > 0) {
        printf("\n前置条件不满足，后面的测试没法做。\n");
        printf("请先执行：sudo mysql < sql/init_db.sql\n");
        db_close();
        return 1;
    }

    /* ==========================================================
     * 测试 1：正常开方
     * ========================================================== */
    printf("\n===== 测试 1：正常开方（M001 x 2 + M002 x 1） =====\n");

    st_before = get_stock("M001");
    {
        int st_m002 = get_stock("M002");

        memset(items, 0, sizeof(items));
        snprintf(items[0].med_id, sizeof(items[0].med_id), "M001");
        items[0].quantity = 2;
        snprintf(items[0].dosage, sizeof(items[0].dosage), "一日三次，每次1粒");
        snprintf(items[1].med_id, sizeof(items[1].med_id), "M002");
        items[1].quantity = 1;
        snprintf(items[1].dosage, sizeof(items[1].dosage), "一日两次，每次1粒");

        pid = 0;
        r = presc_add(0, "P20260001", "D001", "上呼吸道感染", items, 2, &pid);

        if (r == ERR_OK) {
            checkf(pid > 0, "开方成功，处方号 = %d", pid);
            if (n_created < MAX_CREATED) {
                created[n_created++] = pid;
            }
        } else {
            checkf(0, "开方失败，返回 %d，原因：%s", r, db_error());
        }

        /* 主表 */
        if (pid > 0 && presc_get_by_id(pid, &p) == ERR_OK) {
            checkf(strcmp(p.patient_id, "P20260001") == 0,
                   "患者编号正确：%s", p.patient_id);
            checkf(strcmp(p.doctor_id, "D001") == 0,
                   "医生编号正确：%s", p.doctor_id);
            checkf(strcmp(p.diagnosis, "上呼吸道感染") == 0,
                   "诊断正确：%s", p.diagnosis);
            checkf(p.status == PRESC_STATUS_NORMAL,
                   "状态为正常（%d）", p.status);
            checkf(cents(p.total_amount) == 4380,
                   "总金额 = 43.80（实际 %.2f）", p.total_amount);
            checkf(p.create_time[0] != '\0',
                   "开方时间已写入：%s", p.create_time);
        } else {
            checkf(0, "presc_get_by_id 能读回这条处方");
        }

        /* 明细 */
        memset(det, 0, sizeof(det));
        n = (pid > 0) ? presc_get_details(pid, det, PRESC_MAX_DETAILS) : -1;
        checkf(n == 2, "明细条数 = 2（实际 %d）", n);

        if (n == 2) {
            checkf(strcmp(det[0].med_id, "M001") == 0, "明细1 药品 = M001");
            checkf(det[0].quantity == 2, "明细1 数量 = 2（实际 %d）", det[0].quantity);
            checkf(cents(det[0].unit_price) == 1250,
                   "明细1 单价 = 12.50（实际 %.2f）", det[0].unit_price);
            checkf(cents(det[0].subtotal) == 2500,
                   "明细1 小计 = 25.00（实际 %.2f）", det[0].subtotal);
            checkf(strcmp(det[0].med_name, "阿莫西林胶囊") == 0,
                   "明细1 中文药名正确：%s", det[0].med_name);
            checkf(strcmp(det[0].dosage, "一日三次，每次1粒") == 0,
                   "明细1 用法用量正确：%s", det[0].dosage);

            checkf(strcmp(det[1].med_id, "M002") == 0, "明细2 药品 = M002");
            checkf(det[1].quantity == 1, "明细2 数量 = 1（实际 %d）", det[1].quantity);
            checkf(cents(det[1].unit_price) == 1880,
                   "明细2 单价 = 18.80（实际 %.2f）", det[1].unit_price);
            checkf(cents(det[1].subtotal) == 1880,
                   "明细2 小计 = 18.80（实际 %.2f）", det[1].subtotal);
        }

        /* 库存 */
        st_after = get_stock("M001");
        checkf(st_before >= 0 && st_after == st_before - 2,
               "M001 库存正确扣减 2（%d → %d）", st_before, st_after);

        st_after = get_stock("M002");
        checkf(st_m002 >= 0 && st_after == st_m002 - 1,
               "M002 库存正确扣减 1（%d → %d）", st_m002, st_after);
    }

    /* ==========================================================
     * 测试 2：单价快照
     * ========================================================== */
    printf("\n===== 测试 2：单价快照不被篡改 =====\n");

    memset(items, 0, sizeof(items));
    snprintf(items[0].med_id, sizeof(items[0].med_id), "M001");
    items[0].quantity   = 1;
    items[0].unit_price = 0.01;        /* 故意传错 */
    snprintf(items[0].dosage, sizeof(items[0].dosage), "睡前一次");

    pid = 0;
    r = presc_add(0, "P20260001", "D001", "单价快照测试", items, 1, &pid);

    if (r == ERR_OK && pid > 0) {
        if (n_created < MAX_CREATED) {
            created[n_created++] = pid;
        }

        memset(det, 0, sizeof(det));
        n = presc_get_details(pid, det, PRESC_MAX_DETAILS);

        if (n == 1) {
            checkf(cents(det[0].unit_price) == 1250,
                   "传入的 0.01 被库里的 12.50 覆盖（实际 %.2f）",
                   det[0].unit_price);
            checkf(cents(det[0].subtotal) == 1250,
                   "小计按真实单价算 = 12.50（实际 %.2f）", det[0].subtotal);
        } else {
            checkf(0, "能读回 1 条明细（实际 %d）", n);
        }
    } else {
        checkf(0, "presc_add 成功（实际返回 %d，原因：%s）", r, db_error());
    }

    /* ==========================================================
     * 测试 3：库存不足 —— 整单回滚
     * ========================================================== */
    printf("\n===== 测试 3：库存不足整单回滚（M001 x 1 + M004 x 5） =====\n");
    printf("（M004 维生素C片库存只有 3，开 5 必然失败）\n");

    cnt_before = count_rows("SELECT COUNT(*) FROM t_prescription");
    st_before  = get_stock("M001");
    {
        int st_m004 = get_stock("M004");
        int det_before = count_rows("SELECT COUNT(*) FROM t_presc_detail");

        memset(items, 0, sizeof(items));
        snprintf(items[0].med_id, sizeof(items[0].med_id), "M001");
        items[0].quantity = 1;
        snprintf(items[1].med_id, sizeof(items[1].med_id), "M004");
        items[1].quantity = 5;

        pid = 0;
        r = presc_add(0, "P20260001", "D001", "库存不足测试", items, 2, &pid);

        checkf(r == ERR_STOCK_SHORT, "返回 ERR_STOCK_SHORT（实际 %d）", r);
        checkf(pid == 0, "失败时 out_presc_id 未被写入（实际 %d）", pid);

        cnt_after = count_rows("SELECT COUNT(*) FROM t_prescription");
        checkf(cnt_after == cnt_before,
               "处方表未新增记录（%d → %d）", cnt_before, cnt_after);

        n = count_rows("SELECT COUNT(*) FROM t_presc_detail");
        checkf(n == det_before, "明细表未新增记录（%d → %d）", det_before, n);

        st_after = get_stock("M001");
        checkf(st_before >= 0 && st_after == st_before,
               "★ M001 库存已回滚（%d → %d）", st_before, st_after);

        st_after = get_stock("M004");
        checkf(st_m004 >= 0 && st_after == st_m004,
               "M004 库存未变（%d → %d）", st_m004, st_after);
    }

    /* ==========================================================
     * 测试 4：停用药品
     * ========================================================== */
    printf("\n===== 测试 4：停用药品拦截（M006 status=0） =====\n");

    memset(items, 0, sizeof(items));
    snprintf(items[0].med_id, sizeof(items[0].med_id), "M006");
    items[0].quantity = 1;

    pid = 0;
    r = presc_add(0, "P20260001", "D001", "停用药测试", items, 1, &pid);
    checkf(r == ERR_NOT_FOUND, "返回 ERR_NOT_FOUND（实际 %d）", r);
    checkf(pid == 0, "未生成处方号（实际 %d）", pid);
    checkf(count_rows("SELECT COUNT(*) FROM t_prescription "
                      "WHERE diagnosis='停用药测试'") == 0,
           "库里未留下这条处方");

    /* ==========================================================
     * 测试 5：参数校验
     * ========================================================== */
    printf("\n===== 测试 5：参数校验 =====\n");

    pid = 0;
    r = presc_add(0, "P20260001", "D001", "x", NULL, 1, &pid);
    checkf(r == ERR_PARAM, "items 为 NULL 被拦截（实际 %d）", r);

    memset(items, 0, sizeof(items));
    snprintf(items[0].med_id, sizeof(items[0].med_id), "M001");
    items[0].quantity = 1;

    
    r = presc_add(0, "P20260001", "D001", "x", items, 0, &pid);
    checkf(r == ERR_PARAM, "n_items = 0 被拦截（实际 %d）", r);

    r = presc_add(0, "P20260001", "D001", "x", items, PRESC_MAX_DETAILS + 1, &pid);
    checkf(r == ERR_PARAM, "n_items 超上限被拦截（实际 %d）", r);

    items[0].quantity = 0;
    r = presc_add(0, "P20260001", "D001", "x", items, 1, &pid);
    checkf(r == ERR_PARAM, "quantity = 0 被拦截（实际 %d）", r);

    items[0].quantity = -3;
    r = presc_add(0, "P20260001", "D001", "x", items, 1, &pid);
    checkf(r == ERR_PARAM, "quantity 为负被拦截（实际 %d）", r);

    items[0].quantity = 1;
    r = presc_add(0,NULL, "D001", "x", items, 1, &pid);
    checkf(r == ERR_PARAM, "patient_id 为 NULL 被拦截（实际 %d）", r);

    memset(items, 0, sizeof(items));
    snprintf(items[0].med_id, sizeof(items[0].med_id), "NOPE99");
    items[0].quantity = 1;
    r = presc_add(0, "P20260001", "D001", "x", items, 1, &pid);
    checkf(r == ERR_NOT_FOUND, "不存在的药品被拦截（实际 %d）", r);

    /* 重复药品 */
    memset(items, 0, sizeof(items));
    snprintf(items[0].med_id, sizeof(items[0].med_id), "M001");
    items[0].quantity = 1;
    snprintf(items[1].med_id, sizeof(items[1].med_id), "M002");
    items[1].quantity = 1;
    snprintf(items[2].med_id, sizeof(items[2].med_id), "M001");
    items[2].quantity = 1;
    r = presc_add(0, "P20260001", "D001", "x", items, 3, &pid);
    checkf(r == ERR_PARAM, "第 3 行重复 M001 被拦截（实际 %d）", r);

    /* 患者不存在 */
    memset(items, 0, sizeof(items));
    snprintf(items[0].med_id, sizeof(items[0].med_id), "M001");
    items[0].quantity = 1;
    r = presc_add(0, "P99999999", "D001", "x", items, 1, &pid);
    checkf(r == ERR_NOT_FOUND, "不存在的患者被拦截（实际 %d）", r);
    checkf(count_rows("SELECT COUNT(*) FROM t_prescription "
                      "WHERE patient_id='P99999999'") == 0,
           "外键失败后没有残留处方");

    /* ==========================================================
     * 测试 6：SQL 注入拦截
     * ========================================================== */
    printf("\n===== 测试 6：SQL 注入拦截 =====\n");

    memset(items, 0, sizeof(items));
    snprintf(items[0].med_id, sizeof(items[0].med_id), "M001");
    items[0].quantity = 1;

    r = presc_add(0, "P20260001' OR '1'='1", "D001", "x", items, 1, &pid);
    checkf(r == ERR_PARAM, "含单引号的 patient_id 被拦截（实际 %d）", r);

    r = presc_add(0, "P20260001", "D001", "诊断为'; DROP TABLE t_medicine; --",
                  items, 1, &pid);
    checkf(r == ERR_PARAM, "含单引号的 diagnosis 被拦截（实际 %d）", r);
    checkf(count_rows("SELECT COUNT(*) FROM t_medicine") > 0,
           "t_medicine 表还在");

    /* ==========================================================
     * 测试 7：查询不存在的处方
     * ========================================================== */
    printf("\n===== 测试 7：查询不存在的处方 =====\n");

    checkf(presc_get_by_id(99999999, &p) == ERR_NOT_FOUND,
           "presc_get_by_id(99999999) 返回 ERR_NOT_FOUND");
    checkf(presc_get_details(99999999, det, PRESC_MAX_DETAILS) == 0,
           "presc_get_details(99999999) 返回 0 条");
    checkf(presc_exists(99999999) == 0,
           "presc_exists(99999999) 返回 0");
    checkf(presc_exists(-1) == ERR_PARAM,
           "presc_exists(-1) 返回 ERR_PARAM");
    checkf(presc_exists(1) == 1,
           "presc_exists(1) 返回 1（初始数据里的处方）");

    /* ==========================================================
     * 汇总
     * ========================================================== */
    printf("\n=============================================\n");
    printf("通过 %d 项，失败 %d 项\n", g_pass, g_fail);
    printf("%s\n", (g_fail == 0) ? "全部测试通过" : "存在失败项，请检查上面的 [FAIL]");

    if (n_created > 0) {
        printf("\n本次测试生成的处方号：");
        for (n = 0; n < n_created; n++) {
            printf("%d ", created[n]);
        }
        printf("\n清理这些数据（先删明细再删主表）：\n");
        printf("  DELETE FROM t_presc_detail WHERE presc_id IN (");
        for (n = 0; n < n_created; n++) {
            printf("%s%d", (n == 0) ? "" : ",", created[n]);
        }
        printf(");\n  DELETE FROM t_prescription WHERE presc_id IN (");
        for (n = 0; n < n_created; n++) {
            printf("%s%d", (n == 0) ? "" : ",", created[n]);
        }
        printf(");\n");
    }

    db_close();
    return (g_fail == 0) ? 0 : 1;
}