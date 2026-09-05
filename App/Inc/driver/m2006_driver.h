/**
  ******************************************************************************
  * @file    m2006_driver.h
  * @brief   M2006 电机（C610 电调）驱动层：目标电流写入 + 安全门 + 反馈解析，基于 FDCAN2
  *
  * 使用方式（Keil 调试）：
  * 1. 在 Keil Debug 的 Watch 窗口添加结构体实例 m2006_debug，即可查看
  *    并修改全部调试变量（无需逐个添加）；
  * 2. 将 m2006_debug.is_enabled 置 1 使能输出；
  * 3. 修改 m2006_debug.current_setpoint 控制输出电流（开环调试；闭环模式由控制层写入）。
  *    （-10000~+10000，对应 ±10A）。
  *
  * 安全保护（均在驱动内自动执行，参数可通过结构体成员调整）：
  * - 电流钳位：输出电流被限制在 ±m2006_debug.current_limit；
  * - 反馈超时：超过 M2006_DRIVER_RX_TIMEOUT_MS 未收到电调反馈时输出置 0；
  * - 超速保护：输出轴转速超过 ±m2006_debug.speed_limit_rpm 时输出置 0；
  * - 断使能：m2006_debug.is_enabled 为 0 时输出恒为 0。
  ******************************************************************************
  */
#ifndef M2006_DRIVER_H
#define M2006_DRIVER_H

#include <stdint.h>

/**
  * @brief  M2006 电机调试变量面板
  * @note   Keil Debug 的 Watch 窗口添加全局实例 m2006_debug 即可一次查看
  *         并修改全部成员；可写成员在运行时可实时修改。
  */
typedef struct m2006_debug
{
  /* ---- 可写成员（Keil Watch 可直接修改） ---- */

  /* 输出使能：0 断输出（电流恒为 0），1 使能控制 */
  uint8_t is_enabled;

  /* 目标电流（驱动输入），范围 -10000~+10000，对应 -10A~+10A；
     OPEN_LOOP 模式由 Watch 手动设定，闭环模式由 m2006_control 层写入 */
  int16_t current_setpoint;

  /* 电流钳位限幅，输出电流绝对值不超过该值（默认 3000 = 3A） */
  int16_t current_limit;

  /* 输出轴转速限幅 rpm，超速时输出置 0（默认 500 rpm） */
  int16_t speed_limit_rpm;

  /* ---- 电调回传原始值（直接来自 CAN 反馈帧，未解析换算） ---- */

  /* 转子机械角度原始编码值，范围 [0, 8191]，对应转子高速侧一圈 */
  uint16_t angle_raw;

  /* 转子转速原始值，单位 rpm（高速侧，除以 36 才是输出轴转速） */
  int16_t speed_rpm;

  /* 电调"实际输出转矩"原始编码值：实为电流环反馈电流，
     -10000~+10000 对应 -10A~+10A（1000 LSB = 1A） */
  int16_t torque_raw;

  /* ---- 换算后的物理量（驱动 1kHz 任务内换算） ---- */

  /* 转子单圈相位角（度，0~360° 随编码器回绕），= angle_raw/8191 × 360° */
  float angle_raw_deg;

  /* 输出轴累计角度（度，多圈连续不回绕），= 回绕展开后累计 LSB × 360/36/8191 */
  float angle_total_deg;

  /* 输出轴转速，单位 rpm，= speed_rpm / 36 */
  float speed_out_rpm;

  /* 输出轴力矩，单位 N·m，= 电流(A) × 0.18（M2006 官方转矩常数，输出轴等效值，
     换算依据见 m2006_driver.c 的 M2006_DRIVER_TORQUE_SCALE_NM 注释） */
  float torque_out_nm;

  /* ---- 其他观测成员 ---- */

  /* 驱动实际下发的电流值（钳位/安全门后的结果） */
  int16_t output_current;

  /* 已接收电调反馈帧计数 */
  uint32_t rx_msg_count;

  /* 反馈超时标志：1 表示最近 M2006_DRIVER_RX_TIMEOUT_MS 内未收到反馈 */
  uint8_t is_rx_timeout;

  /* CAN 发送失败计数 */
  uint32_t tx_fail_count;
} m2006_debug_t;

/* 调试变量面板全局实例：Keil Watch 添加 m2006_debug 即可查看/修改全部 */
extern volatile m2006_debug_t m2006_debug;

/**
  * @brief  初始化 M2006 驱动：配置 FDCAN2 过滤器并启动接收中断
  * @note   必须在 MX_FDCAN2_Init() 之后调用，建议在控制任务入口调用一次
  */
void m2006_driver_init(void);

/**
  * @brief  周期更新控制：换算/钳位/安全门/编码/发送，建议 1kHz 周期调用
  */
void m2006_driver_update(void);

/**
  * @brief  设置目标电流（驱动输入）
  * @note   开环调试直接修改 m2006_debug.current_setpoint 亦可；
  *         闭环模式由 m2006_control 层调用本接口写入。
  */
void m2006_driver_set_current_setpoint(int16_t current_raw);

#endif /* M2006_DRIVER_H */
