# AI 项目上下文交接文档

> 本文件是本项目的唯一 AI 交接入口。后续接手本项目的 AI，在处理任何开发任务前，必须先阅读本文件和 `docs\c_naming_convention.md`，再检查实际代码和 Git 工作区状态。

最后更新日期：2026 年 8 月 11 日

## 一、项目概况

项目路径：`D:\desktop\pid_lib_workplace_v2\pid_lab_h723_m2006`

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

- `Core\Inc\vofa_justfloat.h`
- `Core\Src\vofa_justfloat.c`

当前只发送一个 `float32` 通道。每帧共 8 字节：

1. 前 4 字节为时间戳浮点数的 IEEE-754 小端表示。
2. 后 4 字节为 JustFloat 帧尾：`00 00 80 7F`。

公开编码接口为 `vofa_justfloat_encode_float()`，帧长度宏为 `VOFA_JUSTFLOAT_FRAME_SIZE_BYTES`。该接口不依赖 STM32 HAL 或 RTOS，便于主机端独立测试。

### 相关文件

- `Core\Src\freertos.c`：创建任务、读取 RTOS 时间戳并调用 UART8 DMA。
- `Core\Inc\vofa_justfloat.h`：JustFloat 帧长度常量和编码接口声明。
- `Core\Src\vofa_justfloat.c`：单通道 JustFloat 编码实现。
- `MDK-ARM\pid_lab_h723_m2006.uvprojx`：当前 Keil 工程文件，必须包含 `vofa_justfloat.c`。
- `tests\vofa_justfloat_test.c`：主机端编码测试。
- `docs\c_naming_convention.md`：项目自有 C 代码的强制命名规范和复查流程。
- `tests\check_c_naming.py`：项目自有 C 代码的自动命名检查器。
- `tests\check_c_naming_test.py`：命名检查器的主机端单元测试。

## 四、验证状态

已完成的验证包括：

- 主机端编码测试输出 `vofa_justfloat_test: PASS`。
- Keil 工程构建结果为 `0 Error(s), 0 Warning(s)`。
- C 命名检查器输出 `C naming check: PASS`，其单元测试全部通过。
- 板上原有点亮功能保持正常。
- VOFA 使用 UART8 接收 JustFloat 数据已经完成实测。
- VOFA 无数据问题已经定位为接线松动，不是当前代码、DMA 配置或 JustFloat 帧格式问题。

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
- `Core\Inc\vofa_justfloat.h`。
- `Core\Src\vofa_justfloat.c`。
- `tests\vofa_justfloat_test.c`。
- `MDK-ARM\pid_lab_h723_m2006.uvprojx`。

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
