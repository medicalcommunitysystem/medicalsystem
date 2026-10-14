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

    printf("\n全部测试通过\n");
    return 0;
}