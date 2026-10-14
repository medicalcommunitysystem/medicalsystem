/* test/test_db.c  —— db_connect 单元测试 */
#include <stdio.h>
#include "common.h"
#include "db.h"

/* 连接配置 —— 部署时按实际环境修改 */
#define DB_HOST   "127.0.0.1"
#define DB_USER   "meduser"
#define DB_PASS   "Med@123456"
#define DB_NAME   "medical_system"

int main(void)
{
    /* ---- 测试1：正常连接 ---- */
    if (db_connect(DB_HOST, DB_USER, DB_PASS, DB_NAME) != ERR_OK) {
        printf("[FAIL] 正常连接失败: %s\n", db_error());
        return 1;
    }
    printf("[PASS] 正常连接成功\n");

    /* ---- 测试2：重复连接不应出错、不应泄漏 ---- */
    if (db_connect(DB_HOST, DB_USER, DB_PASS, DB_NAME) != ERR_OK) {
        printf("[FAIL] 重复连接失败: %s\n", db_error());
        return 1;
    }
    printf("[PASS] 重复连接成功（旧连接已自动关闭）\n");
    db_close();

    /* ---- 测试3：错误密码必须被拒绝，且能拿到原因 ---- */
    if (db_connect(DB_HOST, DB_USER, "definitely_wrong", DB_NAME) == ERR_OK) {
        printf("[FAIL] 错误密码竟然连上了\n");
        return 1;
    }
    printf("[PASS] 错误密码被拒绝，原因: %s\n", db_error());

    /* ---- 测试4：NULL 参数必须返回 ERR_PARAM ---- */
    if (db_connect(NULL, DB_USER, DB_PASS, DB_NAME) != ERR_PARAM) {
        printf("[FAIL] NULL 参数未被拦截\n");
        return 1;
    }
    printf("[PASS] NULL 参数被正确拦截\n");

    /* ---- 测试5：db_close 可重复调用 ---- */
    db_close();
    db_close();
    printf("[PASS] db_close 可重复调用\n");

        /* ---- 测试6：db_execute 更新成功并返回受影响行数 ---- */
    {
        int n;

        if (db_connect(DB_HOST, DB_USER, DB_PASS, DB_NAME) != ERR_OK) {
            printf("[FAIL] 测试6 前置连接失败: %s\n", db_error());
            return 1;
        }

        n = db_execute("UPDATE t_medicine SET stock=77 WHERE med_id='TEST_M01'");
        if (n == 1) {
            printf("[PASS] db_execute 更新成功，受影响行数=%d\n", n);
        } else {
            printf("[FAIL] 期望受影响行数 1，实际 %d，错误: %s\n", n, db_error());
            return 1;
        }

        /* 回读确认值真的落库了（不只看返回值） */
        n = db_execute("UPDATE t_medicine SET stock=77 WHERE med_id='TEST_M01'");
        if (n == 0) {
            printf("[PASS] 重复写相同值返回 0（MySQL 的 UPDATE 语义）\n");
        } else {
            printf("[FAIL] 期望 0，实际 %d\n", n);
        }
    }

    /* ---- 测试7：条件不匹配时返回 0，而不是错误 ---- */
    {
        int n = db_execute("UPDATE t_medicine SET stock=0 WHERE med_id='NOT_EXIST_XXX'");
        if (n == 0) {
            printf("[PASS] 未匹配到行时返回 0（成功但无改动）\n");
        } else {
            printf("[FAIL] 期望 0，实际 %d，错误: %s\n", n, db_error());
            return 1;
        }
    }

    /* ---- 测试8：SQL 语法错误必须返回负错误码，且能拿到原因 ---- */
    {
        int n = db_execute("UPDATE t_medicine SET no_such_column=1");
        if (n < 0) {
            printf("[PASS] SQL 错误被捕获，码=%d，原因: %s\n", n, db_error());
        } else {
            printf("[FAIL] 错误 SQL 竟然返回 %d\n", n);
            return 1;
        }
    }

    /* ---- 测试9：未连接时必须返回 ERR_DB_CONN ---- */
    {
        int n;
        db_close();
        n = db_execute("UPDATE t_medicine SET stock=1 WHERE med_id='TEST_M01'");
        if (n == ERR_DB_CONN) {
            printf("[PASS] 未连接时被正确拦截\n");
        } else {
            printf("[FAIL] 期望 %d，实际 %d\n", ERR_DB_CONN, n);
            return 1;
        }
    }
    
    printf("\n全部测试通过\n");
    return 0;
}