# 项目变更历史

> 本文件归档 `AI项目上下文交接文档.md` 的持续更新记录。**追溯"上次改了什么、为什么、沿革如何"时阅读**；头部为最新记录索引，命中索引后再按日期段翻阅。新增变更时追加到对应日期段末尾。

## 最新记录索引

- 2026-09-06：m2006_lib 独立为 git 仓库（GitHub `GaGiaa/m2006_lib`，public，develop，首提交 `6e203b6`）并以 submodule 接入本工程（gitlink `5df2024`）；交接文档分层重构（本文件与 `docs/m2006_hardware.md` 拆分）。
- 2026-09-06：M2006 全部逻辑抽为复用库 `Lib\m2006_lib` 并迁移本工程 App 调用（`bd995ca`）。
- 2026-09-05：PID 库独立为 git 仓库（`GaGiaa/pid_lib`），本工程 `Lib\pid_lib` 切 submodule 并切换远程 URL（`9ebdf96`）。

## 2026 年 9 月 6 日

- 将 M2006 全部逻辑抽为复用库 `Lib\m2006_lib`（include/src/tests 与 pid_lib 同构，纯 C 零 HAL）：m2006_protocol（协议层，ID 上限 4→8，控制帧 0x200/0x1FF）、m2006_motor（电机实例：透明结构体，合并原 driver 安全门与原 control 级联闭环，tick 由调用者传入，中断共享字段 volatile）、m2006_bus（总线实例：8 槽位注册、两遍式聚合打包 0x200/0x1FF、反馈分发）。
- 本工程迁移：新增 `App\Inc\driver\m2006_hal.h/.c`（FDCAN2 滤波/中断取帧分发/发送，持有 m2006_hal_bus）；重写 `App\Src\task\m2006_control_task.c`（实例化 m2006_motor + 编排"闭环→打包→发送"，freertos.c 任务创建接口不变）；删除 App 下 m2006_protocol/driver/control 旧文件与 tests 两个旧 m2006 测试；uvprojx include path 加 `../Lib/m2006_lib/include`、源文件替换为库三源 + m2006_hal.c；命名检查器与单测 fixture 同步更新（m2006_lib 文件纳入检查）。
- 库主机端测试：protocol 15 用例、motor 37 断言、bus 48 断言全部 PASS（bus 打包修复两遍式帧序问题与 esc7 偏移断言）；命名检查 PASS、命名单测 19/19、Keil 完整重建 0 Error 0 Warning。
- 迁移决策（用户明确）：库获得完整重构代码、本工程 App 旧代码删除并改为调用库、先不建独立 git 仓库、不接 submodule。
- 【本次】m2006_lib 独立化 + submodule 接入：GitHub 创建 `GaGiaa/m2006_lib`（public，默认分支 develop，首提交 `6e203b6`，11 文件 = 库三源 + tests + 新增 .gitignore/README）；主仓库 `git rm --cached` 移除 blob 跟踪、.gitmodules 注册子模块、提交 gitlink（`5df2024`）；`git submodule status` 两个子模块（pid_lib/m2006_lib）均正常，工作区文件保留（Keil 路径不受影响）。push 时 GitHub 443 直连被网络干扰，经本机 Clash Verge（127.0.0.1:7897）代理完成推送（`git -c http.proxy=...` 临时参数，未改全局配置），后续 clone/update 子模块如直连超时同样需走代理。主仓库 `5df2024` 未推送（ahead 1，推送时机由用户决定）。
- 【本次】交接文档分层重构：新建 `README.md`（门面 + 指针）、`docs/m2006_hardware.md`（M2006 稳定知识外置）、`docs/history_log.md`（历史归档）；主文档瘦身为"入口 + 当前状态 + 规则 + 文档地图"，删除与库 README 重复内容。

## 2026 年 9 月 5 日

- 将通用 PID 算法库独立为单独 git 仓库：`D:\desktop\junior_project\2_pid_lib_workplace_v2_260804\pid_lib_workplace_v2\pid_lib`（main 分支，root commit `0bbbfc9`），目录结构 include/src/tests，CMake 构建（静态库 + ctest），58 项主机端单元测试全部通过。
- 本工程 `Lib\pid_lib` 改为 git submodule 引用该独立仓库（commit `65e6466`）；submodule 后库文件位于 include/ 与 src/，Keil include 路径相应改为 `../Lib/pid_lib/include`、源文件改为 `../Lib/pid_lib/src/pid.c`（commit `25af4c5`），构建验证 0 Error(s), 0 Warning(s)。
- 同步机制决策：方案 B（git submodule）。因 PID 库定位为多项目复用，方案 A（复制同步）无法建立"库↔项目"双向版本链条，故弃用。
- 注意事项：子模块 URL 已切换为远程地址 `https://github.com/GaGiaa/pid_lib.git`；`protocol.file.allow=always` 为本地路径 clone 遗留配置，已写入本仓库 config，不影响远程拉取。
- 将 submodule URL 切换为远程地址 `https://github.com/GaGiaa/pid_lib.git`（H723 commit `9ebdf96` 已推送）；临时目录 `git clone --recursive` 验证通过，子模块从远程 checkout `0964fc2`。
- App 内部分层（本次）：m2006_control_task、vofa_timestamp_task 迁入 `App\Inc\task` / `App\Src\task`；m2006_driver、m2006_protocol、vofa_justfloat 迁入 `App\Inc\driver` / `App\Src\driver`；新建 `App\Inc\control` / `App\Src\control` 骨架（.gitkeep）预留速度/位置闭环。include 采用扁平策略（Keil include path 加三个子目录，源文件内 include 名不变）；uvprojx、命名检查器与测试同步更新；命名检查 PASS、单测 19/19、Keil 构建 0 Error 0 Warning。
- M2006 闭环控制（本次）：新增 `App\Inc\control\m2006_control.h` / `App\Src\control\m2006_control.c`（累计角度纯函数、compute 纯函数、update 胶水、PID 实例、独立调试面板）与 `tests\m2006_control_test.c`（18 项断言）；`m2006_driver` 新增 `m2006_driver_set_current_setpoint()` 接口、`current_setpoint` 语义升级为驱动输入；`m2006_control_task` 编排改为闭环先于发送；uvprojx 源文件列表、命名检查器（含新单测 files dict）同步更新；验证：命名检查 PASS、单测 19/19、control 测试 18 asserts PASS、Keil 构建 0 Error 0 Warning。
- 反馈换算收敛与角度下放（本次）：①control 层删除自有的角度回绕累计（`m2006_control_accumulate_angle` 与累计状态机），多圈连续角下放 driver 维护（新增 `m2006_debug.angle_total_deg`），回绕展开纯函数迁至 `m2006_protocol_unwrap_angle()`（protocol 层，主机端可测），对应 5 个用例迁至 m2006_protocol_test；②`angle_out_deg` 更名为 `angle_raw_deg` 并改为转子单圈相位角（0~360° 随编码器回绕），消除"输出轴一圈回绕"命名歧义；③control 位置/速度反馈直接映射 driver 换算值（pos_feedback_deg = angle_total_deg、speed_feedback_rpm = speed_out_rpm）；④protocol.h 新增共享常量 `M2006_PROTOCOL_ANGLE_RAW_SCALE_DEG` / `M2006_PROTOCOL_ANGLE_SCALE_DEG` / `M2006_PROTOCOL_TORQUE_CONSTANT_NM_PER_A`，driver/control 本地换算宏删除、统一引用；验证：命名检查 PASS、单测 19/19、control 测试 16 asserts PASS、protocol 测试 11 用例 PASS、Keil 构建 0 Error 0 Warning。

## 2026 年 9 月 4 日

- 新增 M2006 电机（配合 C610 电调，电调 ID=2）电流开环调试驱动：m2006_protocol 负责 CAN 协议编解码（控制帧 0x200、反馈帧 0x202 解析），m2006_driver 负责 FDCAN2 初始化、接收中断、电流钳位与安全门、控制帧发送。
- 在 freertos.c 新增 1kHz 控制任务 m2006_control，调试变量集中在结构体实例 m2006_debug（Keil Watch 一键添加即可查看/修改全部成员），支持在 Keil Watch 窗口在线修改调试。
- 安全保护：反馈超时（500ms）断输出、电流钳位（默认 ±3000=3A）、输出轴超速（默认 ±500rpm）断输出、断使能恒 0。
- 将命名检查器前缀规则泛化为多模块前缀（vofa、m2006），并把 m2006 模块与主机端测试纳入检查范围；新增 4 项命名单元测试。
- 新增主机端 m2006_protocol_test 协议测试；命名检查、单元测试与 Keil 构建均通过（0 Error, 0 Warning）。
- 本项改动不涉及 UART8/VOFA 时间戳任务，原有点亮与 VOFA 功能不受影响；M2006 功能尚未硬件实测。
- 目录分层：新建 App 层（App\Inc / App\Src），将 vofa_justfloat、m2006_protocol、m2006_driver 及两个 RTOS 任务（m2006_control、vofa_timestamp）全部迁入 App；Core 目录仅保留 CubeMX 生成文件，freertos.c 的 USER CODE 区只留 osThreadNew 胶水调用，CubeMX 重新生成不受影响。
- 为 m2006_debug 增加三个输出轴换算物理量（angle_out_deg / speed_out_rpm / torque_out_nm），并明确 angle_raw / speed_rpm / torque_raw 三个成员为电调回传原始值（未解析换算）；超速保护改用换算后的输出轴转速判断。
- 力矩换算改用 M2006 官方手册转矩常数 0.18 N·m/A（输出轴等效值），替换此前按额定点反推的估算值 0.3333 N·m/A；交接文档补录 M2006 完整电机参数表。
- 新增通用 PID 算法库（Lib\pid_lib\pid.h + pid.c）：位置式 + 增量式，纯 C 零平台依赖，支持设定值斜坡、梯形积分、条件积分、积分限幅、微分先行、微分滤波、输出限幅、死区滞回；配置区直接改结构体字段，dt=0 报错且安全返回；命名检查器加 pid 前缀，uvprojx 加 Lib/pid_lib include path 与源文件；新增 tests\pid_test.c 主机端 58 项单元测试全部通过，Keil 构建 0 Error 0 Warning。

## 2026 年 8 月 11 日

- 建立 `docs\c_naming_convention.md`，明确项目自有 C 代码的模块前缀、`snake_case`、宏、类型、任务名称、保留标识符和人工复查规则。
- 新增标准库 Python 命名检查器及其单元测试；检查器只覆盖当前项目自有 VOFA 文件、主机端协议测试和 `freertos.c` 的 `USER CODE` 区域。
- 将 JustFloat 公开接口迁移为 `vofa_justfloat_encode_float()`，将帧长度宏迁移为 `VOFA_JUSTFLOAT_FRAME_SIZE_BYTES`，并将 VOFA 时间戳任务标识符迁移为统一的 `snake_case`。
- 完成命名检查器单元测试、主机端 JustFloat 协议测试和 Keil 构建验证；检查器输出 `C naming check: PASS`，Keil 构建结果为 `0 Error(s), 0 Warning(s)`。
- 本次仅修改项目自有命名和文档，未更改 UART8 DMA、JustFloat 帧格式、任务周期或 CubeMX 配置，因此未新增硬件复测。

## 2026 年 8 月 6 日

- 完成 UART8 向 VOFA JustFloat 发送 RTOS 当前毫秒时间戳。
- 完成主机端协议测试和 Keil 构建验证。
- 完成板上 VOFA 接收验证；接线松动问题已经排除。
- 建立本交接文档，固化 Git、文档、子代理和 `docs\superpowers` 管理规则。
- 根据用户明确要求，将仓库整理为单一新的根提交，并将本机生成内容加入忽略规则。
