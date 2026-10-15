/* test/test_trans.c  —— 事务三件套测试
 * 核心验证：回滚之后，插入的数据必须【查不到】
 */
#include <stdio.h>
#include <string.h>
#include "common.h"
#include "db.h"

#define DB_HOST   "127.0.0.1"
#define DB_USER   "meduser"
#define DB_PASS   "Med@123456"
#define DB_NAME   "medical_system"

static int g_fail = 0;

#define CHECK(cond, msg)                                        \
    do {                                                        \
        if (cond) { printf("[PASS] %s\n", msg); }               \
        else { printf("[FAIL] %s\n", msg); g_fail++; }          \
    } while (0)

/* 数据库中是否存在指定 med_id 的记录 */
static int medicine_exists(const char *med_id)
{
    char sql[256];
    int  found = 0;

    snprintf(sql, sizeof(sql),
             "SELECT med_id FROM t_medicine WHERE med_id='%s'", med_id);

    if (db_query(sql) != ERR_OK) {
        printf("       查询失败: %s\n", db_error());
        db_free_result();
        return -1;
    }
    if (db_fetch_row() == 1) {
        found = 1;
    }
    db_free_result();
    return found;
}

int main(void)
{
    if (db_connect(DB_HOST, DB_USER, DB_PASS, DB_NAME) != ERR_OK) {
        printf("[FAIL] 连接失败: %s\n", db_error());
        return 1;
    }

    /* ---- 测试1：commit 之后数据必须留下 ---- */
    {
        int r;

        db_execute("DELETE FROM t_medicine WHERE med_id='TRANS_A'");

        r = db_begin();
        CHECK(r == ERR_OK, "db_begin 成功");
        if (r != ERR_OK) { printf("       原因: %s\n", db_error()); }

        CHECK(db_execute("INSERT INTO t_medicine (med_id, med_name, price, stock) "
                         "VALUES ('TRANS_A', '事务测试A', 1.00, 10)") >= 0,
              "事务内 INSERT 成功");

        /* 还没提交之前，从本连接应该能看到这条未提交的数据 */
        CHECK(medicine_exists("TRANS_A") == 1,
              "事务内可见自己未提交的改动");

        CHECK(db_commit() == ERR_OK, "db_commit 成功");
        CHECK(medicine_exists("TRANS_A") == 1, "提交后数据存在（正确）");

        /* 清理 */
        db_execute("DELETE FROM t_medicine WHERE med_id='TRANS_A'");
    }

    /* ---- 测试2：rollback 之后数据必须消失 —— 本项目的命脉 ---- */
    {
        db_execute("DELETE FROM t_medicine WHERE med_id='TRANS_B'");

        CHECK(db_begin() == ERR_OK, "db_begin 成功（回滚测试）");

        CHECK(db_execute("INSERT INTO t_medicine (med_id, med_name, price, stock) "
                         "VALUES ('TRANS_B', '事务测试B', 2.00, 20)") >= 0,
              "事务内 INSERT 成功（回滚测试）");

        CHECK(medicine_exists("TRANS_B") == 1,
              "回滚前，事务内可见该数据");

        CHECK(db_rollback() == ERR_OK, "db_rollback 成功");

        CHECK(medicine_exists("TRANS_B") == 0,
              "★ 回滚后数据【不存在】（事务生效）");

        /* 再确认一次：确保真的没落库 */
        db_execute("DELETE FROM t_medicine WHERE med_id='TRANS_B'");
    }

    /* ---- 测试3：多表操作整单回滚（模拟开处方场景） ---- */
    {
        int stock_before = 0;

        db_execute("DELETE FROM t_medicine WHERE med_id='TRANS_C'");
        db_execute("INSERT INTO t_medicine (med_id, med_name, price, stock) "
                   "VALUES ('TRANS_C', '事务测试C', 3.00, 50)");

        CHECK(db_begin() == ERR_OK, "db_begin 成功（多表场景）");

        /* 模拟开处方：扣库存 + 写主表，然后中途失败回滚 */
        CHECK(db_execute("UPDATE t_medicine SET stock=stock-5 "
                         "WHERE med_id='TRANS_C'") >= 0, "扣库存成功");

        /* 检查库存已变为 45 */
        if (db_query("SELECT stock FROM t_medicine WHERE med_id='TRANS_C'") == ERR_OK) {
            if (db_fetch_row() == 1) {
                stock_before = atoi(db_get_field(0));
            }
            db_free_result();
        }
        CHECK(stock_before == 45, "事务内库存已扣减为 45");

        /* 模拟"后续步骤失败"→ 回滚 */
        CHECK(db_rollback() == ERR_OK, "模拟失败后回滚");

        stock_before = 0;
        if (db_query("SELECT stock FROM t_medicine WHERE med_id='TRANS_C'") == ERR_OK) {
            if (db_fetch_row() == 1) {
                stock_before = atoi(db_get_field(0));
            }
            db_free_result();
        }
        CHECK(stock_before == 50, "★ 回滚后库存恢复为 50（扣减被撤销）");

        db_execute("DELETE FROM t_medicine WHERE med_id='TRANS_C'");
    }

    /* ---- 测试4：未连接时返回 ERR_DB_CONN ---- */
    {
        db_close();
        CHECK(db_begin()    == ERR_DB_CONN, "未连接时 db_begin 被拦截");
        CHECK(db_commit()   == ERR_DB_CONN, "未连接时 db_commit 被拦截");
        CHECK(db_rollback() == ERR_DB_CONN, "未连接时 db_rollback 被拦截");
    }

    printf("\n%s（失败 %d 项）\n", g_fail == 0 ? "全部测试通过" : "存在失败", g_fail);
    return g_fail == 0 ? 0 : 1;
}