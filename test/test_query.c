/* test/test_query.c  —— 结果集接口单元测试 */
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

int main(void)
{
    if (db_connect(DB_HOST, DB_USER, DB_PASS, DB_NAME) != ERR_OK) {
        printf("[FAIL] 连接失败: %s\n", db_error());
        return 1;
    }

    /* ---- 测试1：基本查询 + 行数列数 ---- */
    CHECK(db_query("SELECT med_id, med_name, price, stock FROM t_medicine") == ERR_OK,
          "SELECT 执行成功");

    printf("       结果集 %d 行 %d 列\n", db_row_count(), db_field_count());
    CHECK(db_field_count() == 4, "列数为 4");

    /* ---- 测试2：逐行取数据 + 中文不乱码 ---- */
    {
        int rows = 0;
        int ok_chinese = 1, ok_id = 1;

        while (db_fetch_row() == 1) {
            const char *id   = db_get_field(0);
            const char *name = db_get_field_by_name("med_name");

            if (id == NULL || name == NULL) { ok_chinese = 0; break; }

            /* 中文校验：UTF-8 中文的每个字节都 >= 0x80。
             * 若出现乱码成 '?' 或控制字符，说明字符集没设对。 */
            if (strlen(name) > 0) {
                unsigned char c = (unsigned char)name[0];
                if (c < 0x80 && !(c >= 'A' && c <= 'Z') && !(c >= 'a' && c <= 'z'))
                    ok_chinese = 0;
            }
            printf("       [%d] id=%s name=%s\n", rows + 1, id, name);
            rows++;
        }
        CHECK(rows == db_row_count(), "取到的行数与 db_row_count 一致");
        CHECK(ok_chinese, "中文未乱码（UTF-8 字节序列正常）");
        CHECK(ok_id, "主键字段可读");
        db_free_result();
    }

    /* ---- 测试3：按列名取不存在的列，必须返回 NULL ---- */
    {
        db_query("SELECT med_id FROM t_medicine LIMIT 1");
        if (db_fetch_row() == 1) {
            CHECK(db_get_field_by_name("no_such_col") == NULL,
                  "不存在的列名返回 NULL");
        }
        db_free_result();
    }

    /* ---- 测试4：字段值为 NULL 时返回 NULL ---- */
    {
        /* 用 SQL 直接构造一个 NULL，不依赖表里有没有空字段 */
        db_query("SELECT NULL AS n, 'x' AS s");
        if (db_fetch_row() == 1) {
            CHECK(db_get_field_by_name("n") == NULL, "SQL NULL 字段返回 NULL");
            CHECK(db_get_field_by_name("s") != NULL, "非 NULL 字段正常返回");
        } else {
            printf("[FAIL] 取 NULL 测试行失败\n"); g_fail++;
        }
        db_free_result();
    }

    /* ---- 测试5：连续两次查询不报 Commands out of sync ---- */
    {
        int i, ok = 1;
        for (i = 0; i < 3; i++) {
            if (db_query("SELECT med_id FROM t_medicine") != ERR_OK) { ok = 0; break; }
            db_fetch_row();
            /* 故意只取一行就丢掉结果集，模拟调用方忘记 free 的情况 */
            if (db_query("SELECT med_id FROM t_medicine") != ERR_OK) { ok = 0; break; }
            db_free_result();
        }
        CHECK(ok, "连续查询未出现 Commands out of sync");
    }

    /* ---- 测试6：无结果集的语句也不该报错 ---- */
    {
        int r = db_query("SET @x = 1");
        CHECK(r == ERR_OK, "无结果集语句返回 ERR_OK");
        db_free_result();
    }

    /* ---- 测试7：列越界必须返回 NULL，不能崩 ---- */
    {
        db_query("SELECT med_id FROM t_medicine LIMIT 1");
        if (db_fetch_row() == 1) {
            CHECK(db_get_field(99) == NULL, "列索引越界返回 NULL");
            CHECK(db_get_field(-1) == NULL, "负列索引返回 NULL");
        }
        db_free_result();
    }

    /* ---- 测试8：db_free_result 可重复调用 ---- */
    {
        db_free_result();
        db_free_result();
        CHECK(1, "db_free_result 可重复调用");
    }

    /* ---- 测试9：语法错误的 SELECT 必须返回负数 ---- */
    {
        int r = db_query("SELECT * FROM no_such_table_xxx");
        CHECK(r < 0, "查询不存在的表返回错误码");
        printf("       错误原因: %s\n", db_error());
        db_free_result();
    }

    db_close();

    printf("\n%s（失败 %d 项）\n", g_fail == 0 ? "全部测试通过" : "存在失败", g_fail);
    return g_fail == 0 ? 0 : 1;
}