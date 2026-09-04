/**
  ******************************************************************************
  * @file    m2006_protocol.c
  * @brief   M2006 电机配合 C610 电调的 CAN 协议编解码实现
  * @note    本模块不依赖 STM32 HAL 或 RTOS，可在主机端直接编译测试。
  ******************************************************************************
  */
#include "m2006_protocol.h"

uint32_t m2006_protocol_parse_feedback(
    const uint8_t feedback_data[M2006_PROTOCOL_FRAME_BYTES],
    m2006_measure_t *measure)
{
  if ((feedback_data == 0) || (measure == 0))
  {
    return 0U;
  }

  measure->angle_raw = (uint16_t)((uint16_t)feedback_data[0] << 8U)
                       | (uint16_t)feedback_data[1];
  measure->speed_rpm = (int16_t)((uint16_t)((uint16_t)feedback_data[2] << 8U)
                                 | (uint16_t)feedback_data[3]);
  measure->torque_raw = (int16_t)((uint16_t)((uint16_t)feedback_data[4] << 8U)
                                  | (uint16_t)feedback_data[5]);

  return 1U;
}

uint32_t m2006_protocol_encode_control(
    uint8_t motor_id,
    int16_t current_raw,
    uint8_t control_data[M2006_PROTOCOL_FRAME_BYTES])
{
  uint8_t byte_index;
  uint8_t byte_offset;

  if ((control_data == 0)
      || (motor_id < M2006_PROTOCOL_MOTOR_ID_MIN)
      || (motor_id > M2006_PROTOCOL_MOTOR_ID_MAX))
  {
    return 0U;
  }

  for (byte_index = 0U; byte_index < M2006_PROTOCOL_FRAME_BYTES; ++byte_index)
  {
    control_data[byte_index] = 0U;
  }

  byte_offset = (uint8_t)((motor_id - M2006_PROTOCOL_MOTOR_ID_MIN) * 2U);
  control_data[byte_offset] = (uint8_t)((uint16_t)current_raw >> 8U);
  control_data[byte_offset + 1U] = (uint8_t)(current_raw & 0xFFU);

  return M2006_PROTOCOL_FRAME_BYTES;
}
