#ifndef MENU_C_H
#define MENU_C_H

#include "session.h"

/* 管理员：药品管理。返回 ERR_OK 或负错误码。 */
int menu_c_medicine(const UserSession *session);

/* 医生：处方管理（开方 / 查看 / 作废 / 退药）。 */
int menu_c_prescription(const UserSession *session);

/* 任意角色：按身份列出处方，并可查看某张处方的药品明细。 */
int menu_c_presc_view(const UserSession *session);

#endif