#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "menu_c.h"
#include "medicine.h"
#include "prescription.h"
#include "visit.h"        
#include "member_b.h"     
#include "auth.h"
#include "db.h"           

#define C_LIST_MAX   20
#define C_MAX_QTY    1000000
#define C_MAX_PRICE  99999999.99


static int c_choice(int max_option, int *out)
{
    int rc;

    rc = common_read_int("请选择：", 0, max_option, out);
    if (rc == B_ERR_INPUT) {
        putchar('\n');
        return -1;
    }
    if (rc != ERR_OK) {
        puts("输入无效，请重新选择。");
        return 0;
    }
    return 1;
}

static void c_show(int rc)
{
    switch (rc) {
    case ERR_OK:
        puts("操作成功。");
        return;
    case ERR_STOCK_SHORT:
        puts("药品库存不足，操作已取消。");
        return;
    case ERR_INVALID_QTY:
        puts("数量必须大于 0。");
        return;
    case ERR_DB_CONN:
        puts("数据库未连接。");
        return;
    case ERR_DB_BEGIN:
    case ERR_DB_COMMIT:
    case ERR_DB_ROLLBACK:
    case ERR_DB_EXEC:
        printf("数据库操作失败：%s\n", db_error());
        return;
    case AUTH_ERR_LOCKED:
        puts("账号已被锁定，请稍后再试。");
        return;
    case AUTH_ERR_NOT_LOGGED_IN:
        puts("尚未登录。");
        return;
    case AUTH_ERR_FORBIDDEN:
        puts("没有权限执行该操作。");
        return;
    default:
        break;
    }
    printf("%s\n", b_error_message(rc));
}

/* 统一的会话检查（照 A 的写法：先校验账号是否仍然有效） */
static int c_require(const UserSession *session)
{
    AuthResult rc = auth_require_active(session);

    if (rc == AUTH_OK) {
        return ERR_OK;
    }
    c_show((int)rc);
    return (int)rc;
}

static const char *c_presc_status_text(int status)
{
    if (status == PRESC_STATUS_NORMAL) {
        return "有效";
    }
    if (status == PRESC_STATUS_CANCEL) {
        return "已作废";
    }
    if (status == PRESC_STATUS_RETURN) {
        return "已退药";
    }
    return "未知";
}

static void c_show_medicine(const Medicine *m)
{
    printf("%-8s %-20s %-14s %-6s %10.2f %8d  %s\n",
           m->med_id, m->med_name, m->spec, m->unit,
           m->price, m->stock, (m->status == 1) ? "有效" : "停用");
}

static void c_show_presc_row(const Prescription *p)
{
    printf("%-6d %-14s %-8s %10.2f %-7s %s\n",
           p->presc_id, p->patient_id, p->doctor_id,
           p->total_amount, c_presc_status_text(p->status), p->create_time);
}

/* ---------------- M06 药品管理 ---------------- */

static void c_med_list(void)
{
    Medicine list[C_LIST_MAX];
    int n, i, low = 0;

    n = med_query_all(list, C_LIST_MAX);
    if (n < 0) {
        c_show(n);
        return;
    }
    if (n == 0) {
        puts("药品库中没有有效药品。");
        return;
    }

    puts("编号     名称                 规格           单位     单价       库存   状态");
    for (i = 0; i < n; ++i) {
        c_show_medicine(&list[i]);
        if (list[i].stock < LOW_STOCK) {
            ++low;
        }
    }
    printf("共 %d 种有效药品；库存低于 %d 的有 %d 种。\n", n, LOW_STOCK, low);
    if (n == C_LIST_MAX) {
        printf("（本次只显示前 %d 条）\n", C_LIST_MAX);
    }
}

static void c_med_add(void)
{
    Medicine m;
    int rc;

    memset(&m, 0, sizeof(m));

    if (common_read_line("药品编号（如 M007，20 字符内）：", m.med_id, sizeof(m.med_id)) != ERR_OK) {
        puts("输入结束。");
        return;
    }
    if (!common_valid_text(m.med_id, 20, 1)) {
        puts("编号为空、超长或含非法字符。");
        return;
    }
    if (common_read_line("药品名称（50 字符内）：", m.med_name, sizeof(m.med_name)) != ERR_OK ||
        !common_valid_text(m.med_name, 50, 1)) {
        puts("名称不合法。");
        return;
    }
    if (common_read_line("规格（可留空，50 字符内）：", m.spec, sizeof(m.spec)) != ERR_OK ||
        !common_valid_text(m.spec, 50, 0)) {
        puts("规格不合法。");
        return;
    }
    if (common_read_line("单位（如 盒 / 瓶 / 支，可留空）：", m.unit, sizeof(m.unit)) != ERR_OK ||
        !common_valid_text(m.unit, 10, 0)) {
        puts("单位不合法。");
        return;
    }
    if (common_read_double("单价（元）：", 0.0, C_MAX_PRICE, &m.price) != ERR_OK) {
        puts("单价输入无效。");
        return;
    }
    if (common_read_int("初始库存：", 0, C_MAX_QTY, &m.stock) != ERR_OK) {
        puts("库存输入无效。");
        return;
    }

    m.status = 1;    /* 新增一律有效，停用请走“4 停用药品” */
    rc = med_add(&m);
    if (rc == ERR_DB_DUP) {
        printf("编号 %s 已存在，新增失败。\n", m.med_id);
        return;
    }
    c_show(rc);
}

static void c_med_edit(void)
{
    char id[21];
    char buf[201];
    Medicine m;
    int rc;

    if (common_read_line("要修改的药品编号：", id, sizeof(id)) != ERR_OK) {
        puts("输入结束。");
        return;
    }
    if (!common_valid_text(id, 20, 1)) {
        puts("编号无效。");
        return;
    }
    rc = med_get_by_id(id, &m);
    if (rc != ERR_OK) {
        c_show(rc);
        return;
    }

    puts("当前信息：");
    c_show_medicine(&m);
    puts("（直接回车表示不修改；库存不能在这里改，请用入库/出库）");

    if (common_read_line("新名称：", buf, sizeof(buf)) != ERR_OK) {
        puts("输入结束。");
        return;
    }
    if (buf[0] != '\0') {
        if (!common_valid_text(buf, 50, 1)) {
            puts("名称不合法。");
            return;
        }
        common_copy(m.med_name, sizeof(m.med_name), buf);
    }

    if (common_read_line("新规格：", buf, sizeof(buf)) != ERR_OK) {
        puts("输入结束。");
        return;
    }
    if (buf[0] != '\0') {
        if (!common_valid_text(buf, 50, 0)) {
            puts("规格不合法。");
            return;
        }
        common_copy(m.spec, sizeof(m.spec), buf);
    }

    if (common_read_line("新单位：", buf, sizeof(buf)) != ERR_OK) {
        puts("输入结束。");
        return;
    }
    if (buf[0] != '\0') {
        if (!common_valid_text(buf, 10, 0)) {
            puts("单位不合法。");
            return;
        }
        common_copy(m.unit, sizeof(m.unit), buf);
    }

    if (common_read_line("新单价（元）：", buf, sizeof(buf)) != ERR_OK) {
        puts("输入结束。");
        return;
    }
    if (buf[0] != '\0') {
        char *end = NULL;
        double price;

        price = strtod(buf, &end);
        if (end == buf || *end != '\0' || price < 0.0 || price > C_MAX_PRICE) {
            puts("单价无效。");
            return;
        }
        m.price = price;
    }

    if (common_read_line("状态（1 有效 / 0 停用）：", buf, sizeof(buf)) != ERR_OK) {
        puts("输入结束。");
        return;
    }
    if (buf[0] != '\0') {
        if (strcmp(buf, "1") == 0) {
            m.status = 1;
        } else if (strcmp(buf, "0") == 0) {
            m.status = 0;
        } else {
            puts("状态只能是 1 或 0。");
            return;
        }
    }

    c_show(med_update(&m));
}

static void c_med_stop(void)
{
    char id[21];
    Medicine m;
    int rc, confirm;

    if (common_read_line("要停用的药品编号：", id, sizeof(id)) != ERR_OK) {
        puts("输入结束。");
        return;
    }
    if (!common_valid_text(id, 20, 1)) {
        puts("编号无效。");
        return;
    }
    rc = med_get_by_id(id, &m);
    if (rc != ERR_OK) {
        c_show(rc);
        return;
    }
    c_show_medicine(&m);

    if (m.status != 1) {
        puts("该药品已经是停用状态。");
        return;
    }
    puts("停用后不能再开方，历史处方仍可正常显示。1 确认 / 0 取消");
    if (c_choice(1, &confirm) != 1 || confirm != 1) {
        puts("已取消。");
        return;
    }
    c_show(med_delete(id));
}

/* inbound = 1 入库；inbound = 0 出库 */
static void c_med_stock(int inbound)
{
    char id[21];
    int stock = 0;
    int qty;
    int rc;

    if (common_read_line("药品编号：", id, sizeof(id)) != ERR_OK) {
        puts("输入结束。");
        return;
    }
    if (!common_valid_text(id, 20, 1)) {
        puts("编号无效。");
        return;
    }
    rc = med_get_stock(id, &stock);
    if (rc != ERR_OK) {
        c_show(rc);
        return;
    }
    printf("当前库存：%d\n", stock);

    if (common_read_int(inbound ? "入库数量：" : "出库数量：", 1, C_MAX_QTY, &qty) != ERR_OK) {
        puts("数量输入无效。");
        return;
    }
    if (!inbound && qty > stock) {
        printf("库存只有 %d，不能出库 %d。\n", stock, qty);
        return;
    }

    rc = inbound ? med_stock_in(id, qty) : med_stock_out(id, qty);
    c_show(rc);
    if (rc == ERR_OK && med_get_stock(id, &stock) == ERR_OK) {
        printf("操作后库存：%d\n", stock);
    }
}

int menu_c_medicine(const UserSession *session)
{
    int opt, st;

    if (c_require(session) != ERR_OK) {
        return ERR_NO_PERMISSION;
    }
    if (session->role != ROLE_ADMIN) {
        puts("只有管理员可以使用药品管理。");
        return ERR_NO_PERMISSION;
    }

    for (;;) {
        puts("");
        puts("药品管理：1 盘点列表 / 2 新增药品 / 3 修改信息与调价 / 4 停用药品");
        puts("          5 入库 / 6 出库 / 0 返回");

        st = c_choice(6, &opt);
        if (st < 0) {
            return ERR_OK;
        }
        if (st == 0) {
            continue;
        }

        if (opt == 0) {
            return ERR_OK;
        } else if (opt == 1) {
            c_med_list();
        } else if (opt == 2) {
            c_med_add();
        } else if (opt == 3) {
            c_med_edit();
        } else if (opt == 4) {
            c_med_stop();
        } else if (opt == 5) {
            c_med_stock(1);
        } else if (opt == 6) {
            c_med_stock(0);
        }
    }
}

/* ---------------- M05 处方 ---------------- */

static int c_show_presc_detail(int presc_id)
{
    Prescription p;
    PrescDetail list[PRESC_MAX_DETAILS];
    double sum = 0.0;
    int rc, n, i;

    rc = presc_get_by_id(presc_id, &p);
    if (rc != ERR_OK) {
        c_show(rc);
        return rc;
    }

    puts("");
    printf("处方号：%d\n", p.presc_id);
    if (p.visit_id > 0) {
        printf("就诊号：%d\n", p.visit_id);
    }
    printf("患者：%s    医生：%s\n", p.patient_id, p.doctor_id);
    printf("开方时间：%s\n", p.create_time);
    printf("诊断：%s\n", (p.diagnosis[0] != '\0') ? p.diagnosis : "（未填写）");
    printf("状态：%s    总金额：%.2f 元\n",
           c_presc_status_text(p.status), p.total_amount);

    n = presc_get_details(presc_id, list, PRESC_MAX_DETAILS);
    if (n < 0) {
        c_show(n);
        return n;
    }
    if (n == 0) {
        puts("该处方没有药品明细。");
        return ERR_OK;
    }

    puts("明细：");
    puts("  药品编号  药品名称              数量   单价      小计");
    for (i = 0; i < n; ++i) {
        printf("  %-9s %-20s %4d %8.2f %9.2f",
               list[i].med_id, list[i].med_name, list[i].quantity,
               list[i].unit_price, list[i].subtotal);
        if (list[i].dosage[0] != '\0') {
            printf("   用法：%s", list[i].dosage);
        }
        putchar('\n');
        sum += list[i].subtotal;
    }
    printf("明细合计：%.2f 元\n", sum);
    return ERR_OK;
}

int menu_c_presc_view(const UserSession *session)
{
    Prescription list[C_LIST_MAX];
    int n, i, id, st;

    if (c_require(session) != ERR_OK) {
        return ERR_NO_PERMISSION;
    }

    if (session->role == ROLE_PATIENT) {
        n = presc_query_by_patient(session->user_id, list, C_LIST_MAX);
    } else if (session->role == ROLE_DOCTOR) {
        n = presc_query_by_doctor(session->user_id, list, C_LIST_MAX);
    } else if (session->role == ROLE_ADMIN) {
        n = presc_query_by_date("", "", list, C_LIST_MAX);
    } else {
        puts("当前角色不能查询处方。");
        return ERR_NO_PERMISSION;
    }

    if (n < 0) {
        c_show(n);
        return n;
    }
    if (n == 0) {
        puts("没有查到处方。");
        return ERR_OK;
    }

    puts("处方号 患者           医生     总金额     状态    开方时间");
    for (i = 0; i < n; ++i) {
        c_show_presc_row(&list[i]);
    }
    if (n == C_LIST_MAX) {
        printf("（只显示最近 %d 条）\n", C_LIST_MAX);
    }

    for (;;) {
        st = common_read_int("输入处方号查看明细，0 返回：", 0, 999999999, &id);
        if (st == B_ERR_INPUT) {
            return ERR_OK;
        }
        if (st != ERR_OK) {
            puts("输入无效。");
            continue;
        }
        if (id == 0) {
            return ERR_OK;
        }
        (void)c_show_presc_detail(id);
        return ERR_OK;
    }
}

/* 在“本人接诊记录”里找一条，确保 given 的编号确实属于当前医生 */
static int c_find_visit(const UserSession *session, int visit_id, VisitRecord *out)
{
    VisitRecord list[10];
    int n, i;

    n = visit_query_by_doctor(session, session->user_id, list, 10, 0);
    if (n < 0) {
        return n;
    }
    for (i = 0; i < n; ++i) {
        if (list[i].visit_id == visit_id) {
            *out = list[i];
            return ERR_OK;
        }
    }
    return ERR_NOT_FOUND;
}

static void c_presc_create(const UserSession *session)
{
    VisitRecord visits[10];
    VisitRecord chosen;
    PrescDetail items[PRESC_MAX_DETAILS];
    char diagnosis[PRESC_DIAG_LEN];
    char med_buf[PRESC_MED_ID_LEN];
    char dosage_buf[PRESC_DOSAGE_LEN];
    int n, i, rc, count = 0, presc_id = 0;
    int visit_id = 0;

    n = visit_query_by_doctor(session, session->user_id, visits, 10, 0);
    if (n < 0) {
        c_show(n);
        return;
    }
    if (n == 0) {
        puts("你还没有接诊记录，请先在“7 接诊与病历”里完成接诊。");
        return;
    }

    puts("最近的接诊记录：");
    for (i = 0; i < n; ++i) {
        printf("  就诊 %-4d 患者 %-14s 时间 %s\n",
               visits[i].visit_id, visits[i].patient_id, visits[i].visit_time);
    }

    if (common_read_int("选择就诊号：", 1, 999999999, &visit_id) != ERR_OK) {
        puts("输入结束。");
        return;
    }
    rc = c_find_visit(session, visit_id, &chosen);
    if (rc != ERR_OK) {
        puts("没有找到该接诊记录：只能给本人接诊过的患者开方。");
        return;
    }

    if (common_read_line("诊断（可留空，不要输入英文单引号）：",
                         diagnosis, sizeof(diagnosis)) != ERR_OK) {
        puts("输入结束。");
        return;
    }
    if (!common_valid_text(diagnosis, 200, 0)) {
        puts("诊断超长或含非法字符。");
        return;
    }

    memset(items, 0, sizeof(items));

    while (count < PRESC_MAX_DETAILS) {
        Medicine med;

        printf("\n第 %d 条药品编号（直接回车结束）：", count + 1);
        fflush(stdout);
        if (common_read_line(NULL, med_buf, sizeof(med_buf)) != ERR_OK) {
            puts("\n输入结束，已取消开方。");
            return;
        }
        if (med_buf[0] == '\0') {
            break;
        }
        if (!common_valid_text(med_buf, 20, 1)) {
            puts("药品编号不合法。");
            continue;
        }

        for (i = 0; i < count; ++i) {
            if (strcmp(items[i].med_id, med_buf) == 0) {
                break;
            }
        }
        if (i < count) {
            puts("这个药品已经加过了，同一张处方不能重复，请换一个。");
            continue;
        }

        rc = med_get_by_id(med_buf, &med);
        if (rc != ERR_OK) {
            puts("药品不存在。");
            continue;
        }
        if (med.status != 1) {
            puts("该药品已停用，不能开方。");
            continue;
        }
        printf("    %s  规格 %s  单价 %.2f 元  库存 %d\n",
               med.med_name, med.spec, med.price, med.stock);

        if (common_read_int("    数量：", 1, C_MAX_QTY, &items[count].quantity) != ERR_OK) {
            puts("数量无效。");
            continue;
        }
        if (items[count].quantity > med.stock) {
            printf("    库存只有 %d，数量不能超过库存。\n", med.stock);
            continue;
        }

        if (common_read_line("    用法用量（可留空）：",
                             dosage_buf, sizeof(dosage_buf)) != ERR_OK) {
            puts("输入结束，已取消开方。");
            return;
        }
        if (!common_valid_text(dosage_buf, 100, 0)) {
            puts("用法用量超长或含非法字符。");
            continue;
        }

        common_copy(items[count].med_id, sizeof(items[count].med_id), med_buf);
        common_copy(items[count].dosage, sizeof(items[count].dosage), dosage_buf);
        ++count;
    }

    if (count == 0) {
        puts("没有选择任何药品，已取消开方。");
        return;
    }

    rc = presc_add(chosen.visit_id, chosen.patient_id, session->user_id,
                   diagnosis, items, count, &presc_id);
    if (rc == ERR_OK) {
        printf("开方成功：处方号 %d，共 %d 种药品，总金额 %.2f 元。\n",
               presc_id, count, presc_calc_total(items, count));
        return;
    }

    switch (rc) {
    case ERR_STOCK_SHORT:
        puts("有药品库存不足，整单已回滚，数据库没有写入任何数据。");
        break;
    case ERR_NOT_FOUND:
        puts("有药品不存在或已停用，整单已回滚。");
        break;
    case ERR_PARAM:
        puts("输入内容不合法（常见原因：诊断里有英文单引号），整单已回滚。");
        break;
    default:
        c_show(rc);
        break;
    }
}

/* do_return = 1 退药；do_return = 0 作废 */
static void c_presc_change(const UserSession *session, int do_return)
{
    Prescription p;
    int id, rc, confirm;

    if (common_read_int("处方号：", 1, 999999999, &id) != ERR_OK) {
        puts("输入结束。");
        return;
    }
    rc = presc_get_by_id(id, &p);
    if (rc != ERR_OK) {
        c_show(rc);
        return;
    }
    c_show_presc_row(&p);

    if (strcmp(p.doctor_id, session->user_id) != 0) {
        puts("只能处理本人开具的处方。");
        return;
    }

    if (do_return) {
        if (p.status == PRESC_STATUS_CANCEL) {
            puts("该处方已作废，不能退药。");
            return;
        }
        if (p.status == PRESC_STATUS_RETURN) {
            puts("该处方已经退过药。");
            return;
        }
        puts("退药会把每种药品的数量加回库存。1 确认 / 0 取消");
    } else {
        if (p.status == PRESC_STATUS_CANCEL) {
            puts("该处方已经作废。");
            return;
        }
        if (p.status == PRESC_STATUS_RETURN) {
            puts("已退药的处方不能再作废。");
            return;
        }
        puts("作废后处方不可再使用，且不返还库存。1 确认 / 0 取消");
    }

    if (c_choice(1, &confirm) != 1 || confirm != 1) {
        puts("已取消。");
        return;
    }
    c_show(do_return ? presc_return(id) : presc_cancel(id));
}

int menu_c_prescription(const UserSession *session)
{
    int opt, st;

    if (c_require(session) != ERR_OK) {
        return ERR_NO_PERMISSION;
    }
    if (session->role != ROLE_DOCTOR) {
        puts("只有医生可以开方、作废和退药。");
        return ERR_NO_PERMISSION;
    }

    for (;;) {
        puts("");
        puts("处方管理：1 开处方 / 2 本人处方与明细 / 3 作废处方 / 4 退药 / 0 返回");

        st = c_choice(4, &opt);
        if (st < 0) {
            return ERR_OK;
        }
        if (st == 0) {
            continue;
        }

        if (opt == 0) {
            return ERR_OK;
        } else if (opt == 1) {
            c_presc_create(session);
        } else if (opt == 2) {
            (void)menu_c_presc_view(session);
        } else if (opt == 3) {
            c_presc_change(session, 0);
        } else if (opt == 4) {
            c_presc_change(session, 1);
        }
    }
}