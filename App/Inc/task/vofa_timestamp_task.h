/**
  ******************************************************************************
  * @file    vofa_timestamp_task.h
  * @brief   VOFA 时间戳上报任务（RTOS 任务声明）
  * @note    任务句柄、属性与入口均定义于 vofa_timestamp_task.c；
  *          freertos.c 的 USER CODE 区仅通过本头文件调用 osThreadNew 创建任务。
  ******************************************************************************
  */
#ifndef VOFA_TIMESTAMP_TASK_H
#define VOFA_TIMESTAMP_TASK_H

#include "cmsis_os.h"

/* 任务句柄（osThreadNew 返回值写入，供外部查询/操作） */
extern osThreadId_t vofa_timestamp_task_handle;

/* 任务属性：名称、栈大小、优先级 */
extern const osThreadAttr_t vofa_timestamp_task_attributes;

/**
  * @brief  VOFA 时间戳任务入口：周期通过 UART8 DMA 上报 RTOS 毫秒时间戳
  * @param  argument 未使用
  */
void vofa_timestamp_task_entry(void *argument);

#endif /* VOFA_TIMESTAMP_TASK_H */
