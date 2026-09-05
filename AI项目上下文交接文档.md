# AI 项目上下文交接文档

> 本文件是本项目的唯一 AI 交接入口。后续接手本项目的 AI，在处理任何开发任务前，必须先阅读本文件和 `docs\c_naming_convention.md`，再检查实际代码和 Git 工作区状态。

最后更新日期：2026 年 9 月 5 日

## 一、项目概况

项目路径：`D:\desktop\junior_project\2_pid_lib_workplace_v2_260804\pid_lib_workplace_v2\pid_lab_h723_m2006`

这是一个基于 STM32H723ZGTx 的嵌入式固件工程，使用 Keil MDK 进行构建，使用 CubeMX 生成基础外设代码，使用 CMSIS-RTOS2 接口和 FreeRTOS 内核实现实时任务调度。

项目最初从 `single_motor_test` 项目复用了 CubeMX 工程，随后修复了 CAN1 引脚配置问题，并移除了 UART7 配置。当前工程能够正常点亮，已有 LED 行为必须保持不变。

主要目录职责如下：

- `Core\Inc`：应用头文件、外设头文件和 FreeRTOS 配置文件。
- `Core\Src`：应用代码、外设初始化代码、中断处理代码和 RTOS 任务代码。
- `Drivers`：STM32H7 HAL 驱动、CMSIS 内核头文件和芯片支持文件。
- `Middlewares`：FreeRTOS、CMSIS-RTOS2 和 ARM DSP 相关中间件。
- `MDK-ARM`：Keil 工程文件、启动文件以及本机生成的构建和调试文件。
- `tests`：可在主机端运行的协议编码验证程序。
- `pid_lab_h723_m2006.ioc`：CubeMX 工程配置文件。

## 二、当前开发进度

当前已完成 UART8 向 VOFA 发送 RTOS 系统当前时间戳的健康检查功能，功能目的为通过一个持续递增的数值确认芯片、RTOS 调度器和应用任务仍在正常运行。

当前没有已知的代码级待办事项。项目自有 C 代码必须遵循 `docs\c_naming_convention.md`，并在每次新增或修改后通过自动命名检查与人工命名复查。后续新增需求应先阅读本文件，再根据实际代码、构建结果和用户最新要求更新本文件中的进度记录。

### 已完成事项

- 完成 UART8 发送链路确认，未使用 UART7。
- 完成 VOFA JustFloat 单通道编码器。
- 完成 RTOS 时间戳任务，并通过 UART8 DMA 周期发送。
- 将编码器源文件加入当前 Keil 工程文件。
- 增加主机端 JustFloat 字节编码测试。
- 完成主机端测试和 Keil 工程构建验证。
- 已完成硬件验证：最初 VOFA 无数据的原因是 UART8 物理接线松动；接线恢复后，VOFA 已经可以正常接收数据。
- 新增 M2006 电机（配合 C610 电调，电调 ID=2）电流开环调试驱动，接入 FDCAN2（PB12/PB13，经典 CAN 1Mbps）。
- 新增 m2006_protocol 纯协议编解码模块与 m2006_driver HAL 驱动模块。
- 新增 1kHz 电流开环控制任务 m2006_control，可在 Keil Watch 窗口修改全局变量在线调试。
- 将命名检查器前缀规则泛化为多模块前缀（vofa、m2006），并新增对应单元测试。
- 新增主机端 m2006_protocol_test 协议测试；命名检查、单元测试与 Keil 构建均通过。
- 目录分层：新建 App 层（App\Inc / App\Src）收纳全部自有模块与两个 RTOS 任务，Core 仅保留 CubeMX 生成文件。

## 三、UART8 和 VOFA 功能说明

### UART8 硬件配置

- 外设：`UART8`。
- 发送引脚：`PE1`，即 UART8 TX。
- 接收引脚：`PE0`，当前功能只使用发送方向。
- 波特率：`1000000`。
- 数据格式：8 数据位、无校验、1 个停止位，无硬件流控。
- DMA：`DMA1_Stream1`，方向为内存到外设，普通模式。
- 当前工程的 UART8 初始化和 DMA 初始化位于 `Core\Src\usart.c`。

### RTOS 时间戳任务

时间戳任务位于 `Core\Src\freertos.c`，任务属性如下：

- 任务名称：`vofa_timestamp`。
- 任务优先级：`osPriorityNormal`。
- 任务栈大小：`512` 字节。
- 发送周期：`100` 毫秒。
- 时间来源：`osKernelGetTickCount()`。
- 当前 FreeRTOS 配置的时钟节拍为 `1000 Hz`，因此时间戳数值表示 RTOS 启动后的毫秒数。
- 时间戳转换为 `float32` 后发送。
- UART DMA 忙或出现其他 HAL 错误时，当前采样会被丢弃，任务等待下一个周期继续运行，不进行紧密重试。

任务使用自身的 8 字节帧缓冲区。由于当前 UART8 只有该健康检查任务发送数据，暂时没有增加互斥锁；以后如果增加其他 UART8 发送者，必须统一串行化 UART8 发送，或改为共享发送队列。

### JustFloat 数据帧

编码器位于：

- `App\Inc\vofa_justfloat.h`
- `App\Src\vofa_justfloat.c`

当前只发送一个 `float32` 通道。每帧共 8 字节：

1. 前 4 字节为时间戳浮点数的 IEEE-754 小端表示。
2. 后 4 字节为 JustFloat 帧尾：`00 00 80 7F`。

公开编码接口为 `vofa_justfloat_encode_float()`，帧长度宏为 `VOFA_JUSTFLOAT_FRAME_SIZE_BYTES`。该接口不依赖 STM32 HAL 或 RTOS，便于主机端独立测试。

### 相关文件

- `Core\Src\freertos.c`：CubeMX 生成文件，USER CODE 区仅保留 osThreadNew 胶水调用，defaultTask 由 CubeMX 管理。
- `App\Inc\vofa_justfloat.h`：JustFloat 帧长度常量和编码接口声明。
- `App\Src\vofa_justfloat.c`：单通道 JustFloat 编码实现。
- `MDK-ARM\pid_lab_h723_m2006.uvprojx`：当前 Keil 工程文件，必须包含 `vofa_justfloat.c`。
- `tests\vofa_justfloat_test.c`：主机端编码测试。
- `docs\c_naming_convention.md`：项目自有 C 代码的强制命名规范和复查流程。
- `tests\check_c_naming.py`：项目自有 C 代码的自动命名检查器。
- `tests\check_c_naming_test.py`：命名检查器的主机端单元测试。
- `App\Src\m2006_driver.c`、`App\Inc\m2006_driver.h`：M2006 电机电流开环调试驱动。
- `App\Src\m2006_protocol.c`、`App\Inc\m2006_protocol.h`：C610 电调 CAN 协议编解码纯函数。
- `tests\m2006_protocol_test.c`：主机端协议编解码测试。
- `App\Src\m2006_control_task.c`、`App\Inc\m2006_control_task.h`：M2006 1kHz 电流开环控制任务。
- `App\Src\vofa_timestamp_task.c`、`App\Inc\vofa_timestamp_task.h`：VOFA 时间戳上报任务。
- `Lib\pid_lib`：git submodule，引用独立 PID 库仓库（路径与版本见"通用 PID 算法库"小节）。
- `tests\pid_test.c`：PID 库主机端单元测试（58 项断言，覆盖 P/PI/PD/限幅/斜坡/死区/滞回/滤波/条件积分/增量式等）。

### M2006 电机调试功能（C610 电调，FDCAN2）

电机通过 C610 电调（电调 ID=2）接入 FDCAN2。FDCAN2 引脚为 PB12（RX）/ PB13（TX），AF9，经典 CAN 帧格式，波特率 1Mbps。

硬件接线注意：C610 的 CAN_H/CAN_L 需要接到板子 CAN2 接口（对应 FDCAN2 收发器），总线两端需 120Ω 终端电阻（电调端由拨码开关控制，控制板端取决于板卡设计）。

电机参数（M2006 P36 官方手册）：
- 额定电压 24V；转矩常数 0.18 N·m/A（输出轴等效值）；转速常数 32.96 rpm/V；转速转矩梯度 110 rpm/N·m；机械时间常数 52.78 ms。
- 相电阻 461 mΩ；相电感 64.22 μH；极对数 7；减速比 36:1；减速电机重量 90 g；最大径向载荷（动载荷）495 N；使用环境温度 0-55℃。
- 注：相电阻/相电感/极对数为电机本体参数，可用于后续电流环建模或 FOC；转矩常数判定为输出轴等效值（推理见上）。

控制协议：
- 控制帧：标准帧 0x200，DLC=8，每电调 ID 占 2 字节（高字节在前）；电流值 -10000~+10000 对应 -10A~+10A，即 1000 LSB/A。ID=2 的电流位于 DATA[2..3]。
- 反馈帧：标准帧 0x200+电调ID（即 0x202），DLC=8；DATA[0..1] 转子机械角度 0~8191，DATA[2..3] 转子转速 rpm（int16），DATA[4..5] 实际输出转矩（int16）。
- 反馈角度/转速均为转子（高速侧）原始值，输出轴转速 = 转速值 ÷ 36（M2006 减速比）。
- 反馈字段说明：DATA[4..5]“实际输出转矩”实为电调电流环反馈的实际输出电流（C610 只能测电流、不能测机械力矩），换算口径与控制指令同量纲：1000 LSB = 1A（即 -10000~+10000 对应 -10A~+10A）。真正的机械力矩 = 电流 × 转矩常数 × 减速比 × 效率。M2006 官方手册给出转矩常数 0.18 N·m/A（输出轴等效值，已含 36:1 减速比与传动效率；若为电机本体值则经减速后会远超额定，故判定为输出轴等效值，与额定点 3A→约 0.54 N·m 自洽）。驱动输出轴力矩换算：torque_out_nm = 电流(A) × 0.18 = torque_raw × 0.18 / 1000。

Keil 调试方法（在 Debug 界面 Watch 窗口）：
- 一键添加结构体实例 `m2006_debug`，即可查看并修改全部调试变量（无需逐个添加）。
- 可写成员：`m2006_debug.is_enabled`（0 断输出/1 使能）、`m2006_debug.current_setpoint`（目标电流 ±10000）、`m2006_debug.current_limit`（电流钳位，默认 3000）、`m2006_debug.speed_limit_rpm`（输出轴转速限幅，默认 500）。
- 只读成员：
  - 电调回传原始值（未解析换算）：`m2006_debug.angle_raw`（转子机械角度编码 0~8191）、`m2006_debug.speed_rpm`（转子转速 rpm，÷36 为输出轴）、`m2006_debug.torque_raw`（“实际输出转矩”编码，实为电流环反馈电流，1000 LSB = 1A）；
  - 输出轴换算值（驱动 1kHz 内换算）：`m2006_debug.angle_out_deg`（输出轴角度°）、`m2006_debug.speed_out_rpm`（输出轴转速 rpm）、`m2006_debug.torque_out_nm`（输出轴力矩 N·m，= 电流 A × 0.18，M2006 官方转矩常数）；
  - 其他：`m2006_debug.output_current`、`m2006_debug.rx_msg_count`、`m2006_debug.is_rx_timeout`、`m2006_debug.tx_fail_count`。
- 调试流程：烧录后运行，先在 Watch 中确认 `m2006_debug.rx_msg_count` 持续增长（说明收到电调反馈）；再把 `m2006_debug.is_enabled` 置 1，从较小的 `m2006_debug.current_setpoint`（如 500）开始缓慢增大。

安全保护（驱动内自动执行，参数可调）：
- 电流钳位：输出电流限制在 ±m2006_debug.current_limit（默认 ±3000 = 3A，即 M2006 额定电流）。
- 反馈超时：连续 500ms 未收到反馈（`m2006_debug.is_rx_timeout` 置 1）时输出强制置 0。
- 超速保护：输出轴转速绝对值超过 m2006_debug.speed_limit_rpm 时输出置 0。
- 断使能：m2006_debug.is_enabled 为 0 时输出恒为 0。


### 通用 PID 算法库（Lib/pid_lib）

> 2026 年 9 月 5 日起，PID 库已独立为单独 git 仓库：`D:\desktop\junior_project\2_pid_lib_workplace_v2_260804\pid_lib_workplace_v2\pid_lib`（分支 main，root commit `0bbbfc9`，CMake 构建 + ctest）。本工程的 `Lib\pid_lib` 以 git submodule 方式引用该仓库，不再直接维护库文件；改库需在独立仓库提交并推送，再回到本工程升级子模块指针（进入 `Lib/pid_lib` 执行 `git fetch` + `git checkout <版本>`，然后在本工程提交更新后的 gitlink）。子模块 URL 当前为本地绝对路径，待独立仓库推送远程后执行 `git submodule set-url Lib/pid_lib <远程URL>` 并提交。

通用 PID 算法库，纯 C 实现，零平台依赖（不依赖 HAL/RTOS），可独立复用于任意 C 项目。

**目录**：`Lib\pid_lib\pid.h`（类型定义+函数声明）、`Lib\pid_lib\pid.c`（实现）。

**类型**：
- `pid_status_t`：错误码枚举（`PID_OK` / `PID_ERR_INVALID_DT` / `PID_ERR_INVALID_LIMIT`）。
- `pid_t`：位置式 PID 实例结构体（配置区 + 状态区）。
- `pid_inc_t`：增量式 PID 实例结构体（配置区 + 状态区）。

**函数**：
- 位置式：`pid_init()`（检查配置+清状态，返回错误码）、`pid_reset()`（只清状态保留配置）、`pid_update(pid, setpoint, measurement)`（周期计算，返回输出）。
- 增量式：`pid_inc_init()`、`pid_inc_reset()`、`pid_inc_update()`。

**位置式特性**：设定值斜坡、梯形积分、条件积分（饱和停积分 + 大误差停积分，`integral_hold_error=0` 时只启用饱和停积分）、积分限幅、微分先行（始终开启）、微分滤波（对测量值低通后再微分）、输出限幅、死区滞回（死区内误差置零，退出需超 `deadband+hysteresis`）。

**增量式特性**：设定值斜坡、梯形积分、微分先行、微分滤波、输出限幅（作用于累加后的绝对输出，内部维护累加器）、死区滞回；P/I/D 三项诊断变量为本次增量（Δp/Δi/Δd），`delta_u` 为总增量。无独立积分器，不需要积分限幅和条件积分。

**配置约定**：
- 配置区直接修改结构体字段即可，运行时可调参，不设 setter。
- `dt` 必须 > 0，`pid_init` 检查，失败返回错误码且 `is_valid=0`；`pid_update` 入口也检查 `dt`，非法时安全返回 0。
- 输出限幅 `out_min/out_max` 都为 0 时视为不限幅；积分限幅同理。
- `setpoint_rate=0` 禁用斜坡；`deadband=0` 禁用死区；`d_filter_alpha=1` 不滤波。
- 微分先行与梯形积分始终开启，无开关。

**测试**：`tests\pid_test.c` 主机端单元测试，gcc 编译运行，58 项断言全部通过，覆盖 P/PI/PD 基本控制、输出限幅、不限幅（双零）、积分限幅、设定值斜坡、死区、死区滞回、dt=0 报错、reset 清状态、条件积分大误差停积分、微分滤波、增量式 P/PI/限幅/reset/dt 报错。

## 四、验证状态

已完成的验证包括：

- 主机端编码测试输出 `vofa_justfloat_test: PASS`。
- Keil 工程构建结果为 `0 Error(s), 0 Warning(s)`。
- C 命名检查器输出 `C naming check: PASS`，其单元测试全部通过。
- 板上原有点亮功能保持正常。
- VOFA 使用 UART8 接收 JustFloat 数据已经完成实测。
- VOFA 无数据问题已经定位为接线松动，不是当前代码、DMA 配置或 JustFloat 帧格式问题。
- 主机端 m2006_protocol_test 输出 `m2006_protocol_test: PASS`。
- 命名检查器输出 `C naming check: PASS`，其单元测试（含 m2006 模块）全部通过。
- Keil 工程构建结果为 `0 Error(s), 0 Warning(s)`。
- M2006 电机调试功能尚未进行硬件实测；上板验证时应先确认 FDCAN2 对应板卡 CAN2 接口接线与终端电阻，再按 Keil 调试流程操作。

硬件复测时应确认：VOFA 串口选择 UART8 TX 对应的物理线路，波特率为 1,000,000，协议选择 JustFloat，并且串口地线与板子共地。正常情况下，一个通道的数值应持续递增，约每 100 毫秒产生一次新采样；复位后数值应重新从接近零的位置开始。

## 五、Git 和文件管理规则

### 提交规则

- 除非用户明确要求提交 Git，否则不得根据开发进度自动提交。
- 用户明确要求提交时，必须先全面审视工作区、暂存区、差异、忽略规则和提交范围。
- 提交时不使用代码审查机器人或其他用户未要求的审查工具。
- 提交信息必须使用详细中文，必要的专业名词可以保留英文，例如 `STM32H723`、`UART8`、`VOFA`、`JustFloat`、`FreeRTOS` 和 `Keil`。
- 提交后必须检查提交标题、正文换行、中文编码和提交内容。如果发现乱码、排版错误或范围错误，必须立即修正后重新提交。
- 不得因为提交方便而把构建产物、调试配置或与当前任务无关的本机文件加入版本控制。

### 忽略规则

根目录 `.gitignore` 负责忽略 Keil 构建产物、本机调试配置、`docs\superpowers`、Python 字节码缓存和主机端临时测试程序。

以下固件源文件不能被忽略：

- `Core\Src\freertos.c`。
- `App\Inc\vofa_justfloat.h`。
- `App\Src\vofa_justfloat.c`。
- `tests\vofa_justfloat_test.c`。
- `MDK-ARM\pid_lab_h723_m2006.uvprojx`。
- `App\Src\m2006_driver.c`。
- `App\Inc\m2006_driver.h`。
- `App\Src\m2006_protocol.c`。
- `App\Inc\m2006_protocol.h`。
- `tests\m2006_protocol_test.c`。
- `App\Src\m2006_control_task.c`。
- `App\Inc\m2006_control_task.h`。
- `App\Src\vofa_timestamp_task.c`。
- `App\Inc\vofa_timestamp_task.h`。
- `Lib\pid_lib\pid.h`。
- `Lib\pid_lib\pid.c`。
- `tests\pid_test.c`。

### submodule 规则

- `Lib\pid_lib` 是 git submodule，引用独立 PID 库仓库；子模块内容由独立仓库管理，不在本工程内直接修改。
- 本工程已设置 `protocol.file.allow=always`（仅本仓库 local config），支持从本地路径 clone 子模块；换机后如需重新初始化子模块，需先确认该配置。
- 独立 pid_lib 仓库推送远程后，必须执行 `git submodule set-url Lib/pid_lib <远程URL>` 更新 `.gitmodules`，并提交。

### docs\superpowers 规则

不得在 `docs\superpowers` 目录生成新的计划、设计或其他文件。需要记录的方案、风险、待确认事项和设计内容，直接在当前会话中说明，并要求用户确认。

当前目录中已经存在的历史文件只保留在本地，不纳入新的 Git 提交，并由 `.gitignore` 忽略。后续 AI 不应把这些文件当作项目交接入口；项目交接以本文件为准。

## 六、子代理使用规则

鼓励将相互独立的任务分发给子代理进行并行处理。子代理的模型和推理程度应与主代理保持一致。

分发任务时必须明确文件边界和验收标准。多个代理不得同时修改同一个文件或同时改写同一个 Git 索引。涉及 Git 历史重建、提交、分支切换和共享工作区的操作，应由主代理统一执行并在执行前后检查工作区状态。

## 七、文档语言规则

- 项目自有文档必须使用中文撰写。
- 必要的专业英语名词、协议名称、代码、文件路径和命令可以保留原文。
- STM32、CMSIS、FreeRTOS、HAL、Keil 等第三方组件的许可证和法律原文不得擅自翻译或修改。
- 新增项目说明、交接记录、进度记录和变更说明时，默认使用中文。

## 八、后续 AI 工作顺序

接手新任务时，按以下顺序执行：

1. 先完整阅读本文件。
2. 完整阅读 `docs\c_naming_convention.md`，确认本任务涉及的项目自有标识符、外部接口例外和自动检查范围。
3. 检查 `git status`、当前分支、最近提交和相关源文件，确认实际状态没有偏离本文件。
4. 明确任务范围、成功标准和是否涉及硬件验证。
5. 对相互独立的只读检查或实现任务合理使用子代理；涉及同一文件的修改保持串行。
6. 修改前说明将修改的文件和原因。
7. 新增或修改项目自有 C 代码后，运行 `py tests\check_c_naming.py` 并完成命名规范规定的人工复查；检查器发现的违规必须修复，外部固定名称必须记录最小范围的例外理由。
8. 运行与改动风险相匹配的测试和构建验证。
9. 只有用户明确要求时才提交 Git，并按本文件的提交规则检查提交结果。
10. 完成任务后更新本文件的开发进度、验证状态和已知限制；不要创建 `docs\superpowers` 文件。

## 九、持续更新记录

### 2026 年 9 月 5 日

- 将通用 PID 算法库独立为单独 git 仓库：`D:\desktop\junior_project\2_pid_lib_workplace_v2_260804\pid_lib_workplace_v2\pid_lib`（main 分支，root commit `0bbbfc9`），目录结构 include/src/tests，CMake 构建（静态库 + ctest），58 项主机端单元测试全部通过。
- 本工程 `Lib\pid_lib` 改为 git submodule 引用该独立仓库（commit `65e6466`），Keil include 路径与源文件引用不变，编译行为不受影响。
- 同步机制决策：方案 B（git submodule）。因 PID 库定位为多项目复用，方案 A（复制同步）无法建立"库↔项目"双向版本链条，故弃用。
- 注意事项：子模块 URL 当前为本地绝对路径（`D:/.../pid_lib`），独立仓库推送远程后需 `git submodule set-url` 更新；本地路径 clone 依赖 `protocol.file.allow=always`（已写入本仓库 config）。

### 2026 年 9 月 4 日

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

### 2026 年 8 月 11 日

- 建立 `docs\c_naming_convention.md`，明确项目自有 C 代码的模块前缀、`snake_case`、宏、类型、任务名称、保留标识符和人工复查规则。
- 新增标准库 Python 命名检查器及其单元测试；检查器只覆盖当前项目自有 VOFA 文件、主机端协议测试和 `freertos.c` 的 `USER CODE` 区域。
- 将 JustFloat 公开接口迁移为 `vofa_justfloat_encode_float()`，将帧长度宏迁移为 `VOFA_JUSTFLOAT_FRAME_SIZE_BYTES`，并将 VOFA 时间戳任务标识符迁移为统一的 `snake_case`。
- 完成命名检查器单元测试、主机端 JustFloat 协议测试和 Keil 构建验证；检查器输出 `C naming check: PASS`，Keil 构建结果为 `0 Error(s), 0 Warning(s)`。
- 本次仅修改项目自有命名和文档，未更改 UART8 DMA、JustFloat 帧格式、任务周期或 CubeMX 配置，因此未新增硬件复测。

### 2026 年 8 月 6 日

- 完成 UART8 向 VOFA JustFloat 发送 RTOS 当前毫秒时间戳。
- 完成主机端协议测试和 Keil 构建验证。
- 完成板上 VOFA 接收验证；接线松动问题已经排除。
- 建立本交接文档，固化 Git、文档、子代理和 `docs\superpowers` 管理规则。
- 根据用户明确要求，将仓库整理为单一新的根提交，并将本机生成内容加入忽略规则。

后续开发者应在这里追加新的日期和事实记录，删除已经失效的状态，不要保留与实际代码不一致的描述。
