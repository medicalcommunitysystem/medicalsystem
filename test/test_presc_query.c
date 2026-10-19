/* ============================================================
 * 文件名  ：test_presc_query.c
 * 功能    ：M05 处方作废与查询测试
 * 作者    ：成员C
 * 创建日期：2026-10-19
 * ============================================================ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "common.h"
#include "db.h"
#include "prescription.h"

#define DB_HOST   "127.0.0.1"
#define DB_USER   "meduser"
#define DB_PASS   "Med@123456"
#define DB_NAME   "medical_system"

#define LIST_MAX 20

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

static long cents(double v)
{
    return (long)(v * 100.0 + 0.5);
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

/* 在结果列表里找某个处方号，返回下标，找不到返回 -1 */
static int find_presc(const Prescription *list, int n, int presc_id)
{
    int i;

    for (i = 0; i < n; i++) {
        if (list[i].presc_id == presc_id) {
            return i;
        }
    }
    return -1;
}

int main(void)
{
    Prescription list[LIST_MAX];
    Prescription p;
    char         day[11];
    int  n, n2, r, idx;
    int  st_before, st_after;

    if (db_connect(DB_HOST, DB_USER, DB_PASS, DB_NAME) != ERR_OK) {
        printf("[FATAL] 连接数据库失败：%s\n", db_error());
        return 1;
    }
    printf("数据库连接成功\n");

    /* ==========================================================
     * 前置条件
     * ========================================================== */
    printf("\n===== 前置条件检查 =====\n");

    checkf(count_rows("SELECT COUNT(*) FROM t_prescription WHERE presc_id = 1") == 1,
           "初始数据里存在处方 1");
    checkf(count_rows("SELECT COUNT(*) FROM t_prescription WHERE presc_id = 2") == 1,
           "初始数据里存在处方 2");

    if (g_fail > 0) {
        printf("\n前置条件不满足，请先执行：sudo mysql < sql/init_db.sql\n");
        db_close();
        return 1;
    }

    /* ==========================================================
     * 测试 1：presc_cancel 参数与异常
     * ========================================================== */
    printf("\n===== 测试 1：作废的参数校验 =====\n");

    checkf(presc_cancel(-1) == ERR_PARAM,
           "presc_cancel(-1) 返回 ERR_PARAM");
    checkf(presc_cancel(0) == ERR_PARAM,
           "presc_cancel(0) 返回 ERR_PARAM");
    checkf(presc_cancel(99999999) == ERR_NOT_FOUND,
           "presc_cancel(99999999) 返回 ERR_NOT_FOUND");

    /* ==========================================================
     * 测试 2：正常作废 + 库存不返还
     * ========================================================== */
    printf("\n===== 测试 2：作废处方 1（含 M001 x 2） =====\n");

    db_execute("UPDATE t_prescription SET status = 1 WHERE presc_id = 1");

    st_before = get_stock("M001");
    r = presc_cancel(1);
    checkf(r == ERR_OK, "作废成功（实际返回 %d）", r);

    memset(&p, 0, sizeof(p));
    if (presc_get_by_id(1, &p) == ERR_OK) {
        checkf(p.status == PRESC_STATUS_CANCEL,
               "处方 1 状态已变为作废（%d）", p.status);
    } else {
        checkf(0, "能读回处方 1");
    }

    st_after = get_stock("M001");
    checkf(st_before >= 0 && st_after == st_before,
           "★ 作废不返还库存：M001 前后都是 %d", st_after);

    /* ==========================================================
     * 测试 3：重复作废（幂等）
     * ========================================================== */
    printf("\n===== 测试 3：重复作废 =====\n");

    checkf(presc_cancel(1) == ERR_OK,
           "对已作废的处方 1 再作废，仍返回 ERR_OK");
    checkf(presc_cancel(2) == ERR_OK,
           "对初始数据里已作废的处方 2 作废，返回 ERR_OK");

    /* 恢复初始状态：处方 1 有效、处方 2 作废 */
    db_execute("UPDATE t_prescription SET status = 1 WHERE presc_id = 1");
    db_execute("UPDATE t_prescription SET status = 0 WHERE presc_id = 2");
    printf("（已恢复初始状态：处方 1 有效，处方 2 作废）\n");

    /* ==========================================================
     * 测试 4：按患者查
     * ========================================================== */
    printf("\n===== 测试 4：按患者查询 =====\n");

    n = presc_query_by_patient("P20260001", list, LIST_MAX);
    checkf(n >= 1, "患者 P20260001 查到 %d 条处方", n);

    idx = (n > 0) ? find_presc(list, n, 1) : -1;
    checkf(idx >= 0, "结果里包含初始的处方 1");

    if (idx >= 0) {
        checkf(strcmp(list[idx].patient_id, "P20260001") == 0,
               "患者编号正确：%s", list[idx].patient_id);
        checkf(strcmp(list[idx].doctor_id, "D001") == 0,
               "医生编号正确：%s", list[idx].doctor_id);
        checkf(strcmp(list[idx].diagnosis, "急性支气管炎") == 0,
               "诊断正确：%s", list[idx].diagnosis);
        checkf(cents(list[idx].total_amount) == 3490,
               "总金额 = 34.90（25.00 + 9.90，实际 %.2f）",
               list[idx].total_amount);
        checkf(list[idx].create_time[0] != '\0',
               "开方时间不为空：%s", list[idx].create_time);

        /* 把这条记录的日期存下来，供后面按日期查询用 */
        snprintf(day, sizeof(day), "%.10s", list[idx].create_time);
    } else {
        snprintf(day, sizeof(day), "%s", "2000-01-01");
    }

    /* 查询结果按处方号倒序 */
    if (n >= 2) {
        checkf(list[0].presc_id > list[n - 1].presc_id,
               "结果按处方号倒序（%d ... %d）",
               list[0].presc_id, list[n - 1].presc_id);
    }

    /* ==========================================================
     * 测试 5：按医生查
     * ========================================================== */
    printf("\n===== 测试 5：按医生查询 =====\n");

    n = presc_query_by_doctor("D001", list, LIST_MAX);
    checkf(n >= 1, "医生 D001 查到 %d 条处方", n);
    checkf(find_presc(list, n, 1) >= 0, "结果里包含处方 1");

    n = presc_query_by_doctor("D003", list, LIST_MAX);
    checkf(n >= 1, "医生 D003 查到 %d 条处方", n);
    checkf(find_presc(list, n, 2) >= 0, "结果里包含处方 2");

    checkf(presc_query_by_doctor("D999", list, LIST_MAX) == 0,
           "不存在的医生 D999 返回 0 条");

    /* ==========================================================
     * 测试 6：按日期查
     * ========================================================== */
    printf("\n===== 测试 6：按日期查询（用处方 1 的实际日期 %s） =====\n", day);

    n = presc_query_by_date(day, day, list, LIST_MAX);
    checkf(n >= 1, "当天查到 %d 条", n);
    checkf(find_presc(list, n, 1) >= 0, "结果里包含处方 1");

    n = presc_query_by_date("2000-01-01", "2000-01-02", list, LIST_MAX);
    checkf(n == 0, "2000 年区间查到 0 条（实际 %d）", n);

    n = presc_query_by_date("2000-01-01", "", list, LIST_MAX);
    checkf(n >= 2, "只给起始日（2000 年起）查到 %d 条", n);

    n = presc_query_by_date("", day, list, LIST_MAX);
    checkf(n >= 1, "只给结束日（到 %s 止）查到 %d 条", day, n);

    n = presc_query_by_date("", "", list, LIST_MAX);
    checkf(n >= 2, "起止都不给 = 不限日期，查到 %d 条", n);

    n = presc_query_by_date(NULL, NULL, list, LIST_MAX);
    checkf(n >= 2, "起止都传 NULL 也能查，%d 条", n);

    /* ==========================================================
     * 测试 7：参数校验
     * ========================================================== */
    printf("\n===== 测试 7：查询的参数校验 =====\n");

    checkf(presc_query_by_patient(NULL, list, LIST_MAX) == ERR_PARAM,
           "patient_id 为 NULL 返回 ERR_PARAM");
    checkf(presc_query_by_patient("", list, LIST_MAX) == ERR_PARAM,
           "patient_id 为空串返回 ERR_PARAM");
    checkf(presc_query_by_patient("P20260001", NULL, LIST_MAX) == ERR_PARAM,
           "list 为 NULL 返回 ERR_PARAM");
    checkf(presc_query_by_patient("P20260001", list, 0) == ERR_PARAM,
           "max = 0 返回 ERR_PARAM");
    checkf(presc_query_by_patient("P' OR '1'='1", list, LIST_MAX) == ERR_PARAM,
           "含引号的 patient_id 被拦截");

    checkf(presc_query_by_doctor("D001", list, 0) == ERR_PARAM,
           "按医生查 max = 0 返回 ERR_PARAM");

    checkf(presc_query_by_date("2026-1-1", "2026-01-02", list, LIST_MAX) == ERR_PARAM,
           "日期格式不对（2026-1-1）返回 ERR_PARAM");
    checkf(presc_query_by_date("20260101", "2026-01-02", list, LIST_MAX) == ERR_PARAM,
           "日期少了横线返回 ERR_PARAM");
    checkf(presc_query_by_date(day, day, list, 0) == ERR_PARAM,
           "按日期查 max = 0 返回 ERR_PARAM");

    /* 小缓冲测试：容量小于结果条数时，只返回容量那么多，且不崩 */
    n2 = presc_query_by_patient("P20260001", list, 1);
    checkf(n2 >= 0 && n2 <= 1, "max = 1 时最多返回 1 条（实际 %d）", n2);

    /* ==========================================================
     * 汇总
     * ========================================================== */
    printf("\n=============================================\n");
    printf("通过 %d 项，失败 %d 项\n", g_pass, g_fail);
    printf("%s\n", (g_fail == 0) ? "全部测试通过"
                                 : "存在失败项，请检查上面的 [FAIL]");

    db_close();
    return (g_fail == 0) ? 0 : 1;
}