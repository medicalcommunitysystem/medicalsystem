/* ============================================================
 * 文件名  ：test_medicine.c
 * 功能    ：M06 药品管理测试
 *           查询全部、按编号查、查库存、新增、修改、删除（逻辑停用）、
 *           入库、出库（含库存不足）
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

#define DB_HOST   "127.0.0.1"
#define DB_USER   "meduser"
#define DB_PASS   "Med@123456"
#define DB_NAME   "medical_system"

#define LIST_MAX  32
#define TEST_ID   "TM0001"      /* 测试用药品编号，前后会清掉 */

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

/* 直接查库拿库存，用来和接口返回值对照 */
static int raw_stock(const char *med_id)
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

static int raw_status(const char *med_id)
{
    char sql[160];
    int  v = -1;

    snprintf(sql, sizeof(sql),
             "SELECT status FROM t_medicine WHERE med_id = '%s'", med_id);

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

/* 在列表里找某药品，返回下标，找不到 -1 */
static int find_med(const Medicine *list, int n, const char *med_id)
{
    int i;

    for (i = 0; i < n; i++) {
        if (strcmp(list[i].med_id, med_id) == 0) {
            return i;
        }
    }
    return -1;
}

/* 填一条测试药品，status 一定显式设成 1 */
static void fill_med(Medicine *m, const char *id, const char *name,
                     const char *spec, const char *unit,
                     double price, int stock)
{
    memset(m, 0, sizeof(*m));
    snprintf(m->med_id,   sizeof(m->med_id),   "%s", id);
    snprintf(m->med_name, sizeof(m->med_name), "%s", name);
    snprintf(m->spec,     sizeof(m->spec),     "%s", spec);
    snprintf(m->unit,     sizeof(m->unit),     "%s", unit);
    m->price  = price;
    m->stock  = stock;
    m->status = 1;
}

int main(void)
{
    Medicine list[LIST_MAX];
    Medicine m;
    int  n, r, idx;
    int  stock_before, stock_after;

    if (db_connect(DB_HOST, DB_USER, DB_PASS, DB_NAME) != ERR_OK) {
        printf("[FATAL] 连接数据库失败：%s\n", db_error());
        return 1;
    }
    printf("数据库连接成功\n");

    /* 清掉上次跑测试可能残留的测试药品，保证可重复运行 */
    {
        char sql[128];

        snprintf(sql, sizeof(sql),
                 "DELETE FROM t_medicine WHERE med_id = '%s'", TEST_ID);
        db_execute(sql);
    }

    /* ==========================================================
     * 前置条件
     * ========================================================== */
    printf("\n===== 前置条件检查 =====\n");

    checkf(count_rows("SELECT COUNT(*) FROM t_medicine WHERE med_id = 'M001'") == 1,
           "初始药品 M001 存在");
    checkf(count_rows("SELECT COUNT(*) FROM t_medicine WHERE med_id = 'M006'") == 1,
           "初始药品 M006 存在（作废演示用）");

    if (g_fail > 0) {
        printf("\n前置条件不满足，请先执行：sudo mysql < sql/init_db.sql\n");
        db_close();
        return 1;
    }

    /* ==========================================================
     * 测试 1：med_query_all
     * ========================================================== */
    printf("\n===== 测试 1：查询所有有效药品 med_query_all =====\n");

    memset(list, 0, sizeof(list));
    n = med_query_all(list, LIST_MAX);
    checkf(n >= 5, "查到 %d 条有效药品（初始数据是 5 条）", n);

    idx = find_med(list, n, "M001");
    checkf(idx >= 0, "结果里包含 M001");
    if (idx >= 0) {
        checkf(strcmp(list[idx].med_name, "阿莫西林胶囊") == 0,
               "M001 药名正确：%s", list[idx].med_name);
        checkf(cents(list[idx].price) == 1250,
               "M001 单价 12.50（实际 %.2f）", list[idx].price);
        checkf(list[idx].stock == 500,
               "M001 库存 500（实际 %d）", list[idx].stock);
        checkf(list[idx].status == 1,
               "M001 状态为有效（%d）", list[idx].status);
        checkf(list[idx].med_id[0] != '\0' && list[idx].med_name[0] != '\0',
               "M001 字符串字段非空");
    }

    /* 列表里不能出现停用的 M006 */
    checkf(find_med(list, n, "M006") < 0,
           "★ 停用药品 M006 不出现在列表里（WHERE status = 1 生效）");

    /* 编号升序 */
    if (n >= 2) {
        checkf(strcmp(list[0].med_id, list[n - 1].med_id) < 0,
               "结果按编号升序（%s ... %s）",
               list[0].med_id, list[n - 1].med_id);
    }

    /* 容量保护 */
    memset(list, 0, sizeof(list));
    n = med_query_all(list, 2);
    checkf(n == 2, "max = 2 时只返回 2 条（实际 %d）", n);

    n = med_query_all(list, 0);
    checkf(n == 0, "max = 0 时返回 0 条（实际 %d）", n);

    checkf(med_query_all(NULL, LIST_MAX) == ERR_PARAM,
           "list 为 NULL 返回 ERR_PARAM");
    checkf(med_query_all(list, -1) == ERR_PARAM,
           "max 为负返回 ERR_PARAM");

    /* ==========================================================
     * 测试 2：med_get_by_id
     * ========================================================== */
    printf("\n===== 测试 2：按编号查询 med_get_by_id =====\n");

    memset(&m, 0x7f, sizeof(m));     /* 故意填垃圾，验证函数会先清零 */
    r = med_get_by_id("M001", &m);
    checkf(r == ERR_OK, "查 M001 成功（实际 %d）", r);
    checkf(strcmp(m.med_id, "M001") == 0, "编号正确：%s", m.med_id);
    checkf(strcmp(m.med_name, "阿莫西林胶囊") == 0, "药名正确：%s", m.med_name);
    checkf(cents(m.price) == 1250, "单价 12.50（实际 %.2f）", m.price);
    checkf(m.stock == 500, "库存 500（实际 %d）", m.stock);
    checkf(m.status == 1, "状态为有效（%d）", m.status);
    printf("      规格 = \"%s\"，单位 = \"%s\"\n", m.spec, m.unit);

    /* 停用的药品也要能查到，历史处方才能显示药名 */
    r = med_get_by_id("M006", &m);
    checkf(r == ERR_OK, "★ 停用药品 M006 也能查到（实际 %d）", r);
    checkf(m.status == 0, "M006 状态是停用（%d）", m.status);

    checkf(med_get_by_id("NOPE99", &m) == ERR_NOT_FOUND,
           "不存在的编号返回 ERR_NOT_FOUND");
    checkf(med_get_by_id(NULL, &m) == ERR_PARAM,
           "med_id 为 NULL 返回 ERR_PARAM");
    checkf(med_get_by_id("M001", NULL) == ERR_PARAM,
           "out 为 NULL 返回 ERR_PARAM");
    checkf(med_get_by_id("", &m) == ERR_PARAM,
           "med_id 为空串返回 ERR_PARAM");

    /* ==========================================================
     * 测试 3：med_get_stock
     * ========================================================== */
    printf("\n===== 测试 3：查库存 med_get_stock =====\n");

    stock_before = -1;
    r = med_get_stock("M001", &stock_before);
    checkf(r == ERR_OK, "查 M001 库存成功（实际 %d）", r);
    checkf(stock_before == 500, "M001 库存 = 500（实际 %d）", stock_before);

    stock_before = 12345;
    r = med_get_stock("NOPE99", &stock_before);
    checkf(r == ERR_NOT_FOUND, "不存在的药品返回 ERR_NOT_FOUND");
    checkf(stock_before == 0, "失败时 *stock 已被清零（实际 %d）", stock_before);

    checkf(med_get_stock(NULL, &stock_before) == ERR_PARAM,
           "med_id 为 NULL 返回 ERR_PARAM");
    checkf(med_get_stock("M001", NULL) == ERR_PARAM,
           "stock 指针为 NULL 返回 ERR_PARAM");

    /* ==========================================================
     * 测试 4：med_add
     * ========================================================== */
    printf("\n===== 测试 4：新增药品 med_add =====\n");

    fill_med(&m, TEST_ID, "测试药品甲", "10ml*6支", "盒", 33.30, 120);

    r = med_add(&m);
    checkf(r == ERR_OK, "新增成功（实际 %d）", r);
    checkf(count_rows("SELECT COUNT(*) FROM t_medicine WHERE med_id = 'TM0001'") == 1,
           "库里确实多了一条 TM0001");
    checkf(raw_stock(TEST_ID) == 120, "入库库存 = 120（实际 %d）", raw_stock(TEST_ID));
    checkf(raw_status(TEST_ID) == 1, "★ 新药状态是有效 1（实际 %d）", raw_status(TEST_ID));

    /* 读回校验 */
    memset(&m, 0, sizeof(m));
    if (med_get_by_id(TEST_ID, &m) == ERR_OK) {
        checkf(strcmp(m.med_name, "测试药品甲") == 0, "药名写对了：%s", m.med_name);
        checkf(strcmp(m.spec, "10ml*6支") == 0, "规格写对了：%s", m.spec);
        checkf(cents(m.price) == 3330, "单价 33.30（实际 %.2f）", m.price);
    } else {
        checkf(0, "能把刚加的药品读回来");
    }

    /* 重复编号 */
    fill_med(&m, TEST_ID, "重复编号", "x", "盒", 1.00, 1);
    r = med_add(&m);
    checkf(r == ERR_DB_DUP, "★ 重复编号返回 ERR_DB_DUP（实际 %d）", r);
    checkf(count_rows("SELECT COUNT(*) FROM t_medicine WHERE med_id = 'TM0001'") == 1,
           "重复插入没有产生第二条记录");

    /* 参数校验 */
    fill_med(&m, "TM0002", "", "x", "盒", 1.00, 1);
    r = med_add(&m);
    checkf(r == ERR_PARAM, "药名为空返回 ERR_PARAM（实际 %d）", r);

    fill_med(&m, "", "无编号", "x", "盒", 1.00, 1);
    r = med_add(&m);
    checkf(r == ERR_PARAM, "编号为空返回 ERR_PARAM（实际 %d）", r);

    fill_med(&m, "TM0003", "负价格", "x", "盒", -1.00, 1);
    r = med_add(&m);
    checkf(r == ERR_PARAM, "价格为负返回 ERR_PARAM（实际 %d）", r);

    fill_med(&m, "TM0004", "负库存", "x", "盒", 1.00, -5);
    r = med_add(&m);
    checkf(r == ERR_PARAM, "库存为负返回 ERR_PARAM（实际 %d）", r);

    checkf(med_add(NULL) == ERR_PARAM, "med 为 NULL 返回 ERR_PARAM");

    /* ==========================================================
     * 测试 5：med_update
     * ========================================================== */
    printf("\n===== 测试 5：修改药品 med_update =====\n");

    stock_before = raw_stock(TEST_ID);

    fill_med(&m, TEST_ID, "测试药品乙", "20ml*6支", "瓶", 29.90, 999);
    r = med_update(&m);
    checkf(r == ERR_OK, "修改成功（实际 %d）", r);

    memset(&m, 0, sizeof(m));
    if (med_get_by_id(TEST_ID, &m) == ERR_OK) {
        checkf(strcmp(m.med_name, "测试药品乙") == 0, "药名已更新：%s", m.med_name);
        checkf(strcmp(m.spec, "20ml*6支") == 0, "规格已更新：%s", m.spec);
        checkf(strcmp(m.unit, "瓶") == 0, "单位已更新：%s", m.unit);
        checkf(cents(m.price) == 2990, "单价已更新为 29.90（实际 %.2f）", m.price);
    } else {
        checkf(0, "修改后能读回");
    }

    stock_after = raw_stock(TEST_ID);
    checkf(stock_after == stock_before,
           "★ med_update 不改库存（%d → %d，传入的 999 被忽略）",
           stock_before, stock_after);

    /* 不存在的药品 */
    fill_med(&m, "NOPE99", "不存在", "x", "盒", 1.00, 1);
    r = med_update(&m);
    checkf(r == ERR_NOT_FOUND, "修改不存在的药品返回 ERR_NOT_FOUND（实际 %d）", r);

    fill_med(&m, TEST_ID, "", "x", "盒", 1.00, 1);
    checkf(med_update(&m) == ERR_PARAM, "药名为空返回 ERR_PARAM");
    fill_med(&m, TEST_ID, "负价格", "x", "盒", -1.00, 1);
    checkf(med_update(&m) == ERR_PARAM, "价格为负返回 ERR_PARAM");
    checkf(med_update(NULL) == ERR_PARAM, "med 为 NULL 返回 ERR_PARAM");

    /* ==========================================================
     * 测试 6：入库 med_stock_in
     * ========================================================== */
    printf("\n===== 测试 6：入库 med_stock_in =====\n");

    stock_before = raw_stock(TEST_ID);
    r = med_stock_in(TEST_ID, 50);
    checkf(r == ERR_OK, "入库 50 成功（实际 %d）", r);

    stock_after = raw_stock(TEST_ID);
    checkf(stock_after == stock_before + 50,
           "库存正确增加 50（%d → %d）", stock_before, stock_after);

    stock_before = raw_stock(TEST_ID);
    checkf(med_stock_in(TEST_ID, 0) == ERR_PARAM, "入库 0 返回 ERR_PARAM");
    checkf(med_stock_in(TEST_ID, -5) == ERR_PARAM, "入库负数返回 ERR_PARAM");
    checkf(raw_stock(TEST_ID) == stock_before, "非法入库没有改动库存");

    checkf(med_stock_in("NOPE99", 10) == ERR_NOT_FOUND,
           "给不存在的药品入库返回 ERR_NOT_FOUND");
    checkf(med_stock_in(NULL, 10) == ERR_PARAM, "med_id 为 NULL 返回 ERR_PARAM");

    /* ==========================================================
     * 测试 7：出库 med_stock_out
     * ========================================================== */
    printf("\n===== 测试 7：出库 med_stock_out =====\n");

    stock_before = raw_stock(TEST_ID);
    r = med_stock_out(TEST_ID, 20);
    checkf(r == ERR_OK, "出库 20 成功（实际 %d）", r);

    stock_after = raw_stock(TEST_ID);
    checkf(stock_after == stock_before - 20,
           "库存正确减少 20（%d → %d）", stock_before, stock_after);

    /* 出库超过库存 */
    stock_before = raw_stock(TEST_ID);
    r = med_stock_out(TEST_ID, stock_before + 1);
    checkf(r == ERR_STOCK_SHORT,
           "★ 出库超过库存返回 ERR_STOCK_SHORT（实际 %d）", r);
    stock_after = raw_stock(TEST_ID);
    checkf(stock_after == stock_before,
           "出库失败时库存没有被改动（%d → %d）", stock_before, stock_after);

    /* 刚好吃光 */
    stock_before = raw_stock(TEST_ID);
    r = med_stock_out(TEST_ID, stock_before);
    checkf(r == ERR_OK, "刚好出库到 0 成功（实际 %d）", r);
    checkf(raw_stock(TEST_ID) == 0, "库存已经变成 0（实际 %d）", raw_stock(TEST_ID));

    /* 库存为 0 时再出库 */
    r = med_stock_out(TEST_ID, 1);
    checkf(r == ERR_STOCK_SHORT, "库存为 0 时出库返回 ERR_STOCK_SHORT（实际 %d）", r);

    checkf(med_stock_out(TEST_ID, 0) == ERR_PARAM, "出库 0 返回 ERR_PARAM");
    checkf(med_stock_out(TEST_ID, -3) == ERR_PARAM, "出库负数返回 ERR_PARAM");
    checkf(med_stock_out("NOPE99", 1) == ERR_NOT_FOUND,
           "对不存在的药品出库返回 ERR_NOT_FOUND");
    checkf(med_stock_out(NULL, 1) == ERR_PARAM, "med_id 为 NULL 返回 ERR_PARAM");

    /* 用初始数据里的 M004（库存 3）验证真实库存不足 */
    printf("\n----- 用 M004（库存 3）验证库存不足 -----\n");
    stock_before = raw_stock("M004");
    r = med_stock_out("M004", stock_before + 5);
    checkf(r == ERR_STOCK_SHORT, "M004 出库 %d 返回 ERR_STOCK_SHORT（实际 %d）",
           stock_before + 5, r);
    checkf(raw_stock("M004") == stock_before, "M004 库存没变（仍为 %d）", stock_before);

    /* ==========================================================
     * 测试 8：med_delete（逻辑停用）
     * ========================================================== */
    printf("\n===== 测试 8：删除药品 med_delete（逻辑停用） =====\n");

    checkf(raw_status(TEST_ID) == 1, "删除前状态是 1");
    r = med_delete(TEST_ID);
    checkf(r == ERR_OK, "删除成功（实际 %d）", r);
    checkf(raw_status(TEST_ID) == 0,
           "★ 记录还在，只是 status 变成 0（实际 %d）", raw_status(TEST_ID));
    checkf(count_rows("SELECT COUNT(*) FROM t_medicine WHERE med_id = 'TM0001'") == 1,
           "物理记录没有被删掉（历史处方还要用）");

    /* 停用后不出现在列表里 */
    memset(list, 0, sizeof(list));
    n = med_query_all(list, LIST_MAX);
    checkf(find_med(list, n, TEST_ID) < 0, "停用后不再出现在 med_query_all 里");

    /* 但按编号还能查到 */
    memset(&m, 0, sizeof(m));
    checkf(med_get_by_id(TEST_ID, &m) == ERR_OK, "按编号仍能查到（状态为停用）");
    checkf(m.status == 0, "读到 status = 0（实际 %d）", m.status);

    /* 重复删除 */
    r = med_delete(TEST_ID);
    checkf(r == ERR_NOT_FOUND,
           "★ 重复删除返回 ERR_NOT_FOUND（实际 %d）", r);

    checkf(med_delete("NOPE99") == ERR_NOT_FOUND,
           "删除不存在的药品返回 ERR_NOT_FOUND");
    checkf(med_delete(NULL) == ERR_PARAM, "med_id 为 NULL 返回 ERR_PARAM");
    checkf(med_delete("") == ERR_PARAM, "med_id 为空串返回 ERR_PARAM");

    /* ==========================================================
     * 清理
     * ========================================================== */
    printf("\n===== 清理测试数据 =====\n");

    {
        char sql[128];

        snprintf(sql, sizeof(sql),
                 "DELETE FROM t_medicine WHERE med_id = '%s'", TEST_ID);
        db_execute(sql);

        checkf(count_rows("SELECT COUNT(*) FROM t_medicine WHERE med_id = 'TM0001'") == 0,
               "测试药品 TM0001 已从库里清除");

        printf("库中现有药品 %d 条（TM0002~TM0004 都是参数校验失败，从未入库）\n",
               count_rows("SELECT COUNT(*) FROM t_medicine"));
    }

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