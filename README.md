# pid_lab_h723_m2006

STM32H723ZGTx 嵌入式固件工程：M2006 无刷电机（C610 电调）控制实验台。FreeRTOS（CMSIS-RTOS2）任务调度，Keil MDK 构建，UART8→VOFA 健康检查。

## 目录

- `App/`：本工程自有代码——`driver/`（m2006_hal FDCAN2 适配、vofa_justfloat）、`task/`（m2006_control_task、vofa_timestamp_task）、`control/`（预留骨架）
- `Core/`：CubeMX 生成代码（外设、FreeRTOS 配置，`freertos.c` USER CODE 区仅留 osThreadNew 胶水）
- `Lib/`：独立库 git submodule——`pid_lib`（PID 算法库）、`m2006_lib`（M2006 电机库，协议/电机/总线三层，纯 C 零 HAL）
- `MDK-ARM/`：Keil 工程（`pid_lab_h723_m2006.uvprojx`）
- `docs/`：卫星文档——`m2006_hardware.md`（M2006 接线/参数/协议/调试/安全）、`history_log.md`（变更历史）、`c_naming_convention.md`（命名规范）
- `tests/`：主机端验证程序

## 构建与调试

Keil MDK 打开 `MDK-ARM/pid_lab_h723_m2006.uvprojx`，完整重建（-r）应为 `0 Error(s), 0 Warning(s)`。M2006 上板调试流程见 `docs/m2006_hardware.md`。

## 文档

- **AI 交接入口**：`AI项目上下文交接文档.md`（含文档地图，AI 接手任务必读）
- 库通用文档：各子模块自带 README（`Lib/pid_lib/README.md`、`Lib/m2006_lib/README.md`）
