/**
  ******************************************************************************
  * @file    m2006_debug_task.c
  * @brief   M2006 波形调试任务实现
  * @note    任务由 freertos.c 中 osThreadNew 创建；本文件为纯业务代码，
  *          与 CubeMX 生成文件解耦。
  *
  *         通道约定（JustFloat 多通道帧，VOFA 按 ch0..ch7 依次显示）：
  *           ch0  tick               RTOS 毫秒时间戳（健康检查/丢帧检测）
  *           ch1  speed_setpoint_rpm 速度设定（速度环阶跃输入）
  *           ch2  speed_feedback_rpm 速度反馈（速度环=外环反馈；位置环=内环跟踪）
  *           ch3  output_current     最终下发电流（看饱和/抖振）
  *           ch4  pos_feedback_deg   位置反馈（位置环外环反馈）
  *           ch5  pos_setpoint_deg   位置设定（位置环阶跃输入）
  *           ch6  speed_cmd_rpm      速度指令（位置环输出，判内外环责任）
  *           ch7  spd_pid.i_term     速度环积分增量 Δi（调 ki 专用诊断）
  *
  *         发送者唯一：原 vofa_timestamp_task 已并入本任务（时间戳即 ch0），
  *         UART8 DMA 无并发竞争。
  ******************************************************************************
  */
#include "m2006_debug_task.h"

#include "m2006_control_task.h"
#include "usart.h"
#include "vofa_justfloat.h"

#define M2006_DEBUG_TASK_PERIOD_MS (1U)   /* 上报周期 1ms = 1kHz */
#define M2006_DEBUG_CHANNEL_COUNT (8U)

osThreadId_t m2006_debug_task_handle;
const osThreadAttr_t m2006_debug_task_attributes = {
  .name = "m2006_debug",
  .stack_size = 1024U,
  .priority = (osPriority_t) osPriorityNormal,
};

void m2006_debug_task_entry(void *argument)
{
  static uint8_t frame_buffer[VOFA_JUSTFLOAT_FRAME_SIZE_FOR_CHANNELS(
      M2006_DEBUG_CHANNEL_COUNT)];
  float values[M2006_DEBUG_CHANNEL_COUNT];
  uint32_t frame_size;

  (void)argument;

  for (;;)
  {
    values[0] = (float)osKernelGetTickCount();
    values[1] = m2006_motor.speed_setpoint_rpm;
    values[2] = m2006_motor.speed_feedback_rpm;
    values[3] = (float)m2006_motor.output_current;
    values[4] = m2006_motor.pos_feedback_deg;
    values[5] = m2006_motor.pos_setpoint_deg;
    values[6] = m2006_motor.speed_cmd_rpm;
    values[7] = m2006_motor.spd_pid.i_term;

    frame_size = vofa_justfloat_encode_multi(values,
                                             M2006_DEBUG_CHANNEL_COUNT,
                                             frame_buffer,
                                             sizeof(frame_buffer));
    if (frame_size != 0U)
    {
      (void)HAL_UART_Transmit_DMA(&huart8, frame_buffer, frame_size);
    }

    osDelay(M2006_DEBUG_TASK_PERIOD_MS);
  }
}
