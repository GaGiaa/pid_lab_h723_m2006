# C 工程命名规范

## 一、目的和适用范围

本规范统一项目自有 C 代码的命名方式，使模块归属、作用和数据单位可以从名称直接判断。后续 AI 和开发者在处理本项目任何开发任务前，必须先阅读根目录的 `AI项目上下文交接文档.md`，再阅读本文件。

本规范适用于以下内容：

- 项目自有的 `.c`、`.h` 文件和主机端 C 测试代码。
- CubeMX 文件中 `/* USER CODE BEGIN ... */` 与 `/* USER CODE END ... */` 之间新增或维护的项目自有代码。
- 后续新增的项目自有模块、测试和接口。

以下内容不适用本规范，也不得为了统一外观而改名：

- CubeMX 在 `USER CODE` 区域以外生成的符号、文件和配置。
- STM32 HAL、CMSIS、FreeRTOS、C 标准库和 Keil 提供的 API、类型、宏和回调。
- 因框架协议而固定的入口或回调，例如 `MX_FREERTOS_Init`、`startDefaultTask`、`vApplicationStackOverflowHook` 和 `main`。

遇到新的外部固定名称时，保留原名称，并在命名检查器的例外表中添加最小范围、带原因的记录；不得用宽泛模式跳过一类名称。

## 二、通用规则

- 项目自有标识符、文件名和字符串任务名使用 ASCII 字符。
- 单词以完整语义表达，不使用无上下文的缩写；协议、芯片和行业通用缩写（例如 `vofa`、`uart`、`dma`、`rtos`）可以保留。
- 不用前导下划线或双下划线命名项目标识符，避免占用 C 实现保留名称。
- 不使用 `g_`、`s_` 等前缀编码存储期；`static`、`extern` 和模块边界应由声明与文件职责表达。
- 变量名应表达语义和单位。缓冲区使用 `_buffer`，句柄使用 `_handle`，数组数量使用 `_count`，下标使用 `_index`，字节数使用 `_bytes`，时间使用 `_ms`、`_us` 或 `_s`。

## 三、名称格式

| 对象 | 规则 | 示例 |
| --- | --- | --- |
| 源文件和头文件 | `lower_snake_case` | `vofa_justfloat.c` |
| 头文件保护宏 | 模块前缀加 `UPPER_SNAKE_CASE`，不以 `_` 开头 | `VOFA_JUSTFLOAT_H` |
| 宏和枚举值 | 模块前缀加 `UPPER_SNAKE_CASE` | `VOFA_JUSTFLOAT_FRAME_SIZE_BYTES` |
| 结构体、联合体和枚举标签 | `lower_snake_case` | `struct vofa_frame` |
| `typedef` 类型别名 | `lower_snake_case_t` | `vofa_frame_t` |
| 公开函数 | `模块_动作_对象` 的 `lower_snake_case` | `vofa_justfloat_encode_float` |
| 文件内 `static` 函数 | 语义明确的 `lower_snake_case` | `vofa_timestamp_task_entry` |
| 变量、参数、结构字段和测试辅助函数 | `lower_snake_case` | `frame_buffer`、`byte_count` |
| 布尔值 | `is_`、`has_`、`can_` 或 `should_` 开头 | `is_dma_ready` |
| FreeRTOS 任务运行时名称 | 模块前缀加 `lower_snake_case` | `"vofa_timestamp"` |

公开函数、公开宏、枚举值和任务名称必须以所属模块的前缀开头。文件内变量与函数仍应使用能反映模块或职责的完整名称，避免 `data`、`temp`、`value1` 等无业务含义的名称。

## 四、模块接口规则

- 一个公开函数名称包含模块名、动作和对象；例如 `vofa_justfloat_encode_float`，不要使用大驼峰或数字序号式名称。
- 公开宏应说明用途，表达数量时必须体现单位；例如帧大小为字节时使用 `VOFA_JUSTFLOAT_FRAME_SIZE_BYTES`。
- 公开头文件只暴露本模块需要的接口；文件内辅助函数声明为 `static`。
- 重命名公开项目接口时，必须同步修改仓库内所有调用点、测试、交接文档和自动检查器中的旧名称清单。不保留未经明确需求批准的兼容别名。

## 五、强制复查流程

每次新增、生成或修改项目自有 C 代码后，按以下顺序完成复查：

1. 在写实现前确认新增标识符符合本文件的格式、模块前缀、语义和单位规则。
2. 先补充或修改相关自动化测试，并观察其在实现前按预期失败。
3. 使用 PowerShell 运行命名检查器：

   ```powershell
   $env:PYTHONDONTWRITEBYTECODE='1'
   py tests\check_c_naming.py
   ```

4. 修复所有报告为 `路径:行号:规则:标识符` 的违规，直到输出 `C naming check: PASS`。
5. 人工复查新增标识符的模块归属、动作/对象语义、单位后缀、外部接口例外理由和 CubeMX `USER CODE` 边界。自动检查不能替代这一步。
6. 运行与变更风险相匹配的主机测试和 Keil 构建；只有全部通过才可更新交接文档中的验证状态。

当前检查器位于 `tests\check_c_naming.py`，其单元测试位于 `tests\check_c_naming_test.py`。它固定检查 `Core\Inc\vofa_justfloat.h`、`Core\Src\vofa_justfloat.c`、`Core\Src\freertos.c` 的 `USER CODE` 区域以及 `tests\vofa_justfloat_test.c`。以后扩大项目自有代码范围时，必须先为新规则写失败测试，再同步扩大检查器范围；不得让新文件绕过检查。
