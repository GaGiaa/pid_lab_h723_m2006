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


FREERTOS_SOURCE = """\
/* USER CODE BEGIN PD */
#define VOFA_TIMESTAMP_PERIOD_MS (100U)
/* USER CODE END PD */

/* USER CODE BEGIN Variables */
osThreadId_t vofa_timestamp_task_handle;
const osThreadAttr_t vofa_timestamp_task_attributes = {
  .name = \"vofa_timestamp\",
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


class NamingCheckerTestCase(unittest.TestCase):
  def create_project(
      self,
      project_root: Path,
      *,
      header_source: str = HEADER_SOURCE,
      vofa_source: str = VOFA_SOURCE,
      test_source: str = TEST_SOURCE,
      freertos_source: str = FREERTOS_SOURCE,
  ) -> None:
    files = {
        "Core/Inc/vofa_justfloat.h": header_source,
        "Core/Src/vofa_justfloat.c": vofa_source,
        "Core/Src/freertos.c": freertos_source,
        "tests/vofa_justfloat_test.c": test_source,
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
