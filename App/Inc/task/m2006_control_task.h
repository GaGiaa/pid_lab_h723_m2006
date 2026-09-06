/**
  ******************************************************************************
  * @file    m2006_control_task.h
  * @brief   M2006 电机 1kHz 控制任务（RTOS 任务声明）
  * @note    任务句柄、属性与入口均定义于 m2006_control_task.c；
  *          freertos.c 的 USER CODE 区仅通过本头文件调用 osThreadNew 创建任务。
  *          控制逻辑由 m2006_lib 提供（m2006_motor 电机实例 + m2006_bus 总线实例），
  *          本任务只做装配与编排。
  ******************************************************************************
  */
#ifndef M2006_CONTROL_TASK_H
#define M2006_CONTROL_TASK_H

#include "cmsis_os.h"

#include "m2006_motor.h"

/* 任务句柄（osThreadNew 返回值写入，供外部查询/操作） */
extern osThreadId_t m2006_control_task_handle;

/* 任务属性：名称、栈大小、优先级 */
extern const osThreadAttr_t m2006_control_task_attributes;

/* 本工程电机实例（esc_id=2）：Keil Watch 添加 m2006_motor 即可查看/修改全部
   配置与观测字段（等价于原 m2006_debug + m2006_control_debug 两个面板） */
extern m2006_motor_t m2006_motor;

/**
  * @brief  M2006 控制任务入口：初始化 HAL/电机/总线后以 1kHz 周期
  *         执行"闭环计算 → 总线聚合 → 发送"
  * @param  argument 未使用
  */
void m2006_control_task_entry(void *argument);

#endif /* M2006_CONTROL_TASK_H */
