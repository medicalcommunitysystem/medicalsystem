#ifndef COMMON_H
#define COMMON_H

#define ERR_OK             0   

#define ERR_PARAM         -1   
#define ERR_NOMEM         -2   


#define ERR_DB_CONN       -3   
#define ERR_DB_EXEC       -4   
#define ERR_DB_RESULT     -5   
#define ERR_DB_BEGIN      -6   
#define ERR_DB_COMMIT     -7   
#define ERR_DB_ROLLBACK   -8   

#define MYSQL_ER_DUP_ENTRY      1062   /* 主键/唯一键冲突 */
#define MYSQL_ER_NO_REFERENCED  1452   /* 外键约束：父记录不存在 */
#define MYSQL_ER_ROW_IS_REFERENCED 1451 /* 外键约束：被子表引用，无法删除 */

#endif 