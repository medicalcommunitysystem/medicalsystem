#ifndef MENU_H
#define MENU_H

#include "session.h"
#include "auth.h"

// 读取登录信息并调用认证
AuthResult menu_login(UserSession *session);
// 分页显示本人预约
void menu_show_my_appointments(const UserSession *session);
// 取消预约并显示结果
void menu_cancel_my_appointment(const UserSession *session,int appoint_id);
// 输入预约编号后取消预约
void menu_prompt_cancel_my_appointment(const UserSession *session);
// 显示患者预约菜单
void menu_patient_appointments(const UserSession *session);
// 运行主菜单并按角色分流
int menu_application(void);
// 显示旧账号密码初始化入口
int menu_initialize_password(void);
#endif
