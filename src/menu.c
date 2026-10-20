#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "menu.h"
#include "menu_c.h"
#include "member_b.h"
#include "appointment.h"
#include "patient.h"
#include "medicine.h"
#include "prescription.h"
#include "member_a_internal.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

// 隐藏回显并读取密码
static int menu_read_password(char *buffer, size_t capacity)
{
    char *newline;
    int ch,ok;
#ifdef _WIN32
    HANDLE handle;
    DWORD original_mode;
#else
    struct termios original_mode,hidden_mode;
#endif
    if (buffer == NULL || capacity<2 || capacity>INT_MAX) { return 0; }
    buffer[0]='\0';
#ifdef _WIN32
    handle=GetStdHandle(STD_INPUT_HANDLE);
    if (!GetConsoleMode(handle,&original_mode) ||
        !SetConsoleMode(handle,original_mode & ~ENABLE_ECHO_INPUT)) { return 0; }
#else
    if (tcgetattr(STDIN_FILENO,&original_mode)!=0) { return 0; }
    hidden_mode=original_mode;
    hidden_mode.c_lflag &= ~(ECHO|ECHONL);
    if (tcsetattr(STDIN_FILENO,TCSANOW,&hidden_mode)!=0) { return 0; }
#endif
    fflush(stdout);
    ok=fgets(buffer,(int)capacity,stdin)!=NULL;
    if (ok) {
        newline=strchr(buffer,'\n');
        if (newline == NULL) {
            while ((ch=getchar())!='\n' && ch!=EOF) { }
            ok=0;
        } else { *newline='\0'; }
    }
#ifdef _WIN32
    if (!SetConsoleMode(handle,original_mode)) { ok=0; }
#else
    if (tcsetattr(STDIN_FILENO,TCSANOW,&original_mode)!=0) { ok=0; }
#endif
    putchar('\n');
    return ok && buffer[0]!='\0';
}

// 提示输入文本并检查长度
static int read_text(const char *prompt, char *out, size_t capacity, int required)
{
    char input[1024],*newline;
    int ch;
    fputs(prompt,stdout); fflush(stdout);
    if (fgets(input,sizeof input,stdin)==NULL) { return 0; }
    newline=strchr(input,'\n');
    if (newline == NULL) {
        while ((ch=getchar())!='\n' && ch!=EOF) { }
        puts("输入过长，本次操作取消。"); return 0;
    }
    *newline='\0';
    if (strlen(input)>=capacity || (required && input[0]=='\0')) {
        puts("输入为空或超过字段长度，本次操作取消。"); return 0;
    }
    memcpy(out,input,strlen(input)+1);
    return 1;
}

// 读取菜单选项
static int choice(void)
{
    char input[32];
    int value;
    if (!read_text("选择：",input,sizeof input,1)) { return feof(stdin) ? -1 : -2; }
    if (a_parse_int(input,0,INT_MAX,&value)!=ERR_OK) { return -2; }
    return value;
}

// 根据返回码显示操作结果
static void show_result(int rc)
{
    switch (rc) {
    case ERR_OK: puts("操作成功。"); break;
    case ERR_PARAM: case AUTH_ERR_PARAM: puts("输入格式不正确。"); break;
    case ERR_DB_DUP: puts("账号、身份证号或有效预约重复。"); break;
    case ERR_NOT_FOUND: puts("没有符合条件的记录，或预约不属于本人/已不能取消。"); break;
    case AUTH_ERR_DISABLED: puts("账号已停用，拒绝操作。"); break;
    case AUTH_ERR_LOCKED: puts("账号已锁定，请十分钟后重试。"); break;
    case AUTH_ERR_INVALID_CREDENTIALS: puts("账号或密码不正确。"); break;
    case AUTH_ERR_PASSWORD_SETUP: puts("旧账号尚未设置安全密码，请由数据库管理员初始化。"); break;
    case AUTH_ERR_FORBIDDEN: case AUTH_ERR_NOT_LOGGED_IN: case ERR_NO_PERMISSION:
        puts("尚未登录或没有操作权限。"); break;
    case ERR_IO: puts("文件无法创建或写入；已有文件不会被覆盖。"); break;
    case ERR_INVALID_STATE: puts("目标状态不允许此操作。"); break;
    default: printf("操作失败，错误码 %d。请查看数据库配置及升级脚本是否已应用。\n",rc);
    }
}

// 两次输入并确认新密码
static int new_password(char *out, size_t capacity)
{
    char confirmation[AUTH_PASSWORD_MAX_BYTES+2]={0};
    int ok;
    fputs("新密码（不回显）：",stdout);
    ok=menu_read_password(out,capacity);
    if (ok) {
        fputs("再次输入：",stdout);
        ok=menu_read_password(confirmation,sizeof confirmation) && strcmp(out,confirmation)==0;
    }
    auth_secure_clear(confirmation,sizeof confirmation);
    if (!ok) { auth_secure_clear(out,capacity); puts("密码输入失败或两次不一致。"); }
    return ok;
}

// 读取登录信息并调用认证
AuthResult menu_login(UserSession *session)
{
    char username[USERNAME_CAPACITY],password[AUTH_PASSWORD_MAX_BYTES+2]={0};
    int role;
    AuthResult rc=AUTH_ERR_NOT_LOGGED_IN;
    if (session == NULL || session_is_logged_in(session)) { return AUTH_ERR_PARAM; }
    puts("登录角色：1 患者 / 2 医生 / 3 管理员 / 0 返回");
    role=choice();
    if (role<1 || role>3) { return role==0 || role==-1 ? rc : AUTH_ERR_PARAM; }
    if (!read_text("账号：",username,sizeof username,1)) { return AUTH_ERR_PARAM; }
    fputs("密码（不回显）：",stdout);
    if (menu_read_password(password,sizeof password)) {
        rc=auth_login(session,(UserRole)role,username,password);
    }
    auth_secure_clear(password,sizeof password);
    return rc;
}

// 读取患者基本资料
static int read_profile(Patient *patient)
{
    char age[16];
    return read_text("姓名：",patient->name,sizeof patient->name,1) &&
        read_text("性别（可留空）：",patient->gender,sizeof patient->gender,0) &&
        read_text("年龄（未知填 0）：",age,sizeof age,1) &&
        a_parse_int(age,0,INT_MAX,&patient->age)==ERR_OK &&
        read_text("身份证号：",patient->id_card,sizeof patient->id_card,1) &&
        read_text("电话（可留空）：",patient->phone,sizeof patient->phone,0) &&
        read_text("地址（可留空）：",patient->address,sizeof patient->address,0);
}

// 读取资料后注册或新增患者
static void register_patient(const UserSession *admin)
{
    Patient patient={0};
    char password[AUTH_PASSWORD_MAX_BYTES+2]={0};
    int rc;
    if (!read_profile(&patient) ||
        !read_text("登录账号：",patient.username,sizeof patient.username,1) ||
        !new_password(password,sizeof password)) { auth_secure_clear(password,sizeof password); return; }
    rc=admin == NULL ? patient_register(&patient,password) : patient_admin_add(admin,&patient,password);
    auth_secure_clear(password,sizeof password);
    show_result(rc);
    if (rc==ERR_OK) { printf("患者编号：%s\n",patient.patient_id); }
}

// 显示患者资料
static void show_patient(const Patient *patient)
{
    printf("编号：%s  姓名：%s  性别：%s  年龄：%d\n"
           "账号：%s  状态：%d  注册时间：%s\n电话：%s\n地址：%s\n",
        patient->patient_id,patient->name,patient->gender,patient->age,
        patient->username,patient->status,patient->create_time,patient->phone,patient->address);
}

// 读取原密码和新密码后修改密码
static void change_password(const UserSession *session)
{
    char old_password[AUTH_PASSWORD_MAX_BYTES+2]={0},password[AUTH_PASSWORD_MAX_BYTES+2]={0};
    fputs("原密码（不回显）：",stdout);
    if (menu_read_password(old_password,sizeof old_password) && new_password(password,sizeof password)) {
        show_result(auth_change_password(session,old_password,password));
    }
    auth_secure_clear(old_password,sizeof old_password);
    auth_secure_clear(password,sizeof password);
}

// 显示预约列表或查询错误
static void show_appointments(const Appointment *list, int count)
{
    if (count<0) { show_result(count); return; }
    puts("预约ID | 患者 | 医生 | 日期 | 时段 | 状态(0待就诊/1已就诊/2取消)");
    for (int i=0;i<count;++i) {
        printf("%d | %s | %s | %s | %s | %d\n",list[i].appoint_id,list[i].patient_id,
            list[i].doctor_id,list[i].appoint_date,list[i].time_slot,list[i].status);
    }
    printf("本次显示 %d 条。\n",count);
}

// 分页显示本人预约
void menu_show_my_appointments(const UserSession *session)
{
    Appointment list[20];
    int offset=0,count;
    do {
        count=appointment_query_my_page(session,list,20,offset);
        show_appointments(list,count);
        if (count<20 || offset>INT_MAX-20) { return; }
        puts("1 下一页 / 0 返回"); offset+=20;
    } while (choice()==1);
}

// 取消预约并显示结果
void menu_cancel_my_appointment(const UserSession *session, int appoint_id)
{
    show_result(appointment_cancel_my(session,appoint_id));
}

// 输入预约编号后取消预约
void menu_prompt_cancel_my_appointment(const UserSession *session)
{
    char input[32];
    int id;
    if (read_text("要取消的预约ID：",input,sizeof input,1) &&
        a_parse_int(input,1,INT_MAX,&id)==ERR_OK) { menu_cancel_my_appointment(session,id); }
}

// 分页显示可预约医生
static void list_doctors(const UserSession *session)
{
    AppointmentDoctor list[20];
    int offset=0,count;
    do {
        count=appointment_query_available_doctors(session,list,20,offset);
        if (count<0) { show_result(count); return; }
        for (int i=0;i<count;++i) {
            printf("%s | %s | %s | %s\n",list[i].doctor_id,list[i].name,list[i].dept,list[i].title);
        }
        if (count<20 || offset>INT_MAX-20) { return; }
        puts("1 下一页 / 0 返回"); offset+=20;
    } while (choice()==1);
}

// 读取预约信息并提交挂号
static void book(const UserSession *session)
{
    char doctor[11],date[11],slot[41],symptom[801];
    int id,rc;
    if (!read_text("医生编号（先在有效医生列表中选择）：",doctor,sizeof doctor,1) ||
        !read_text("预约日期 YYYY-MM-DD：",date,sizeof date,1) ||
        !read_text("时段（最多10字符）：",slot,sizeof slot,1) ||
        !read_text("症状描述（可留空）：",symptom,sizeof symptom,0)) { return; }
    rc=appointment_book(session,doctor,date,slot,symptom,&id);
    show_result(rc);
    if (rc==ERR_OK) { printf("预约ID：%d\n",id); }
}

// 显示患者预约菜单
void menu_patient_appointments(const UserSession *session)
{
    int option;
    for (;;) {
        puts("预约：1 本人预约 / 2 取消预约 / 3 挂号 / 4 有效医生 / 0 返回");
        option=choice();
        if (option==0 || option==-1) { return; }
        switch (option) {
        case 1: menu_show_my_appointments(session); break;
        case 2: menu_prompt_cancel_my_appointment(session); break;
        case 3: book(session); break;
        case 4: list_doctors(session); break;
        default: puts("无效选择。");
        }
    }
}

// 调用药品接口显示药品列表
static void list_medicines(const UserSession *session)
{
    Medicine list[20];
    int count=auth_require_active(session);
    if (count!=AUTH_OK) { show_result(count); return; }
    count=med_query_all(list,20);
    if (count<0) { show_result(count); return; }
    for (int i=0;i<count;++i) {
        printf("%s | %s | %.2f | 库存%d | 状态%d\n",list[i].med_id,list[i].med_name,
            list[i].price,list[i].stock,list[i].status);
    }
    if (count==20) { puts("C 的现有列表接口本次最多显示20条。"); }
}

// 根据当前角色查询并显示处方
static void list_prescriptions(const UserSession *session)
{
    Prescription list[20];
    int count=auth_require_active(session);
    if (count!=AUTH_OK) { show_result(count); return; }
    if (session->role==ROLE_PATIENT) {
        count=presc_query_by_patient(session->user_id,list,20);
    } else if (session->role==ROLE_DOCTOR) {
        count=presc_query_by_doctor(session->user_id,list,20);
    } else if (session->role==ROLE_ADMIN) {
        char start[11],end[11];
        if (!read_text("起始日期 YYYY-MM-DD：",start,sizeof start,1) ||
            !read_text("结束日期 YYYY-MM-DD：",end,sizeof end,1) ||
            !a_date_valid(start) || !a_date_valid(end) || strcmp(start,end)>0) { show_result(ERR_PARAM); return; }
        count=presc_query_by_date(start,end,list,20);
    } else { show_result(AUTH_ERR_FORBIDDEN); return; }
    if (count<0) { show_result(count); return; }
    for (int i=0;i<count;++i) {
        printf("处方%d | 患者%s | 医生%s | %.2f | 状态%d | %s\n",list[i].presc_id,
            list[i].patient_id,list[i].doctor_id,list[i].total_amount,list[i].status,list[i].create_time);
    }
    if (count==20) { puts("C 的现有列表接口本次最多显示20条。"); }
}

// 显示患者功能菜单
static void patient_menu(const UserSession *session)
{
    int option;
    for (;;) {
        puts("患者：1 本人档案 / 2 修改联系方式 / 3 预约挂号 / 4 修改密码 / 5 药品 / 6 本人处方 / 7 就诊与病历 / 0 退出登录");
        option=choice();
        if (option==0 || option==-1) { return; }
        if (option==1) {
            Patient patient; int rc=patient_get_my(session,&patient);
            if (rc==ERR_OK) { show_patient(&patient); } else { show_result(rc); }
        } else if (option==2) {
            char phone[21],address[801];
            if (read_text("新电话（可留空）：",phone,sizeof phone,0) &&
                read_text("新地址（可留空）：",address,sizeof address,0)) {
                show_result(patient_update_my_contact(session,phone,address));
            }
        } else if (option==3) { menu_patient_appointments(session); }
        else if (option==4) { change_password(session); }
        else if (option==5) { list_medicines(session); }
        else if (option==6) { (void)menu_c_presc_view(session); }
        else if (option==7) { menu_member_b(session); }
        else { puts("无效选择。"); }
    }
}

// 显示管理员患者管理菜单
static void admin_patients(const UserSession *session)
{
    int option;
    for (;;) {
        puts("患者管理：1 新增 / 2 按编号查询 / 3 列表 / 4 修改档案 / 5 注销 / 6 导出 / 0 返回");
        option=choice();
        if (option==0 || option==-1) { return; }
        if (option==1) { register_patient(session); }
        else if (option==2 || option==4 || option==5) {
            char id[21]; Patient patient;
            int rc;
            if (!read_text("患者编号：",id,sizeof id,1)) { continue; }
            rc=patient_admin_query(session,id,&patient);
            if (rc!=ERR_OK) { show_result(rc); continue; }
            show_patient(&patient);
            if (option==4 && read_profile(&patient)) { show_result(patient_admin_update(session,&patient)); }
            if (option==5) {
                puts("注销后不能登录和挂号。1 确认注销 / 0 取消");
                if (choice()==1) { show_result(patient_deactivate(session,id)); }
            }
        } else if (option==3) {
            Patient list[20]; int offset=0,count;
            do {
                count=patient_query_all(session,list,20,offset);
                if (count<0) { show_result(count); break; }
                for (int i=0;i<count;++i) { show_patient(&list[i]); }
                if (count<20 || offset>INT_MAX-20) { break; }
                puts("1 下一页 / 0 返回"); offset+=20;
            } while (choice()==1);
        } else if (option==6) {
            char path[801]; int exported,rc;
            if (!read_text("新文本文件路径（已有文件不会覆盖）：",path,sizeof path,1)) { continue; }
            rc=patient_export(session,path,&exported); show_result(rc);
            if (rc==ERR_OK) { printf("已导出%d条，不含密码和身份证号。\n",exported); }
        } else { puts("无效选择。"); }
    }
}

// 显示医生或管理员功能菜单
static void staff_menu(const UserSession *session)
{
    int option;
    for (;;) {
        if (session->role==ROLE_ADMIN) {
            puts("管理员：1 患者管理 / 2 全部预约 / 3 修改密码 / 4 药品 / 5 处方 / 6 药品管理 / 7 医生、就诊与病历 / 0 退出登录");
        } else {
            puts("医生：1 今日待就诊 / 3 修改密码 / 4 药品 / 5 本人处方 / 6 处方管理 / 7 接诊与病历 / 0 退出登录");
        }
        option=choice();
        if (option==0 || option==-1) { return; }
        if (session->role==ROLE_ADMIN && option==1) { admin_patients(session); }
        else if ((session->role==ROLE_ADMIN && option==2) ||
                 (session->role==ROLE_DOCTOR && option==1)) {
            Appointment list[20];
            int offset=0,count;
            do {
                count=session->role==ROLE_ADMIN ? appointment_query_all(session,list,20,offset)
                    : appointment_query_doctor_today_page(session,list,20,offset);
                show_appointments(list,count);
                if (count<20 || offset>INT_MAX-20) { break; }
                puts("1 下一页 / 0 返回"); offset+=20;
            } while (choice()==1);
        } else if (option==3) { change_password(session); }
        else if (option==4) { list_medicines(session); }
        else if (option==5) { list_prescriptions(session); }
        else if (option==6) {
            if (session->role==ROLE_ADMIN) { (void)menu_c_medicine(session); }
            else { (void)menu_c_prescription(session); }
        }
        else if (option==7) { menu_member_b(session); }
        else { puts("无效选择。"); }
    }
}

// 运行主菜单并按角色分流
int menu_application(void)
{
    UserSession session;
    session_init(&session);
    for (;;) {
        int option;
        puts("社区医疗系统：1 登录 / 2 患者注册 / 0 退出");
        option=choice();
        if (option==0 || option==-1) { return 0; }
        if (option==2) { register_patient(NULL); continue; }
        if (option!=1) { puts("无效选择。"); continue; }
        {
            AuthResult rc=menu_login(&session);
            if (rc!=AUTH_OK) { show_result(rc); continue; }
        }
        printf("登录成功：%s\n",session.username);
        if (session.role==ROLE_PATIENT) { patient_menu(&session); }
        else { staff_menu(&session); }
        session_logout(&session);
    }
}

// 显示旧账号密码初始化入口
int menu_initialize_password(void)
{
    char username[USERNAME_CAPACITY],password[AUTH_PASSWORD_MAX_BYTES+2]={0};
    int role,result;
    puts("数据库管理员初始化旧账号密码：只接受尚未使用安全哈希的账号。");
    puts("角色：1 患者 / 2 医生 / 3 管理员 / 0 取消");
    role=choice();
    if (role==0 || role==-1) { return AUTH_ERR_NOT_LOGGED_IN; }
    if (role<1 || role>3 || !read_text("现有账号：",username,sizeof username,1) ||
        !new_password(password,sizeof password)) {
        auth_secure_clear(password,sizeof password); return AUTH_ERR_PARAM;
    }
    result=auth_initialize_password((UserRole)role,username,password);
    show_result(result);
    auth_secure_clear(password,sizeof password);
    return result;
}
