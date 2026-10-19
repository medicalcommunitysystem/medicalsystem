# 成员 B：接入 submission/member-a 的版本

本包以 `submission/member-a` 提交 `b9cd81942a87c8ccd9f5ca34c475dee3d2516529` 为基线。
完成医生管理、接诊、就诊查询、病历管理和最终 Linux 构建，并将入口接入 A 的三类角色菜单。

## 当前用户的操作顺序

你已完成：克隆到 `~/member_a`、建立 `submission/member-b`、编译 A 代码、应用 A 的数据库升级。
现在使用本版补丁，不使用之前基于旧 main 的源码包。

1. 将 `member_b_for_member_a.patch` 下载到 Windows 下载目录。
2. 在 WSL 执行（假定 Windows 用户名仍为 srj66）：

   ```bash
   cd ~/member_a
   cp /mnt/c/Users/srj66/Downloads/member_b_for_member_a.patch /tmp/member_b_for_member_a.patch
   git status -sb
   git apply --check /tmp/member_b_for_member_a.patch
   ```

   下载目录不同时调整 cp 的来源路径。`git apply --check` 无输出且退出成功表示可以合入。
   若报错先停止，不使用强制覆盖或重置分支。

3. 检查通过后应用：

   ```bash
   git apply /tmp/member_b_for_member_a.patch
   git status -sb
   ```

   这些是本地文件修改，不会自动提交、推送或修改远程分支。

4. 升级 B 的数据库结构。使用新的备份名，避免覆盖你之前 A 升级前的备份：

   ```bash
   sudo mysqldump medical_system > ~/medical_system_before_b_$(date +%Y%m%d_%H%M%S).sql
   sudo mysql medical_system < sql/member_b_upgrade.sql
   sudo mysql medical_system -e "SELECT version FROM t_member_b_schema;"
   ```

   每条命令成功后再执行下一条。标记应是 `1`。升级不删除业务表；若旧数据重复，会报告冲突并停止。
   原团队的 `init_db.sql` 会清表，不要重新导入。

5. 在 WSL 构建最终系统：

   ```bash
   make
   ```

   不再使用 `make -f Makefile.member-a`，该文件是 A 的原始构建入口，没有 B 源码。
   默认编译器为 `gcc-11`，依赖 MySQL C 开发库和 OpenSSL 开发库。

6. 首次运行需要为旧的明文管理员账号初始化安全密码：

   ```bash
   sudo bash scripts/run-linux.sh --initialize-password
   ```

   角色选择 `3 管理员`，账号输入 `admin`，两次设置你自己选择的新密码。
   这复用 A 的管理员初始化工具，只接受尚未使用安全哈希格式的账号。
   如果提示已初始化，不要覆盖，直接使用已设置的业务密码登录。

7. 启动完整菜单：

   ```bash
   sudo bash scripts/run-linux.sh
   ```

   `sudo` 密码是 Ubuntu 用户密码；菜单输入的是业务账号密码，两者不同。
   启动脚本默认使用本机 Unix socket 的数据库 root 账号，适合你当前 Ubuntu 开发环境。
   如果改用密码数据库账号，可设置 `MEDICAL_DB_HOST/USER/PASSWORD/NAME/PORT`，与 A 的 main.c 规则一致。
   不要把数据库密码写入源码、提交到 Git 或放到命令行参数中。

## 菜单验收

登录后，角色菜单新增 `7` 号入口：

- 管理员：`7 医生、就诊与病历`。
- 医生：`7 接诊与病历`。
- 患者：`7 就诊与病历`。

B 子菜单：

| 编号 | 功能 | 角色 |
|---|---|---|
| 1 | 医生列表 | 管理员全部；患者只看在职；医生只看本人 |
| 2 | 就诊记录 | 管理员全部；患者本人；医生本人接诊 |
| 3 | 病历查询 | 管理员全部；患者本人；医生本人书写 |
| 4 | 新增医生 | 管理员 |
| 5 | 修改医生资料 | 管理员；医生本人 |
| 6 | 停用医生 | 管理员 |
| 7 | 当日待接诊 | 医生 |
| 8 | 接诊 | 医生 |
| 9 | 新增病历 | 医生 |
| 10 | 修改病历 | 医生本人、创建后二十四小时内 |
| 0 | 返回 A 的上级菜单 | 当前角色 |

建议先用管理员新建医生，再通过 A 的患者注册/挂号操作产生当日预约，最后用医生进入 B 子菜单接诊、写病历。
患者和管理员随后查询对应记录。医生职称与科室只能由管理员修改。

## 与其他成员的兼容处理

- 直接使用 A 的 `UserSession`，角色值保持患者=1、医生=2、管理员=3。
- 新医生密码调用 A 的 `auth_hash_password()`；登录、改密、失败五次锁定十分钟都沿用 A 的实现。
- 账号同步和跨角色唯一约束沿用 A 的 `t_auth_account` 及触发器，不另建第二套认证表。
- 业务 SQL 使用 A 的 `db_query_params()` 和 `db_execute_params()`，没有覆盖 `src/db.c` 或 `include/db.h`。
- B 的 `b_db.c` 仅将参数和返回结果适配为本模块内部结构，所有操作仍使用同一数据库连接及事务。
  LIMIT/OFFSET 只格式化已验证的 C 整数，其他数据通过真实参数绑定。
- 新增 B 错误码使用 -200 段，避开 A 的 -100 段。
- `appointment_take()` 与 `visit_add()` 共用唯一接诊实现，不会先单独把预约标记为已就诊。
- 保留 A 的患者、挂号、认证和 C 的药品、处方源码。A 的菜单和 main.c 只增加 B 入口及完成标记检查。
- C 的写操作业务接口存在，但原 A 菜单尚未提供开方、库存维护等完整写操作界面。
  本交付没有重写 C 的模块；全流程测试会直接调用 C 的业务接口验证衔接。

## 关键规则

- 一次预约最多一条就诊，一次就诊最多一条病历，由唯一索引保证。
- 接诊仅允许当日、待就诊且属于当前医生的预约；患者必须仍有效。
- 插入就诊和更新预约状态处于同一事务，任一步失败都回滚。
- 病历使用固定 create_time 判定修改期；update_time 不会延长二十四小时限制。
- 旧病历没有可靠 create_time 时，升级会保守地设为已过修改期，保留旧 update_time。
- 医生编号 D001～D999，超过范围明确报错。旧编号和软删除编号不复用。
- 主诉必填、现病史可空，输入最多各二千个字符；中文按 Unicode 字符数验证。
- 过敏史必须提供字符串，空字符串代表无过敏，NULL 代表未提供并拒绝。
- 体温 25～45、心率 1～300，血压要求如 120/80，收缩压 40～300、舒张压 20～200 且前者大于后者。
  这些是输入校验，不是医疗建议。

## 库与构建

最终 `Makefile` 生成：

- `build/medical_system`：一个统一入口的完整程序。
- `build/libdb.so`：A/C 共用的数据库动态库。
- `build/libcommon.a`：公共校验、输入函数和 A 的基础文本处理。
- `build/libmember_b.a`：B 的业务和菜单。

动态库从可执行文件所在目录查找。头文件依赖使用 `-MMD -MP` 自动跟踪。
`make check` 只编译系统和测试，不自动改数据库。

## 自动测试

```bash
make check
sudo python3 tools/test_member_b.py
```

测试工具只创建新的 `member_b_test_*` 临时库，依次导入初始结构、A 升级和 B 升级，并重复执行升级验证幂等性。
测试结束保留该库供检查，不触碰现有 medical_system。
一百一十二项检查覆盖 B 的权限、事务、病历时限，以及 A 注册挂号、B 接诊、C 开方、B 病历、患者查询的业务接口流程。

本次自动验证使用 Windows GCC 13.2 和独立 MySQL 8.0.42；你的 WSL GCC 11 / MySQL 8.4 构建与交互验收需要继续执行。
Windows 验证不能替代 Linux 共享库加载验收。详细结果见 TEST_REPORT_MEMBER_B.md。

## 提交前

先查看 `git diff --stat` 和 `git diff`，确认变更符合 B 的范围。
此包不会自动执行 git add、commit、push 或创建 PR；按小组约定完成验收后再提交。
