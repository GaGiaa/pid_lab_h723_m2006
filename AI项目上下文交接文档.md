# AI 项目上下文交接文档

> 本文件是本项目的唯一 AI 交接入口。后续接手本项目的 AI，在处理任何开发任务前，必须先完整阅读本文件，再按"〇、文档地图"按需阅读卫星文档，随后检查实际代码和 Git 工作区状态。

最后更新日期：2026 年 9 月 6 日

## 〇、文档地图

| 文档 | 定位 | 何时读 |
| --- | --- | --- |
| 本文件 | AI 交接入口：当前状态 + 规则 + 指针 | 每次任务必读 |
| `docs/m2006_hardware.md` | M2006 稳定知识：接线/参数/协议/调试/安全门/闭环/本工程接线 | 涉及 M2006 调试、接线、参数、协议或代码修改时 |
| `docs/history_log.md` | 变更历史归档（头部有最新 3 条索引） | 追溯"上次改了什么、为什么"时 |
| `docs/c_naming_convention.md` | 项目自有 C 代码命名规范与复查流程 | 新增/修改项目自有 C 代码时 |
| `Lib/pid_lib/README.md` | PID 库通用文档（子模块自带） | 使用/修改 PID 库时 |
| `Lib/m2006_lib/README.md` | M2006 库通用文档（子模块自带） | 使用/修改 m2006 库时 |
| `README.md` | 仓库门面（面向人） | 不承载 AI 任务信息，可忽略 |

## 一、项目概况

项目路径：`D:\desktop\junior_project\2_pid_lib_workplace_v2_260804\pid_lib_workplace_v2\pid_lab_h723_m2006`

基于 STM32H723ZGTx 的嵌入式固件工程，Keil MDK 构建，CubeMX 生成基础外设代码，CMSIS-RTOS2 + FreeRTOS 实现实时任务调度。当前开发分支：`develop`。

主要目录职责：

- `Core\`：CubeMX 生成（外设、FreeRTOS 配置）；`freertos.c` 的 USER CODE 区仅保留 osThreadNew 胶水调用。
- `App\`：本工程自有代码——`task\`（m2006_control_task、vofa_timestamp_task）、`driver\`（m2006_hal FDCAN2 适配、vofa_justfloat 纯协议编码）、`control\`（预留骨架）。
- `Lib\`：两个 git submodule——`pid_lib`（PID 算法库）、`m2006_lib`（M2006 电机库，protocol/motor/bus 三层，纯 C 零 HAL）；均由独立仓库管理，不在本工程直接修改。
- `MDK-ARM\`：Keil 工程（`pid_lab_h723_m2006.uvprojx`）。
- `tests\`：主机端验证程序（协议/命名检查）。
- `docs\`：卫星文档（见文档地图）。
- `pid_lab_h723_m2006.ioc`：CubeMX 工程配置。

## 二、当前开发进度

当前状态：UART8→VOFA 健康检查（时间戳递增）功能正常；M2006 电机（C610，电调 ID=2，FDCAN2）驱动与 1kHz 控制任务已实现开环/速度环/位置环三模式与安全门；M2006 逻辑已全部抽入 `Lib\m2006_lib`（独立 git 仓库 `GaGiaa/m2006_lib`，submodule 接入）并由本工程 App 调用。

已完成事项（摘要）：

- UART8 时间戳任务 + JustFloat 编码（硬件已实测）。
- M2006 驱动、三模式闭环、安全门；库化迁移 + 独立仓库 + submodule 接入；命名检查、库单测、命名单测与 Keil 构建均通过。
- 目录分层：Core 仅保留 CubeMX 生成文件，自有模块收纳于 App。
- PID 库独立仓库 + submodule 接入（`GaGiaa/pid_lib`）。

当前已知限制 / 待办：

- **M2006 电机功能尚未硬件实测**；上板验证前先确认 FDCAN2 接线与终端电阻，再按 `docs/m2006_hardware.md` 调试流程操作。
- 主仓库 develop 分支 `5df2024`、`3d4addf` 两个提交未推送（push 需走代理，见 §五）。

## 三、功能与使用说明

### UART8 与 VOFA

- 硬件：UART8（TX=PE1，RX=PE0），波特率 1Mbps，8N1，无流控；DMA1_Stream1 内存→外设普通模式。初始化在 `Core\Src\usart.c`。
- 时间戳任务：`vofa_timestamp`（osPriorityNormal，栈 512B），每 100ms 通过 UART8 DMA 发送 RTOS tick 转 float32。FreeRTOS 节拍 1000Hz，数值即启动后毫秒数。DMA 忙时丢弃本次采样，不紧密重试。
- JustFloat 帧：`App\Inc\driver\vofa_justfloat.h` + `App\Src\driver\vofa_justfloat.c`，接口 `vofa_justfloat_encode_float()`，帧长宏 `VOFA_JUSTFLOAT_FRAME_SIZE_BYTES`；前 4 字节 float32 小端 + 帧尾 `00 00 80 7F`。不依赖 HAL/RTOS，主机端可测。
- 注意：UART8 当前仅该任务发送，无互斥锁；以后新增 UART8 发送者必须统一串行化或改共享发送队列。
- 相关文件：`Core\Src\freertos.c`、`tests\vofa_justfloat_test.c`、`App\Src\task\vofa_timestamp_task.c` + `App\Inc\task\vofa_timestamp_task.h`。

### M2006（硬件 / 协议 / 调试 / 闭环）

稳定知识全部外置于 `docs/m2006_hardware.md`：硬件接线、电机参数、CAN 控制/反馈协议与换算、Keil Watch 调试方法与字段清单、安全门参数、闭环结构与调参顺序、本工程 HAL/任务接线方式、多电机多 CAN 复用。**调试或修改 M2006 相关代码前必读**。库的通用 API 见 `Lib/m2006_lib/README.md`。

### PID 算法库

通用文档见 `Lib/pid_lib/README.md`（子模块自带）。本工程通过 m2006_motor 间接使用（位置式 + 增量式），不在工程内直接维护库代码。

## 四、验证状态

- 主机端：命名检查 `C naming check: PASS`、命名单测 19/19、m2006 库单测（protocol 15 用例 / motor 37 断言 / bus 48 断言）PASS、pid_test 58 断言 PASS。
- 构建：Keil 完整重建（-r）`0 Error(s), 0 Warning(s)`，axf/hex 正常生成。
- 硬件：板上原有点亮正常；VOFA（UART8，1Mbps，JustFloat）已实测，时间戳约每 100ms 递增、复位后从零开始；**M2006 电机功能未硬件实测**。
- 硬件复测注意：VOFA 串口选 UART8 TX 对应物理线路、波特率 1,000,000、协议 JustFloat、串口地与板子共地。

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

以下本工程自有文件不能被忽略：

- `Core\Src\freertos.c`。
- `App\Inc\driver\vofa_justfloat.h`、`App\Src\driver\vofa_justfloat.c`。
- `tests\vofa_justfloat_test.c`。
- `MDK-ARM\pid_lab_h723_m2006.uvprojx`。
- `App\Src\driver\m2006_hal.c`、`App\Inc\driver\m2006_hal.h`。
- `App\Src\task\m2006_control_task.c`、`App\Inc\task\m2006_control_task.h`。
- `App\Src\task\vofa_timestamp_task.c`、`App\Inc\task\vofa_timestamp_task.h`。
- `tests\pid_test.c`。

（`Lib\pid_lib` 与 `Lib\m2006_lib` 为 submodule，内容由独立仓库管理，不在此列举。）

### submodule 规则

- `Lib\pid_lib`、`Lib\m2006_lib` 均为 git submodule，分别引用 `https://github.com/GaGiaa/pid_lib.git` 与 `https://github.com/GaGiaa/m2006_lib.git`；子模块内容不在本工程内直接修改。
- 改库流程：在独立仓库提交并推送，再回到本工程升级子模块指针（进入 `Lib/<lib>` 执行 `git fetch` + `git checkout <版本>`，然后在本工程提交更新后的 gitlink）。
- 克隆本工程需使用 `git clone --recursive` 以带出子模块。
- **网络注意**：本机访问 GitHub 443 直连不稳定（TCP 通但 HTTP 层超时），git 操作建议加 `-c http.proxy=http://127.0.0.1:7897`（本机 Clash Verge 混合端口，临时参数不写全局配置）；后续 clone/update 子模块如直连超时同样需走代理。
- 本仓库 config 保留 `protocol.file.allow=always`（早期本地路径 clone 的遗留配置，远程拉取不受影响，可保留）。

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
- **文档职责边界**：库文档（`Lib/pid_lib/README.md`、`Lib/m2006_lib/README.md`）只描述库本身与通用接入方式，不写任何调用方工程的特定配置；M2006 硬件/协议/调试稳定知识归 `docs/m2006_hardware.md`；变更历史归 `docs/history_log.md`；本交接文档只记录本工程（pid_lab_h723_m2006）的当前状态与规则。写文档前先判断内容归属：换一个工程是否仍成立——成立属库文档，依赖本工程路径或配置的属本交接文档或其卫星文档。

## 八、后续 AI 工作顺序

接手新任务时，按以下顺序执行：

1. 先完整阅读本文件，再按"〇、文档地图"判断本次任务涉及的主题，按需阅读对应卫星文档。
2. 完整阅读 `docs\c_naming_convention.md`，确认本任务涉及的项目自有标识符、外部接口例外和自动检查范围。
3. 检查 `git status`、当前分支、最近提交和相关源文件，确认实际状态没有偏离本文件。
4. 明确任务范围、成功标准和是否涉及硬件验证。
5. 对相互独立的只读检查或实现任务合理使用子代理；涉及同一文件的修改保持串行。
6. 修改前说明将修改的文件和原因。
7. 新增或修改项目自有 C 代码后，运行 `py tests\check_c_naming.py` 并完成命名规范规定的人工复查；检查器发现的违规必须修复，外部固定名称必须记录最小范围的例外理由。
8. 运行与改动风险相匹配的测试和构建验证。
9. 只有用户明确要求时才提交 Git，并按本文件的提交规则检查提交结果。
10. 完成任务后更新本文件的"当前开发进度/最近变更"与相关卫星文档；变更记录追加到 `docs/history_log.md`；不要创建 `docs\superpowers` 文件。

## 九、最近变更

- 2026-09-06：m2006_lib 独立为 git 仓库（GitHub `GaGiaa/m2006_lib`，public，develop，首提交 `6e203b6`）并以 submodule 接入本工程（gitlink `5df2024`）；交接文档分层重构（新建 README、`docs/m2006_hardware.md`、`docs/history_log.md`，主文档瘦身为"入口 + 当前状态 + 规则 + 文档地图"）。完整历史见 `docs/history_log.md`。
