"""Unit tests for the project C naming checker."""

from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

import check_c_naming


HEADER_SOURCE = """\
#ifndef VOFA_JUSTFLOAT_H
#define VOFA_JUSTFLOAT_H

#include <stdint.h>

#define VOFA_JUSTFLOAT_FRAME_SIZE_BYTES (8U)

uint32_t vofa_justfloat_encode_float(
    float value,
    uint8_t encoded_frame[VOFA_JUSTFLOAT_FRAME_SIZE_BYTES]);

#endif /* VOFA_JUSTFLOAT_H */
"""


VOFA_SOURCE = """\
#include \"vofa_justfloat.h\"

uint32_t vofa_justfloat_encode_float(
    float value,
    uint8_t encoded_frame[VOFA_JUSTFLOAT_FRAME_SIZE_BYTES])
{
  (void)value;
  return encoded_frame == 0 ? 0U : VOFA_JUSTFLOAT_FRAME_SIZE_BYTES;
}
"""


TEST_SOURCE = """\
#include <stdint.h>

static int expect_bytes_equal(
    const uint8_t *actual_bytes,
    const uint8_t *expected_bytes,
    uint32_t byte_count)
{
  uint32_t byte_index;

  for (byte_index = 0U; byte_index < byte_count; ++byte_index)
  {
    if (actual_bytes[byte_index] != expected_bytes[byte_index])
    {
      return 0;
    }
  }

  return 1;
}

int main(void)
{
  return expect_bytes_equal(0, 0, 0U);
}
"""


M2006_PROTOCOL_HEADER_SOURCE = """\
#ifndef M2006_PROTOCOL_H
#define M2006_PROTOCOL_H

#include <stdint.h>

#define M2006_PROTOCOL_FRAME_BYTES (8U)

typedef struct m2006_measure
{
  uint16_t angle_raw;
} m2006_measure_t;

uint32_t m2006_protocol_parse_feedback(
    const uint8_t feedback_data[M2006_PROTOCOL_FRAME_BYTES],
    m2006_measure_t *measure);

#endif /* M2006_PROTOCOL_H */
"""


M2006_PROTOCOL_SOURCE = """\
#include \"m2006_protocol.h\"

uint32_t m2006_protocol_parse_feedback(
    const uint8_t feedback_data[M2006_PROTOCOL_FRAME_BYTES],
    m2006_measure_t *measure)
{
  if ((feedback_data == 0) || (measure == 0))
  {
    return 0U;
  }

  measure->angle_raw = (uint16_t)feedback_data[0];
  return 1U;
}
"""


M2006_MOTOR_HEADER_SOURCE = """\
#ifndef M2006_MOTOR_H
#define M2006_MOTOR_H

#include <stdint.h>

#include "m2006_protocol.h"

typedef enum
{
  M2006_MOTOR_MODE_OPEN_LOOP = 0,
  M2006_MOTOR_MODE_SPEED,
  M2006_MOTOR_MODE_POSITION,
} m2006_motor_mode_t;

typedef struct m2006_motor
{
  m2006_motor_mode_t mode;
  int16_t output_current;
} m2006_motor_t;

void m2006_motor_init(m2006_motor_t *motor, uint8_t esc_id);
void m2006_motor_update(m2006_motor_t *motor, uint32_t tick_ms);

#endif /* M2006_MOTOR_H */
"""


M2006_MOTOR_SOURCE = """\
#include "m2006_motor.h"

void m2006_motor_init(m2006_motor_t *motor, uint8_t esc_id)
{
  (void)esc_id;
  motor->mode = M2006_MOTOR_MODE_OPEN_LOOP;
  motor->output_current = 0;
}

void m2006_motor_update(m2006_motor_t *motor, uint32_t tick_ms)
{
  (void)tick_ms;
  motor->output_current = 0;
}
"""


M2006_BUS_HEADER_SOURCE = """\
#ifndef M2006_BUS_H
#define M2006_BUS_H

#include <stdint.h>

#include "m2006_motor.h"
#include "m2006_protocol.h"

typedef struct m2006_bus
{
  m2006_motor_t *motor_slots[8];
} m2006_bus_t;

void m2006_bus_init(m2006_bus_t *bus);
uint32_t m2006_bus_attach_motor(m2006_bus_t *bus,
                                m2006_motor_t *motor,
                                uint8_t esc_id);
uint32_t m2006_bus_pack_tx_frames(m2006_bus_t *bus,
                                  uint32_t *frame_id,
                                  uint8_t (*frame_data)[8]);
void m2006_bus_handle_rx_frame(m2006_bus_t *bus,
                               uint32_t can_id,
                               const uint8_t *frame_data,
                               uint32_t tick_ms);

#endif /* M2006_BUS_H */
"""


M2006_BUS_SOURCE = """\
#include "m2006_bus.h"

void m2006_bus_init(m2006_bus_t *bus)
{
  uint32_t slot_index;

  for (slot_index = 0U; slot_index < 8U; ++slot_index)
  {
    bus->motor_slots[slot_index] = 0;
  }
}

uint32_t m2006_bus_attach_motor(m2006_bus_t *bus,
                                m2006_motor_t *motor,
                                uint8_t esc_id)
{
  (void)bus;
  (void)motor;
  (void)esc_id;
  return 1U;
}

uint32_t m2006_bus_pack_tx_frames(m2006_bus_t *bus,
                                  uint32_t *frame_id,
                                  uint8_t (*frame_data)[8])
{
  (void)bus;
  (void)frame_id;
  (void)frame_data;
  return 0U;
}

void m2006_bus_handle_rx_frame(m2006_bus_t *bus,
                               uint32_t can_id,
                               const uint8_t *frame_data,
                               uint32_t tick_ms)
{
  (void)bus;
  (void)can_id;
  (void)frame_data;
  (void)tick_ms;
}
"""


M2006_HAL_HEADER_SOURCE = """\
#ifndef M2006_HAL_H
#define M2006_HAL_H

#include <stdint.h>

#include "m2006_bus.h"

extern m2006_bus_t m2006_hal_bus;
extern uint32_t m2006_hal_tx_fail_count;

void m2006_hal_init(void);
uint32_t m2006_hal_tx_frame(uint32_t frame_id,
                            const uint8_t *frame_data);

#endif /* M2006_HAL_H */
"""


M2006_HAL_SOURCE = """\
#include "m2006_hal.h"

m2006_bus_t m2006_hal_bus;
uint32_t m2006_hal_tx_fail_count = 0U;

void m2006_hal_init(void)
{
  m2006_hal_tx_fail_count = 0U;
}

uint32_t m2006_hal_tx_frame(uint32_t frame_id,
                            const uint8_t *frame_data)
{
  (void)frame_id;
  (void)frame_data;
  return 1U;
}

void HAL_FDCAN_RxFifo0Callback(void *hfdcan, uint32_t rx_fifo0_it_flags)
{
  (void)hfdcan;
  (void)rx_fifo0_it_flags;
}
"""


M2006_PROTOCOL_TEST_SOURCE = """\
#include <stdint.h>

int main(void)
{
  return 0;
}
"""


M2006_MOTOR_TEST_SOURCE = """\
#include <stdint.h>

int main(void)
{
  return 0;
}
"""


M2006_BUS_TEST_SOURCE = """\
#include <stdint.h>

int main(void)
{
  return 0;
}
"""


M2006_CONTROL_TASK_HEADER_SOURCE = """\
#ifndef M2006_CONTROL_TASK_H
#define M2006_CONTROL_TASK_H

#include "cmsis_os.h"

extern osThreadId_t m2006_control_task_handle;
extern const osThreadAttr_t m2006_control_task_attributes;

void m2006_control_task_entry(void *argument);

#endif /* M2006_CONTROL_TASK_H */
"""


M2006_CONTROL_TASK_SOURCE = """\
#include "m2006_control_task.h"

osThreadId_t m2006_control_task_handle;
const osThreadAttr_t m2006_control_task_attributes = {
  .name = "m2006_control",
};

void m2006_control_task_entry(void *argument)
{
  (void)argument;

  for (;;)
  {
  }
}
"""


VOFA_TIMESTAMP_TASK_HEADER_SOURCE = """\
#ifndef VOFA_TIMESTAMP_TASK_H
#define VOFA_TIMESTAMP_TASK_H

#include "cmsis_os.h"

extern osThreadId_t vofa_timestamp_task_handle;
extern const osThreadAttr_t vofa_timestamp_task_attributes;

void vofa_timestamp_task_entry(void *argument);

#endif /* VOFA_TIMESTAMP_TASK_H */
"""


VOFA_TIMESTAMP_TASK_SOURCE = """\
#include "vofa_timestamp_task.h"

osThreadId_t vofa_timestamp_task_handle;
const osThreadAttr_t vofa_timestamp_task_attributes = {
  .name = "vofa_timestamp",
};

void vofa_timestamp_task_entry(void *argument)
{
  (void)argument;

  for (;;)
  {
  }
}
"""


FREERTOS_SOURCE = """\
/* USER CODE BEGIN PD */
#define VOFA_TIMESTAMP_PERIOD_MS (100U)
/* USER CODE END PD */

/* USER CODE BEGIN Variables */
osThreadId_t vofa_timestamp_task_handle;
const osThreadAttr_t vofa_timestamp_task_attributes = {
  .name = \"vofa_timestamp\",
};
osThreadId_t m2006_control_task_handle;
const osThreadAttr_t m2006_control_task_attributes = {
  .name = \"m2006_control\",
};
/* USER CODE END Variables */

/* USER CODE BEGIN FunctionPrototypes */
static void vofa_timestamp_task_entry(void *argument);
/* USER CODE END FunctionPrototypes */

/* USER CODE BEGIN 4 */
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName)
{
  (void)xTask;
  (void)pcTaskName;
}
/* USER CODE END 4 */

/* USER CODE BEGIN RTOS_THREADS */
vofa_timestamp_task_handle = osThreadNew(
    vofa_timestamp_task_entry,
    NULL,
    &vofa_timestamp_task_attributes);
/* USER CODE END RTOS_THREADS */

/* USER CODE BEGIN Application */
static void vofa_timestamp_task_entry(void *argument)
{
  uint8_t encoded_frame[VOFA_JUSTFLOAT_FRAME_SIZE_BYTES];

  (void)argument;
  (void)HAL_UART_Transmit_DMA(&huart8, encoded_frame,
                              VOFA_JUSTFLOAT_FRAME_SIZE_BYTES);
}
/* USER CODE END Application */
"""


PID_HEADER_SOURCE = """\
#ifndef PID_H
#define PID_H

#include <stdint.h>

typedef enum
{
  PID_OK = 0,
  PID_ERR_INVALID_DT,
  PID_ERR_INVALID_LIMIT,
} pid_status_t;

typedef struct
{
  float kp;
  float ki;
  float kd;
  float dt;
  float out_min;
  float out_max;
  float error;
  float output;
  uint8_t is_valid;
} pid_t;

typedef struct
{
  float kp;
  float ki;
  float kd;
  float dt;
  float output;
  uint8_t is_valid;
} pid_inc_t;

pid_status_t pid_init(pid_t *pid);
void pid_reset(pid_t *pid);
float pid_update(pid_t *pid, float setpoint, float measurement);

pid_status_t pid_inc_init(pid_inc_t *pid);
void pid_inc_reset(pid_inc_t *pid);
float pid_inc_update(pid_inc_t *pid, float setpoint, float measurement);

#endif /* PID_H */
"""


PID_SOURCE = """\
#include \"pid.h\"

pid_status_t pid_init(pid_t *pid)
{
  if (pid->dt <= 0.0f)
  {
    pid->is_valid = 0U;
    return PID_ERR_INVALID_DT;
  }
  pid_reset(pid);
  pid->is_valid = 1U;
  return PID_OK;
}

void pid_reset(pid_t *pid)
{
  pid->error = 0.0f;
  pid->output = 0.0f;
}

float pid_update(pid_t *pid, float setpoint, float measurement)
{
  if (pid->dt <= 0.0f)
  {
    return 0.0f;
  }
  pid->error = setpoint - measurement;
  pid->output = pid->kp * pid->error;
  return pid->output;
}

pid_status_t pid_inc_init(pid_inc_t *pid)
{
  if (pid->dt <= 0.0f)
  {
    pid->is_valid = 0U;
    return PID_ERR_INVALID_DT;
  }
  pid_inc_reset(pid);
  pid->is_valid = 1U;
  return PID_OK;
}

void pid_inc_reset(pid_inc_t *pid)
{
  pid->output = 0.0f;
}

float pid_inc_update(pid_inc_t *pid, float setpoint, float measurement)
{
  if (pid->dt <= 0.0f)
  {
    return 0.0f;
  }
  pid->output += pid->kp * (setpoint - measurement);
  return pid->output;
}
"""


PID_TEST_SOURCE = """\
#include \"pid.h\"

int main(void)
{
  pid_t pid;
  pid.kp = 1.0f;
  pid.dt = 0.001f;
  pid.out_min = -100.0f;
  pid.out_max = 100.0f;
  if (pid_init(&pid) != PID_OK)
  {
    return 1;
  }
  float output = pid_update(&pid, 10.0f, 0.0f);
  if (output <= 0.0f)
  {
    return 1;
  }
  return 0;
}
"""


class NamingCheckerTestCase(unittest.TestCase):
  def create_project(
      self,
      project_root: Path,
      *,
      header_source: str = HEADER_SOURCE,
      vofa_source: str = VOFA_SOURCE,
      test_source: str = TEST_SOURCE,
      freertos_source: str = FREERTOS_SOURCE,
      m2006_protocol_header_source: str = M2006_PROTOCOL_HEADER_SOURCE,
      m2006_protocol_source: str = M2006_PROTOCOL_SOURCE,
      m2006_motor_header_source: str = M2006_MOTOR_HEADER_SOURCE,
      m2006_motor_source: str = M2006_MOTOR_SOURCE,
      m2006_bus_header_source: str = M2006_BUS_HEADER_SOURCE,
      m2006_bus_source: str = M2006_BUS_SOURCE,
      m2006_hal_header_source: str = M2006_HAL_HEADER_SOURCE,
      m2006_hal_source: str = M2006_HAL_SOURCE,
      m2006_protocol_test_source: str = M2006_PROTOCOL_TEST_SOURCE,
      m2006_motor_test_source: str = M2006_MOTOR_TEST_SOURCE,
      m2006_bus_test_source: str = M2006_BUS_TEST_SOURCE,
      m2006_control_task_header_source: str = M2006_CONTROL_TASK_HEADER_SOURCE,
      m2006_control_task_source: str = M2006_CONTROL_TASK_SOURCE,
      vofa_timestamp_task_header_source: str = VOFA_TIMESTAMP_TASK_HEADER_SOURCE,
      vofa_timestamp_task_source: str = VOFA_TIMESTAMP_TASK_SOURCE,
      pid_header_source: str = PID_HEADER_SOURCE,
      pid_source: str = PID_SOURCE,
      pid_test_source: str = PID_TEST_SOURCE,
  ) -> None:
    files = {
        "App/Inc/driver/vofa_justfloat.h": header_source,
        "App/Src/driver/vofa_justfloat.c": vofa_source,
        "Core/Src/freertos.c": freertos_source,
        "tests/vofa_justfloat_test.c": test_source,
        "App/Inc/task/m2006_control_task.h": m2006_control_task_header_source,
        "App/Src/task/m2006_control_task.c": m2006_control_task_source,
        "App/Inc/task/vofa_timestamp_task.h": vofa_timestamp_task_header_source,
        "App/Src/task/vofa_timestamp_task.c": vofa_timestamp_task_source,
        "App/Inc/driver/m2006_hal.h": m2006_hal_header_source,
        "App/Src/driver/m2006_hal.c": m2006_hal_source,
        "Lib/m2006_lib/include/m2006_protocol.h": m2006_protocol_header_source,
        "Lib/m2006_lib/src/m2006_protocol.c": m2006_protocol_source,
        "Lib/m2006_lib/include/m2006_motor.h": m2006_motor_header_source,
        "Lib/m2006_lib/src/m2006_motor.c": m2006_motor_source,
        "Lib/m2006_lib/include/m2006_bus.h": m2006_bus_header_source,
        "Lib/m2006_lib/src/m2006_bus.c": m2006_bus_source,
        "Lib/m2006_lib/tests/m2006_protocol_test.c": m2006_protocol_test_source,
        "Lib/m2006_lib/tests/m2006_motor_test.c": m2006_motor_test_source,
        "Lib/m2006_lib/tests/m2006_bus_test.c": m2006_bus_test_source,
        "Lib/pid_lib/include/pid.h": pid_header_source,
        "Lib/pid_lib/src/pid.c": pid_source,
        "tests/pid_test.c": pid_test_source,
    }

    for relative_path, source in files.items():
      file_path = project_root / relative_path
      file_path.parent.mkdir(parents=True, exist_ok=True)
      file_path.write_text(source, encoding="utf-8")

  def scan_project(self, **sources: str) -> list[check_c_naming.NamingViolation]:
    with tempfile.TemporaryDirectory() as temporary_directory:
      project_root = Path(temporary_directory)
      self.create_project(project_root, **sources)
      return check_c_naming.scan_project(project_root)

  def assert_has_violation(
      self,
      violations: list[check_c_naming.NamingViolation],
      rule: str,
      identifier: str,
  ) -> None:
    self.assertIn(
        (rule, identifier),
        {(violation.rule, violation.identifier) for violation in violations},
    )

  def test_compliant_project_has_no_violations(self) -> None:
    self.assertEqual(self.scan_project(), [])

  def test_reports_camel_case_variable(self) -> None:
    vofa_source = VOFA_SOURCE.replace(
        "(void)value;",
        "uint32_t camelCase = 0U;\n  (void)camelCase;\n  (void)value;",
    )

    violations = self.scan_project(vofa_source=vofa_source)

    self.assert_has_violation(violations, "identifier_style", "camelCase")

  def test_reports_later_initialized_camel_case_declarator(self) -> None:
    vofa_source = VOFA_SOURCE.replace(
        "(void)value;",
        "uint32_t good_value = 0U, camelCase = 0U;\n"
        "  (void)good_value;\n"
        "  (void)camelCase;\n"
        "  (void)value;",
    )

    violations = self.scan_project(vofa_source=vofa_source)

    self.assert_has_violation(violations, "identifier_style", "camelCase")

  def test_reports_camel_case_type_alias_and_tag(self) -> None:
    vofa_source = VOFA_SOURCE + """\

typedef int CamelCaseType;
struct CamelCaseTag {
  int member_value;
};
"""

    violations = self.scan_project(vofa_source=vofa_source)

    self.assert_has_violation(violations, "identifier_style", "CamelCaseType")
    self.assert_has_violation(violations, "identifier_style", "CamelCaseTag")

  def test_ignores_legacy_name_in_non_task_string(self) -> None:
    vofa_source = VOFA_SOURCE.replace(
        "(void)value;",
        "const char *message = \"VofaJustFloatEncode1\";\n"
        "  (void)message;\n"
        "  (void)value;",
    )

    violations = self.scan_project(vofa_source=vofa_source)

    self.assertEqual(violations, [])

  def test_reports_multiline_parameter_at_its_own_line(self) -> None:
    vofa_source = """\
uint32_t vofa_justfloat_encode_float(
    uint32_t camelCase)
{
  return 0U;
}
"""

    violations = self.scan_project(vofa_source=vofa_source)
    matching_violations = [
        violation
        for violation in violations
        if violation.identifier == "camelCase"
    ]

    self.assertEqual(len(matching_violations), 1)
    self.assertEqual(matching_violations[0].line, 2)

  def test_ignores_return_statement_when_scanning_declarations(self) -> None:
    vofa_source = """\
uint32_t vofa_justfloat_encode_float(void)
{
  return VOFA_JUSTFLOAT_FRAME_SIZE_BYTES;
}
"""

    violations = self.scan_project(vofa_source=vofa_source)

    self.assertEqual(violations, [])

  def test_reports_reserved_header_guard(self) -> None:
    header_source = HEADER_SOURCE.replace(
        "VOFA_JUSTFLOAT_H",
        "__VOFA_JUSTFLOAT_H__",
    )

    violations = self.scan_project(header_source=header_source)

    self.assert_has_violation(
        violations,
        "header_guard_reserved_identifier",
        "__VOFA_JUSTFLOAT_H__",
    )

  def test_reports_header_guard_without_module_prefix(self) -> None:
    header_source = HEADER_SOURCE.replace(
        "VOFA_JUSTFLOAT_H",
        "JUSTFLOAT_H",
    )

    violations = self.scan_project(header_source=header_source)

    self.assert_has_violation(
        violations,
        "macro_module_prefix",
        "JUSTFLOAT_H",
    )

  def test_reports_conditional_macro_without_module_prefix(self) -> None:
    vofa_source = """\
#ifndef LOCAL_FEATURE_ENABLED
#define LOCAL_FEATURE_ENABLED 1
#endif

""" + VOFA_SOURCE

    violations = self.scan_project(vofa_source=vofa_source)

    self.assert_has_violation(
        violations,
        "macro_module_prefix",
        "LOCAL_FEATURE_ENABLED",
    )

  def test_reports_public_function_without_module_prefix(self) -> None:
    header_source = HEADER_SOURCE.replace(
        "vofa_justfloat_encode_float",
        "encode_float",
    )
    vofa_source = VOFA_SOURCE.replace(
        "vofa_justfloat_encode_float",
        "encode_float",
    )

    violations = self.scan_project(
        header_source=header_source,
        vofa_source=vofa_source,
    )

    self.assert_has_violation(
        violations,
        "public_function_module_prefix",
        "encode_float",
    )

  def test_allows_fixed_external_symbols(self) -> None:
    vofa_source = """\
void MX_FREERTOS_Init(void)
{
}

void startDefaultTask(void *argument)
{
  UART_HandleTypeDef *uart_handle = &huart8;

  (void)argument;
  (void)uart_handle;
}
"""

    violations = self.scan_project(vofa_source=vofa_source)

    self.assertEqual(violations, [])

  def test_m2006_compliant_project_has_no_violations(self) -> None:
    self.assertEqual(self.scan_project(), [])

  def test_reports_m2006_camel_case_variable(self) -> None:
    m2006_hal_source = M2006_HAL_SOURCE.replace(
        "m2006_hal_tx_fail_count = 0U;",
        "m2006_hal_tx_fail_count = 0U;\n"
        "  uint32_t camelCase = 0U;\n"
        "  (void)camelCase;",
    )

    violations = self.scan_project(m2006_hal_source=m2006_hal_source)

    self.assert_has_violation(violations, "identifier_style", "camelCase")

  def test_reports_m2006_public_function_without_prefix(self) -> None:
    m2006_hal_source = M2006_HAL_SOURCE.replace(
        "uint32_t m2006_hal_tx_frame(uint32_t frame_id,",
        "uint32_t tx_frame(uint32_t frame_id,",
    )

    violations = self.scan_project(m2006_hal_source=m2006_hal_source)

    self.assert_has_violation(
        violations,
        "public_function_module_prefix",
        "tx_frame",
    )

  def test_reports_m2006_macro_without_module_prefix(self) -> None:
    m2006_protocol_header_source = M2006_PROTOCOL_HEADER_SOURCE.replace(
        "M2006_PROTOCOL_FRAME_BYTES (8U)",
        "FRAME_BYTES (8U)",
    )

    violations = self.scan_project(
        m2006_protocol_header_source=m2006_protocol_header_source
    )

    self.assert_has_violation(violations, "macro_module_prefix", "FRAME_BYTES")

  def test_reports_legacy_identifier(self) -> None:
    header_source = HEADER_SOURCE.replace(
        "VOFA_JUSTFLOAT_FRAME_SIZE_BYTES",
        "VOFA_JUSTFLOAT_FRAME_SIZE",
    )

    violations = self.scan_project(header_source=header_source)

    self.assert_has_violation(
        violations,
        "legacy_identifier",
        "VOFA_JUSTFLOAT_FRAME_SIZE",
    )

  def test_reports_legacy_identifier_use(self) -> None:
    vofa_source = """\
uint32_t vofa_justfloat_encode_float(void)
{
  return VOFA_JUSTFLOAT_FRAME_SIZE;
}
"""

    violations = self.scan_project(vofa_source=vofa_source)

    self.assert_has_violation(
        violations,
        "legacy_identifier",
        "VOFA_JUSTFLOAT_FRAME_SIZE",
    )

  def test_violation_format_uses_relative_path_and_line(self) -> None:
    violation = check_c_naming.NamingViolation(
        relative_path="Core/Src/vofa_justfloat.c",
        line=12,
        rule="identifier_style",
        identifier="camelCase",
    )

    self.assertEqual(
        violation.format(),
        "Core/Src/vofa_justfloat.c:12:identifier_style:camelCase",
    )


if __name__ == "__main__":
  unittest.main()

  def test_pid_compliant_project_has_no_violations(self) -> None:
    self.assertEqual(self.scan_project(), [])

  def test_reports_pid_public_function_without_prefix(self) -> None:
    pid_source = PID_SOURCE.replace(
        "float pid_update(pid_t *pid, float setpoint, float measurement)",
        "float update(pid_t *pid, float setpoint, float measurement)",
    )

    violations = self.scan_project(pid_source=pid_source)

    self.assert_has_violation(
        violations,
        "public_function_module_prefix",
        "update",
    )

  def test_reports_pid_inc_public_function_without_prefix(self) -> None:
    pid_source = PID_SOURCE.replace(
        "float pid_inc_update(pid_inc_t *pid, float setpoint, float measurement)",
        "float inc_update(pid_inc_t *pid, float setpoint, float measurement)",
    )

    violations = self.scan_project(pid_source=pid_source)

    self.assert_has_violation(
        violations,
        "public_function_module_prefix",
        "inc_update",
    )
