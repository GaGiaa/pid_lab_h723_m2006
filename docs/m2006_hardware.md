# M2006 硬件与调试参考

> 本文件是 `AI项目上下文交接文档.md` 的卫星文档，属稳定知识。**涉及 M2006 调试、接线、参数、协议、安全门或代码修改时阅读**。库的通用 API 与设计见 `Lib/m2006_lib/README.md`（子模块自带）；本文件只记录本工程相关的事实与配置。

## 一、硬件接线（本工程）

电机通过 C610 电调（电调 ID=2）接入 FDCAN2。FDCAN2 引脚为 PB12（RX）/ PB13（TX），AF9，经典 CAN 帧格式，波特率 1Mbps。

硬件接线注意：C610 的 CAN_H/CAN_L 需要接到板子 CAN2 接口（对应 FDCAN2 收发器），总线两端需 120Ω 终端电阻（电调端由拨码开关控制，控制板端取决于板卡设计）。

## 二、电机参数（M2006 P36 官方手册）

- 额定电压 24V；转矩常数 0.18 N·m/A（输出轴等效值）；转速常数 32.96 rpm/V；转速转矩梯度 110 rpm/N·m；机械时间常数 52.78 ms。
- 相电阻 461 mΩ；相电感 64.22 μH；极对数 7；减速比 36:1；减速电机重量 90 g；最大径向载荷（动载荷）495 N；使用环境温度 0-55℃。
- 注：相电阻/相电感/极对数为电机本体参数，可用于后续电流环建模或 FOC；转矩常数判定为输出轴等效值（推理见下）。

## 三、控制协议（C610 + M2006，DLC=8）

- 控制帧：标准帧 0x200（电调 ID 1~4）与 0x1FF（电调 ID 5~8）；每电调 ID 占 2 字节（高字节在前），组内偏移 = (ID-1)%4 × 2；电流值 -10000~+10000 对应 -10A~+10A，即 1000 LSB/A。ID=2 的电流位于 0x200 帧 DATA[2..3]，ID=7 的电流位于 0x1FF 帧 DATA[4..5]。
- 反馈帧：标准帧 0x200+电调ID（1~8 全覆盖，即 0x201~0x208；本电机 ID=2 → 0x202），DLC=8；DATA[0..1] 转子机械角度 0~8191，DATA[2..3] 转子转速 rpm（int16），DATA[4..5] 实际输出转矩（int16）。
- 反馈角度/转速均为转子（高速侧）原始值，输出轴转速 = 转速值 ÷ 36（M2006 减速比）。
- 反馈字段说明：DATA[4..5]“实际输出转矩”实为电调电流环反馈的实际输出电流（C610 只能测电流、不能测机械力矩），换算口径与控制指令同量纲：1000 LSB = 1A（即 -10000~+10000 对应 -10A~+10A）。真正的机械力矩 = 电流 × 转矩常数 × 减速比 × 效率。M2006 官方手册给出转矩常数 0.18 N·m/A（输出轴等效值，已含 36:1 减速比与传动效率；若为电机本体值则经减速后会远超额定，故判定为输出轴等效值，与额定点 3A→约 0.54 N·m 自洽）。驱动输出轴力矩换算：torque_out_nm = 电流(A) × 0.18 = torque_raw × 0.18 / 1000。

## 四、Keil 调试方法（Debug 界面 Watch 窗口）

- 一键添加结构体实例 `m2006_motor`（本工程电机实例，m2006_control_task.c 定义），即可查看并修改全部配置与观测变量。
- 可写成员（配置区）：`m2006_motor.is_enabled`（0 断输出/1 使能）、`m2006_motor.mode`（0=开环 / 1=速度环 / 2=位置环）、`m2006_motor.current_setpoint`（开环目标电流 ±10000）、`m2006_motor.current_limit`（电流钳位，默认 10000 = 10A，调试放开，带负载/上线前应收回 3000 = 3A）、`m2006_motor.speed_setpoint_rpm`（速度环目标，输出轴 rpm）、`m2006_motor.pos_setpoint_deg`（位置环目标，输出轴度）、`m2006_motor.speed_limit_rpm`（输出轴超速保护阈值，默认 0 = 关闭，>0 时生效）、`m2006_motor.pos_pid.kp`（位置环增益，默认 1.0）、`m2006_motor.pos_deadband_deg`（位置死区，默认 0.5°）、`m2006_motor.spd_pid.kp/ki`（速度环增益，默认 30/5）、`m2006_motor.spd_setpoint_rate`（速度设定斜坡，默认 0 禁用）。
- 只读成员（观测区）：`angle_raw`（转子角度编码 0~8191）、`speed_rpm`（转子转速，÷36 为输出轴）、`torque_raw`（反馈电流编码，1000 LSB=1A）、`angle_total_deg`（输出轴累计角度°，多圈不回绕）、`speed_out_rpm`（输出轴转速）、`torque_out_nm`（输出轴力矩）、`output_current`（实际下发电流）、`rx_msg_count`（已收反馈帧数）、`is_rx_timeout`（反馈超时标志）、`pos_feedback_deg`、`speed_feedback_rpm`、`speed_cmd_rpm`、`current_cmd_raw`、`pos_in_deadband`。
- 总线调试：Keil Watch 添加 `m2006_hal_bus` 查看总线实例（motor_slots 槽位挂载）；`m2006_hal_tx_fail_count` 查看发送失败计数。
- 调试流程：烧录后运行，先在 Watch 中确认 `m2006_motor.rx_msg_count` 持续增长（说明收到电调反馈）；再把 `m2006_motor.is_enabled` 置 1，从较小的 `m2006_motor.current_setpoint`（如 500）开始缓慢增大。

## 五、安全保护（m2006_motor_update 内自动执行，参数可调）

- 电流钳位：输出电流限制在 ±m2006_motor.current_limit（默认 ±10000 = 10A，电调满量程；M2006 额定 3A，带负载/上线前应收回到 ±3000）。
- 反馈超时：连续 20ms 未收到反馈（`m2006_motor.is_rx_timeout` 置 1）时输出强制置 0（1kHz 下正常每 1ms 一帧反馈）；tick 由调用者传入，超时判定在库内可测。
- 超速保护：speed_limit_rpm > 0 时，输出轴转速绝对值超过它则输出置 0；speed_limit_rpm = 0 表示关闭超速保护（调试期默认关闭）。
- 断使能：m2006_motor.is_enabled 为 0 时输出恒为 0。

## 六、M2006 闭环控制（速度环 + 位置环）

闭环逻辑位于 `Lib\m2006_lib\src\m2006_motor.c`，1kHz 任务（`m2006_control_task.c`）每周期先 `m2006_motor_update(&m2006_motor, tick)` 再打包发送。

级联结构（库内 m2006_motor_update 执行）：位置环（位置式 pid_t，纯 P + 死区）输出速度设定 → 速度环（增量式 pid_inc_t，PI）输出电流设定 → 电流钳位/安全门 → C610 内部电流环。

三模式（`m2006_motor.mode`）：
- `M2006_MOTOR_MODE_OPEN_LOOP`：电流开环，直通 `m2006_motor.current_setpoint`（Watch 手动设定）。
- `M2006_MOTOR_MODE_SPEED`：速度闭环，目标 `m2006_motor.speed_setpoint_rpm`（输出轴 rpm）。
- `M2006_MOTOR_MODE_POSITION`：位置闭环，目标 `m2006_motor.pos_setpoint_deg`（输出轴度），位置环输出限速 `pos_max_speed_rpm`。

位置反馈：多圈累计角度在电机实例内维护（`m2006_motor.angle_total_deg`，跨越 8191↔0 回绕连续）；输出轴角度 = 累计 LSB × 360/(36×8191)°。

调试面板：单一实例 `m2006_motor`（配置区 + 观测区，见"调试方法"小节），电流限幅、使能、超时/超速安全门均在本实例内，单一来源。

调参顺序：先 SPEED 调速度环（kp 从小到大再加 ki），再 POSITION 调位置环（纯 P 起步、kp 从小增大、加死区防抖）。

PID 初值：位置环 kp=1.0、ki=0、kd=0、输出不限幅（pos_max_speed_rpm=0 即不限幅，误差大时速度设定=误差×kp）、死区 0.5°；速度环 kp=30.0、ki=5.0、kd=0、输出 ±current_limit、设定斜坡 spd_setpoint_rate=0（禁用斜坡，设定直通）。

## 七、本工程接线方式与多电机/多 CAN 复用

本工程以 HAL 适配层（`App\Inc\driver\m2006_hal.h`、`App\Src\driver\m2006_hal.c`，FDCAN2 滤波/中断取帧分发/发送，持有总线实例 `m2006_hal_bus`）+ 1kHz 任务编排（`App\Src\task\m2006_control_task.c`：实例化 `m2006_motor`，每周期"闭环→打包→发送"）调用库。

多电机/多 CAN 复用方式（库设计动机）：每个总线实例对应一路 CAN（如 FDCAN2 → m2006_hal_bus，FDCAN3 → 另一个 m2006_bus）；每路总线上 `attach_motor` 挂 1~8 个电机实例（motor_slots[8] 按 esc_id-1 注册），各电机独立配置 PID 与安全参数。HAL 收发留在工程侧，回调把 can_id 与数据喂给对应总线实例的 `m2006_bus_handle_rx_frame`。

## 八、相关文件

- `Lib\m2006_lib`（git submodule）：`include\m2006_protocol.h` + `src\m2006_protocol.c`（协议编解码纯函数）、`include\m2006_motor.h` + `src\m2006_motor.c`（电机实例：透明结构体、三模式级联闭环、安全门、tick 由调用者传入、依赖 pid_lib）、`include\m2006_bus.h` + `src\m2006_bus.c`（总线实例：8 槽位注册、0x200/0x1FF 聚合打包、反馈分发）；`tests\` 三个主机端测试（protocol 15 用例、motor 37 断言、bus 48 断言，全部通过）。
- `App\Inc\driver\m2006_hal.h`、`App\Src\driver\m2006_hal.c`：本工程 FDCAN2 适配层。
- `App\Src\task\m2006_control_task.c`、`App\Inc\task\m2006_control_task.h`：M2006 1kHz 控制任务。
