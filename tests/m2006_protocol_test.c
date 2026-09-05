#include <stdint.h>
#include <stdio.h>

#include "m2006_protocol.h"

static int expect_bytes_equal(const uint8_t *actual_bytes,
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

static int test_encode_control_id2_positive_current(void)
{
  static const uint8_t expected_frame[M2006_PROTOCOL_FRAME_BYTES] = {
    0x00U, 0x00U, 0x03U, 0xE8U, 0x00U, 0x00U, 0x00U, 0x00U
  };
  uint8_t encoded_frame[M2006_PROTOCOL_FRAME_BYTES];

  if (m2006_protocol_encode_control(2U, 1000, encoded_frame)
      != M2006_PROTOCOL_FRAME_BYTES)
  {
    return 0;
  }

  return expect_bytes_equal(encoded_frame,
                            expected_frame,
                            M2006_PROTOCOL_FRAME_BYTES);
}

static int test_encode_control_id2_negative_current(void)
{
  static const uint8_t expected_frame[M2006_PROTOCOL_FRAME_BYTES] = {
    0x00U, 0x00U, 0xFCU, 0x18U, 0x00U, 0x00U, 0x00U, 0x00U
  };
  uint8_t encoded_frame[M2006_PROTOCOL_FRAME_BYTES];

  if (m2006_protocol_encode_control(2U, -1000, encoded_frame)
      != M2006_PROTOCOL_FRAME_BYTES)
  {
    return 0;
  }

  return expect_bytes_equal(encoded_frame,
                            expected_frame,
                            M2006_PROTOCOL_FRAME_BYTES);
}

static int test_encode_control_id1_uses_first_two_bytes(void)
{
  static const uint8_t expected_frame[M2006_PROTOCOL_FRAME_BYTES] = {
    0x03U, 0xE8U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U
  };
  uint8_t encoded_frame[M2006_PROTOCOL_FRAME_BYTES];

  if (m2006_protocol_encode_control(1U, 1000, encoded_frame)
      != M2006_PROTOCOL_FRAME_BYTES)
  {
    return 0;
  }

  return expect_bytes_equal(encoded_frame,
                            expected_frame,
                            M2006_PROTOCOL_FRAME_BYTES);
}

static int test_encode_control_rejects_bad_arguments(void)
{
  uint8_t encoded_frame[M2006_PROTOCOL_FRAME_BYTES];

  if (m2006_protocol_encode_control(0U, 0, encoded_frame) != 0U)
  {
    return 0;
  }

  if (m2006_protocol_encode_control(5U, 0, encoded_frame) != 0U)
  {
    return 0;
  }

  if (m2006_protocol_encode_control(1U, 0, (uint8_t *)0) != 0U)
  {
    return 0;
  }

  return 1;
}

static int test_parse_feedback_values(void)
{
  static const uint8_t feedback_frame[M2006_PROTOCOL_FRAME_BYTES] = {
    0x12U, 0x34U, 0xFFU, 0x9CU, 0x03U, 0xE8U, 0x00U, 0x00U
  };
  m2006_measure_t measure;

  if (m2006_protocol_parse_feedback(feedback_frame, &measure) != 1U)
  {
    return 0;
  }

  if (measure.angle_raw != 0x1234U)
  {
    return 0;
  }

  /* 0xFF9C 作为 int16 为 -100 */
  if (measure.speed_rpm != -100)
  {
    return 0;
  }

  if (measure.torque_raw != 1000)
  {
    return 0;
  }

  return 1;
}

static int test_parse_feedback_rejects_null_arguments(void)
{
  uint8_t feedback_frame[M2006_PROTOCOL_FRAME_BYTES] = {0U};
  m2006_measure_t measure;

  if (m2006_protocol_parse_feedback((const uint8_t *)0, &measure) != 0U)
  {
    return 0;
  }

  if (m2006_protocol_parse_feedback(feedback_frame, (m2006_measure_t *)0) != 0U)
  {
    return 0;
  }

  return 1;
}

static int test_unwrap_angle_forward(void)
{
  return m2006_protocol_unwrap_angle(100U, 200U) == 100;
}

static int test_unwrap_angle_wrap_forward(void)
{
  return m2006_protocol_unwrap_angle(8000U, 100U) == 292;
}

static int test_unwrap_angle_wrap_backward(void)
{
  return m2006_protocol_unwrap_angle(100U, 8000U) == -292;
}

static int test_unwrap_angle_zero(void)
{
  return m2006_protocol_unwrap_angle(5000U, 5000U) == 0;
}

static int test_unwrap_angle_boundary(void)
{
  return m2006_protocol_unwrap_angle(8191U, 0U) == 1;
}

int main(void)
{
  if (!test_encode_control_id2_positive_current())
  {
    return 1;
  }

  if (!test_encode_control_id2_negative_current())
  {
    return 2;
  }

  if (!test_encode_control_id1_uses_first_two_bytes())
  {
    return 3;
  }

  if (!test_encode_control_rejects_bad_arguments())
  {
    return 4;
  }

  if (!test_parse_feedback_values())
  {
    return 5;
  }

  if (!test_parse_feedback_rejects_null_arguments())
  {
    return 6;
  }

  if (!test_unwrap_angle_forward())
  {
    return 7;
  }

  if (!test_unwrap_angle_wrap_forward())
  {
    return 8;
  }

  if (!test_unwrap_angle_wrap_backward())
  {
    return 9;
  }

  if (!test_unwrap_angle_zero())
  {
    return 10;
  }

  if (!test_unwrap_angle_boundary())
  {
    return 11;
  }

  (void)printf("m2006_protocol_test: PASS\n");
  return 0;
}
