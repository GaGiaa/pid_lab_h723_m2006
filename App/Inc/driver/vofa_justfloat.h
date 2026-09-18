#ifndef VOFA_JUSTFLOAT_H
#define VOFA_JUSTFLOAT_H

#include <stdint.h>

/* 单通道帧长：4 字节 float32 小端 + 4 字节帧尾 00 00 80 7F */
#define VOFA_JUSTFLOAT_FRAME_SIZE_BYTES (8U)

/* 多通道帧通道数上限（与 VOFA JustFloat 协议无硬上限，本项目约定 8） */
#define VOFA_JUSTFLOAT_MAX_CHANNELS (8U)

/* 按通道数计算帧长：每通道 4 字节 float32 + 4 字节帧尾 */
#define VOFA_JUSTFLOAT_FRAME_SIZE_FOR_CHANNELS(channel_count) \
    (((channel_count) * 4U) + 4U)

/**
  * @brief  编码单个 float 为 JustFloat 帧（单通道，兼容历史调用方）
  * @param  value        待编码的 float 值
  * @param  frame_buffer 输出缓冲区，长度须 >= VOFA_JUSTFLOAT_FRAME_SIZE_BYTES
  * @retval 帧字节数（VOFA_JUSTFLOAT_FRAME_SIZE_BYTES）；frame_buffer 为 NULL 时返回 0
  */
uint32_t vofa_justfloat_encode_float(
    float value,
    uint8_t frame_buffer[VOFA_JUSTFLOAT_FRAME_SIZE_BYTES]);

/**
  * @brief  编码多个 float 为一条 JustFloat 多通道帧
  * @param  values        待编码的 float 数组（至少 channel_count 个元素）
  * @param  channel_count 通道数（1 ~ VOFA_JUSTFLOAT_MAX_CHANNELS）
  * @param  frame_buffer  输出缓冲区
  * @param  buffer_size   输出缓冲区字节数（须 >= 帧长，否则拒绝编码）
  * @retval 帧字节数；参数非法（NULL / 通道数为 0 / 超上限 / 缓冲区过小）时返回 0
  * @note   纯协议编码，不依赖 HAL/RTOS；float32 小端 + 帧尾 00 00 80 7F
  */
uint32_t vofa_justfloat_encode_multi(
    const float *values,
    uint32_t channel_count,
    uint8_t *frame_buffer,
    uint32_t buffer_size);

#endif /* VOFA_JUSTFLOAT_H */
