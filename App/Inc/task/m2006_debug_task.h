/**
  ******************************************************************************
  * @file    m2006_debug_task.h
  * @brief   M2006 波形调试任务（RTOS 任务声明）
  * @note    任务句柄、属性与入口均定义于 m2006_debug_task.c；
  *          freertos.c 的 USER CODE 区仅通过本头文件调用 osThreadNew 创建任务。
  *          本任务替代原 vofa_timestamp_task：以 1kHz 周期读取 m2006_motor
  *          观测区/配置区字段，编码为 JustFloat 多通道帧经 UART8 DMA 发送，
  *          供 VOFA 示波器实时观测速度环/位置环波形。
  ******************************************************************************
  */
#ifndef M2006_DEBUG_TASK_H
#define M2006_DEBUG_TASK_H

#include "cmsis_os.h"

/* 任务句柄（osThreadNew 返回值写入，供外部查询/操作） */
extern osThreadId_t m2006_debug_task_handle;

/* 任务属性：名称、栈大小、优先级 */
extern const osThreadAttr_t m2006_debug_task_attributes;

/**
  * @brief  M2006 波形调试任务入口：1kHz 周期读取 m2006_motor 字段并编码发送
  * @param  argument 未使用
  */
void m2006_debug_task_entry(void *argument);

#endif /* M2006_DEBUG_TASK_H */
