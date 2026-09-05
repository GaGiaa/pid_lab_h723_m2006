/**
  ******************************************************************************
  * @file    m2006_control_task.c
  * @brief   M2006 电机 1kHz 电流开环控制任务实现
  * @note    任务由 freertos.c 中 osThreadNew 创建；本文件为纯业务代码，
  *          与 CubeMX 生成文件解耦。
  ******************************************************************************
  */
#include "m2006_control_task.h"
#include "m2006_driver.h"

osThreadId_t m2006_control_task_handle;
const osThreadAttr_t m2006_control_task_attributes = {
  .name = "m2006_control",
  .stack_size = 1024U,
  .priority = (osPriority_t) osPriorityAboveNormal,
};

void m2006_control_task_entry(void *argument)
{
  m2006_driver_init();

  (void)argument;

  for (;;)
  {
    m2006_driver_update();
    osDelay(1U);
  }
}
