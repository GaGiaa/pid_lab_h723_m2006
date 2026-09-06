/**
  ******************************************************************************
  * @file    m2006_hal.h
  * @brief   M2006 总线在本工程的 FDCAN 适配层（用户工程侧，不属于 m2006_lib）
  *
  * m2006_lib 为纯逻辑库（零 HAL），本模块把库接到具体外设：
  * - 总线实例 m2006_hal_bus 由本模块持有（RX 回调与任务共用）；
  * - RX：FDCAN2 FIFO0 中断 → 取帧 → m2006_bus_handle_rx_frame() 分发；
  * - TX：m2006_hal_tx_frame() 把库打包好的控制帧送入 FDCAN2 发送 FIFO。
  *
  * 调试：Keil Watch 添加 m2006_hal_bus 查看总线挂载，m2006_hal_tx_fail_count
  *       查看发送失败计数。
  ******************************************************************************
  */
#ifndef M2006_HAL_H
#define M2006_HAL_H

#include <stdint.h>

#include "m2006_bus.h"
#include "m2006_protocol.h"

/* 本工程总线实例（FDCAN2）：RX 回调与 1kHz 控制任务共用 */
extern m2006_bus_t m2006_hal_bus;

/* 控制帧发送失败计数（HAL 发送 FIFO 满等），Keil Watch 可查看 */
extern uint32_t m2006_hal_tx_fail_count;

/**
  * @brief  初始化 FDCAN2 滤波并启动接收中断
  * @note   必须在 MX_FDCAN2_Init() 之后调用，建议在控制任务入口调用一次
  */
void m2006_hal_init(void);

/**
  * @brief  发送一帧控制帧到 FDCAN2 发送 FIFO
  * @param  frame_id   标准帧标识符（0x200 或 0x1FF）
  * @param  frame_data 帧数据指针，长度 M2006_PROTOCOL_FRAME_BYTES
  * @retval 1 发送成功；0 发送失败（计数 m2006_hal_tx_fail_count）
  */
uint32_t m2006_hal_tx_frame(uint32_t frame_id,
                            const uint8_t frame_data[M2006_PROTOCOL_FRAME_BYTES]);

#endif /* M2006_HAL_H */
