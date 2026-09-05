/**
  ******************************************************************************
  * @file    vofa_timestamp_task.c
  * @brief   VOFA 时间戳上报任务实现
  * @note    任务由 freertos.c 中 osThreadNew 创建；本文件为纯业务代码，
  *          与 CubeMX 生成文件解耦。
  ******************************************************************************
  */
#include "vofa_timestamp_task.h"
#include "vofa_justfloat.h"
#include "usart.h"

#define VOFA_TIMESTAMP_PERIOD_MS (100U)   /* 上报周期 */

osThreadId_t vofa_timestamp_task_handle;
const osThreadAttr_t vofa_timestamp_task_attributes = {
  .name = "vofa_timestamp",
  .stack_size = 512U,
  .priority = (osPriority_t) osPriorityNormal,
};

void vofa_timestamp_task_entry(void *argument)
{
  uint8_t frame_buffer[VOFA_JUSTFLOAT_FRAME_SIZE_BYTES];

  (void)argument;

  for (;;)
  {
    if (vofa_justfloat_encode_float((float)osKernelGetTickCount(),
                                    frame_buffer)
        == VOFA_JUSTFLOAT_FRAME_SIZE_BYTES)
    {
      (void)HAL_UART_Transmit_DMA(&huart8,
                                  frame_buffer,
                                  VOFA_JUSTFLOAT_FRAME_SIZE_BYTES);
    }

    osDelay(VOFA_TIMESTAMP_PERIOD_MS);
  }
}
