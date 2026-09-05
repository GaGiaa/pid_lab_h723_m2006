/**
  ******************************************************************************
  * @file    m2006_control.h
  * @brief   M2006 电机闭环控制层：位置环（位置式 P）→ 速度环（增量式 PI）级联
  * @note    依赖 Lib/pid_lib（pid_t / pid_inc_t）与 m2006_driver（反馈与电流接口）。
  *          控制模式可通过 Keil Watch 修改 m2006_control_debug.mode 实时切换，
  *          切换时内部自动复位两个 PID，并把速度环累加器预置为当前输出电流。
  ******************************************************************************
  */
#ifndef M2006_CONTROL_H
#define M2006_CONTROL_H

#include <stdint.h>

#include "pid.h"

/* 控制模式 */
typedef enum
{
  M2006_CTRL_MODE_OPEN_LOOP = 0,  /* 电流开环：目标电流取 m2006_debug.current_setpoint */
  M2006_CTRL_MODE_SPEED,          /* 速度闭环：速度设定 → 速度环（增量式 PI）→ 电流 */
  M2006_CTRL_MODE_POSITION,       /* 位置闭环：位置环（位置式 P）→ 速度环 → 电流 */
} m2006_ctrl_mode_t;

/**
  * @brief  M2006 闭环控制调试变量面板
  * @note   Keil Debug 的 Watch 窗口添加全局实例 m2006_control_debug 即可一次查看
  *         并修改全部成员；可写成员在运行时可实时修改。
  */
typedef struct m2006_control_debug
{
  /* ---- 可写成员（Keil Watch 可直接修改） ---- */

  /* 控制模式：OPEN_LOOP / SPEED / POSITION，切换时内部复位 PID 并预置累加器 */
  m2006_ctrl_mode_t mode;

  /* 位置设定：输出轴累计角度（度），支持多圈连续角度（如 540.0） */
  float pos_setpoint_deg;

  /* 速度设定：输出轴转速（rpm），SPEED 模式使用 */
  float speed_setpoint_rpm;

  /* 位置环比例增益，输出单位 rpm/°（默认 5.0） */
  float pos_pid_kp;

  /* 位置环死区宽度（度，默认 0.5）：|误差| 进入死区后误差置零，到位判停 */
  float pos_deadband_deg;

  /* 位置环输出限幅 = 位置模式最大速度（rpm，默认 300） */
  float pos_max_speed_rpm;

  /* 速度环比例增益（默认 30.0） */
  float spd_pid_kp;

  /* 速度环积分增益（默认 5.0） */
  float spd_pid_ki;

  /* 速度设定最大变化率（rpm/s，默认 300，0 表示禁用斜坡） */
  float spd_setpoint_rate;

  /* ---- 只读成员 ---- */

  /* 实际位置：输出轴累计角度（度），取 m2006_debug.angle_total_deg（driver 维护多圈累计） */
  float pos_feedback_deg;

  /* 实际速度：输出轴转速（rpm）= 转子 rpm / 36 */
  float speed_feedback_rpm;

  /* 位置环输出（速度环设定值），POSITION 模式有效 */
  float speed_cmd_rpm;

  /* 速度环输出（电流指令 raw），镜像到 m2006_debug.current_setpoint */
  int16_t current_cmd_raw;

  /* 位置误差是否在死区内（POSITION 模式有效） */
  uint8_t pos_in_deadband;
} m2006_control_debug_t;

/* 调试变量面板全局实例：Keil Watch 添加 m2006_control_debug 即可查看/修改全部 */
extern volatile m2006_control_debug_t m2006_control_debug;

/**
  * @brief  闭环控制计算（纯函数，主机端可测）：模式分支 + 级联 PID
  * @param  cfg                调试配置（读模式/设定/增益，写回只读观测字段）
  * @param  pos_feedback_deg   实际位置（输出轴累计角度，度）
  * @param  speed_feedback_rpm 实际速度（输出轴 rpm）
  * @param  pos_pid            位置环实例（位置式，POSITION 模式使用）
  * @param  spd_pid            速度环实例（增量式，SPEED/POSITION 模式使用）
  * @param  current_limit      电流钳位（取 m2006_debug.current_limit，单一来源）
  * @param  open_loop_current  开环目标电流（OPEN_LOOP 模式直通值）
  * @param  is_enabled         输出使能（0 时强制输出 0，driver 安全门兜底）
  * @retval 目标电流 raw（-current_limit ~ +current_limit）
  */
int16_t m2006_control_compute_current(
    volatile m2006_control_debug_t *cfg,
    float pos_feedback_deg,
    float speed_feedback_rpm,
    pid_t *pos_pid,
    pid_inc_t *spd_pid,
    int16_t current_limit,
    int16_t open_loop_current,
    uint8_t is_enabled);

/**
  * @brief  初始化闭环控制：配置两个 PID 实例并复位状态
  * @note   必须在 m2006_driver_init() 之后调用，建议在控制任务入口调用一次
  */
void m2006_control_init(void);

/**
  * @brief  周期更新闭环控制：读反馈（driver 面板）→ 模式分支 → 级联 PID → 写目标电流
  * @note   建议 1kHz 周期调用，与 m2006_driver_update() 同任务顺序执行
  */
void m2006_control_update(void);

#endif /* M2006_CONTROL_H */
