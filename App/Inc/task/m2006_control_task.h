/**
  ******************************************************************************
  * @file    m2006_control_task.h
  * @brief   M2006 电机 1kHz 电流开环控制任务（RTOS 任务声明）
  * @note    任务句柄、属性与入口均定义于 m2006_control_task.c；
  *          freertos.c 的 USER CODE 区仅通过本头文件调用 osThreadNew 创建任务。
  ******************************************************************************
  */
#ifndef M2006_CONTROL_TASK_H
#define M2006_CONTROL_TASK_H

#include "cmsis_os.h"

/* 任务句柄（osThreadNew 返回值写入，供外部查询/操作） */
extern osThreadId_t m2006_control_task_handle;

/* 任务属性：名称、栈大小、优先级 */
extern const osThreadAttr_t m2006_control_task_attributes;

/**
  * @brief  M2006 控制任务入口：初始化驱动后以 1kHz 周期调用 m2006_driver_update
  * @param  argument 未使用
  */
void m2006_control_task_entry(void *argument);

#endif /* M2006_CONTROL_TASK_H */
