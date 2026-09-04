/**
  ******************************************************************************
  * @file    m2006_protocol.h
  * @brief   M2006 电机配合 C610 电调的 CAN 协议编解码（纯 C，不依赖 HAL 或 RTOS）
  *
  * 协议依据《RoboMaster C610 无刷电机调速器使用说明》CAN 通信协议章节：
  * - 控制帧：标准帧 0x200（ID 1~4），每 ID 占 2 字节，高字节在前，
  *   电流值 -10000~+10000 对应 -10A~+10A（即 1000 LSB/A）。
  * - 反馈帧：标准帧 0x200+电调ID，DATA[0..1] 转子机械角度 0~8191，
  *   DATA[2..3] 转子转速 rpm（int16），DATA[4..5] 实际输出转矩（int16）。
  * 本模块为纯函数，便于主机端独立测试。
  ******************************************************************************
  */
#ifndef M2006_PROTOCOL_H
#define M2006_PROTOCOL_H

#include <stdint.h>

/* 协议常量 */
#define M2006_PROTOCOL_FRAME_BYTES (8U)          /* CAN 数据帧字节数 */
#define M2006_PROTOCOL_CONTROL_ID (0x200U)       /* 控制帧标识符（ID 1~4） */
#define M2006_PROTOCOL_MOTOR_ID_MIN (1U)         /* 控制帧支持的最小电调 ID */
#define M2006_PROTOCOL_MOTOR_ID_MAX (4U)         /* 控制帧支持的最大电调 ID */
#define M2006_PROTOCOL_GEAR_RATIO (36U)          /* M2006 减速比 36:1 */
#define M2006_PROTOCOL_ANGLE_MAX (8191U)         /* 转子机械角度满量程 */
#define M2006_PROTOCOL_CURRENT_FULL_SCALE (10000) /* 电流满量程，对应 10A */

/* 电调反馈测量值 */
typedef struct m2006_measure
{
  uint16_t angle_raw;   /* 转子机械角度，范围 [0, 8191] */
  int16_t speed_rpm;    /* 转子转速，单位 rpm（高速侧，除以减速比得输出轴转速） */
  int16_t torque_raw;   /* 实际输出转矩原始值 */
} m2006_measure_t;

/**
  * @brief  解析电调反馈帧到测量结构体
  * @param  feedback_data  反馈帧数据指针，长度 M2006_PROTOCOL_FRAME_BYTES
  * @param  measure        输出测量结构体指针
  * @retval 1 成功；0 参数为空
  */
uint32_t m2006_protocol_parse_feedback(
    const uint8_t feedback_data[M2006_PROTOCOL_FRAME_BYTES],
    m2006_measure_t *measure);

/**
  * @brief  编码控制帧：把指定电调 ID 的电流写入对应字节偏移
  * @param  motor_id      电调 ID，范围 1~4
  * @param  current_raw   控制电流值，范围 -10000~+10000
  * @param  control_data  输出控制帧数据指针，长度 M2006_PROTOCOL_FRAME_BYTES
  * @retval M2006_PROTOCOL_FRAME_BYTES 成功；0 参数为空或电调 ID 越界
  */
uint32_t m2006_protocol_encode_control(
    uint8_t motor_id,
    int16_t current_raw,
    uint8_t control_data[M2006_PROTOCOL_FRAME_BYTES]);

#endif /* M2006_PROTOCOL_H */
