/**
  ******************************************************************************
  * @file    m2006_control_task.c
  * @brief   M2006 电机 1kHz 控制任务实现（开环/速度闭环/位置闭环）
  * @note    任务由 freertos.c 中 osThreadNew 创建；本文件为纯业务代码，
  *          与 CubeMX 生成文件解耦。
  *
  *         控制逻辑全部来自 m2006_lib：
  *           - m2006_motor 电机实例：反馈换算 / 累计角度 / 级联闭环 / 安全门；
  *           - m2006_bus 总线实例：控制帧聚合（0x200 / 0x1FF）、反馈分发；
  *           - m2006_hal 本工程适配：FDCAN2 收发。
  *         每周期顺序：先算闭环（m2006_motor_update）→ 总线打包 → HAL 发送。
  ******************************************************************************
  */
#include "m2006_control_task.h"

#include "m2006_hal.h"

osThreadId_t m2006_control_task_handle;
const osThreadAttr_t m2006_control_task_attributes = {
  .name = "m2006_control",
  .stack_size = 1024U,
  .priority = (osPriority_t) osPriorityAboveNormal,
};

/* 本工程电机实例（esc_id=2，FDCAN2 总线） */
m2006_motor_t m2006_motor;

void m2006_control_task_entry(void *argument)
{
  uint32_t tick_ms;
  uint32_t frame_count;
  uint32_t frame_index;
  uint32_t frame_id[2];
  uint8_t frame_data[2][M2006_PROTOCOL_FRAME_BYTES];

  (void)argument;

  m2006_hal_init();
  m2006_motor_init(&m2006_motor, 2U);
  m2006_bus_init(&m2006_hal_bus);
  (void)m2006_bus_attach_motor(&m2006_hal_bus, &m2006_motor, 2U);

  for (;;)
  {
    tick_ms = osKernelGetTickCount();

    /* 电机闭环计算（先算后发；内部含钳位与安全门） */
    (void)m2006_motor_update(&m2006_motor, tick_ms);

    /* 总线聚合打包 → 依次发送 */
    frame_count = m2006_bus_pack_tx_frames(&m2006_hal_bus,
                                           frame_id,
                                           frame_data);
    for (frame_index = 0U; frame_index < frame_count; ++frame_index)
    {
      (void)m2006_hal_tx_frame(frame_id[frame_index], frame_data[frame_index]);
    }

    osDelay(1U);
  }
}
