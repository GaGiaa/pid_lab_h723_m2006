/**
  ******************************************************************************
  * @file    pid.h
  * @brief   通用 PID 算法库（位置式 + 增量式），纯 C 实现，零平台依赖
  *
  * 特性：
  * - 位置式：设定值斜坡、梯形积分、条件积分（饱和停+大误差停）、
  *           积分限幅、微分先行、微分滤波、输出限幅、死区滞回
  * - 增量式：设定值斜坡、梯形积分、微分先行、微分滤波、
  *           输出限幅（作用于累加后的绝对输出）、死区滞回
  * - 微分先行与梯形积分始终开启，无开关
  * - 配置区直接修改结构体字段即可，运行时可调参
  *
  * 典型用法（位置式）：
  * @code
  * pid_t speed_pid;
  * speed_pid.kp = 1.0f;
  * speed_pid.ki = 0.1f;
  * speed_pid.kd = 0.01f;
  * speed_pid.dt = 0.001f;
  * speed_pid.out_min = -10000.0f;
  * speed_pid.out_max = 10000.0f;
  * // ... 其他配置
  * if (pid_init(&speed_pid) != PID_OK) {
  *     // 配置错误处理
  * }
  * // 每 1ms 调用：
  * float output = pid_update(&speed_pid, setpoint, measurement);
  * @endcode
  ******************************************************************************
  */
#ifndef PID_H
#define PID_H

#include <stdint.h>

/**
  * @brief  PID 初始化错误码
  */
typedef enum
{
  PID_OK = 0,              /* 初始化成功 */
  PID_ERR_INVALID_DT,      /* dt <= 0 */
  PID_ERR_INVALID_LIMIT,   /* 限幅配置非法（min > max，非零值时） */
} pid_status_t;

/**
  * @brief  位置式 PID 实例
  * @note   配置区运行时可直接修改；状态区由 pid_update 内部更新，外部只读。
  */
typedef struct
{
  /* === 配置区（运行时可修改） === */

  float kp;                 /* 比例增益 */
  float ki;                 /* 积分增益 */
  float kd;                 /* 微分增益 */
  float dt;                 /* 固定控制周期（秒），必须 > 0 */

  float out_min;            /* 输出下限，out_min 与 out_max 都为 0 表示不限幅 */
  float out_max;            /* 输出上限 */

  float integral_min;       /* 积分下限，integral_min 与 integral_max 都为 0 表示不限幅 */
  float integral_max;       /* 积分上限 */

  float setpoint_rate;      /* 设定值最大变化率（单位/秒），0 表示禁用斜坡 */

  float deadband;           /* 死区宽度，|error| <= deadband 时误差置零，0 表示禁用死区 */
  float hysteresis;         /* 滞回宽度，退出死区需 |error| > deadband + hysteresis */

  float d_filter_alpha;     /* 微分滤波系数（0~1），1 表示不滤波 */

  float integral_hold_error; /* 大误差停积分阈值，|error| > 该值时停止积分，0 表示禁用该策略 */

  /* === 状态区（只读，由 pid_update 更新） === */

  float setpoint_raw;       /* 原始设定值（本次 update 传入） */
  float setpoint_eff;       /* 有效设定值（经斜坡处理后） */

  float measurement;        /* 原始测量值（本次 update 传入） */
  float measurement_filtered; /* 滤波后测量值（仅用于 D 项计算） */

  float error;              /* 当前误差（经死区处理后，死区内为 0） */
  float prev_error;         /* 上一周期误差 */

  float p_term;             /* P 项输出 */
  float i_term;             /* I 项输出 */
  float d_term;             /* D 项输出 */

  float integral;           /* 积分累加值 */

  float prev_measurement_filtered; /* 上一周期滤波后测量值（内部用） */

  float output_unsat;       /* 限幅前输出 */
  float output;             /* 限幅后输出（pid_update 返回值） */

  uint8_t in_deadband;      /* 当前是否在死区内 */
  uint8_t prev_saturated;   /* 上一周期输出是否饱和（内部用，条件积分依据） */
  uint8_t is_valid;         /* init 是否通过（内部用，失败时 update 返回 0） */
} pid_t;

/**
  * @brief  增量式 PID 实例
  * @note   输出为累加后的绝对输出（内部维护累加器）；
  *         P/I/D 三项诊断变量为本次增量 Δp/Δi/Δd。
  */
typedef struct
{
  /* === 配置区（运行时可修改） === */

  float kp;                 /* 比例增益 */
  float ki;                 /* 积分增益 */
  float kd;                 /* 微分增益 */
  float dt;                 /* 固定控制周期（秒），必须 > 0 */

  float out_min;            /* 输出下限，out_min 与 out_max 都为 0 表示不限幅 */
  float out_max;            /* 输出上限 */

  float setpoint_rate;      /* 设定值最大变化率（单位/秒），0 表示禁用斜坡 */

  float deadband;           /* 死区宽度，0 表示禁用死区 */
  float hysteresis;         /* 滞回宽度 */

  float d_filter_alpha;     /* 微分滤波系数（0~1），1 表示不滤波 */

  /* === 状态区（只读，由 pid_inc_update 更新） === */

  float setpoint_raw;       /* 原始设定值 */
  float setpoint_eff;       /* 有效设定值（经斜坡处理后） */

  float measurement;        /* 原始测量值 */
  float measurement_filtered; /* 滤波后测量值（仅用于 D 项） */

  float error;              /* 当前误差（经死区处理后） */
  float prev_error;         /* 上一周期误差 */

  float p_term;             /* 本次 P 项增量 Δp */
  float i_term;             /* 本次 I 项增量 Δi */
  float d_term;             /* 本次 D 项增量 Δd */
  float delta_u;            /* 本次总增量 Δu = Δp + Δi + Δd */

  float prev_d_term;        /* 上一周期 D 项绝对值（内部用，算 Δd） */
  float prev_measurement_filtered; /* 上一周期滤波后测量值（内部用） */

  float output_unsat;       /* 限幅前输出（累加后） */
  float output;             /* 限幅后输出（累加器，pid_inc_update 返回值） */

  uint8_t in_deadband;      /* 当前是否在死区内 */
  uint8_t is_valid;         /* init 是否通过（内部用） */
} pid_inc_t;

/* === 位置式函数 === */

/**
  * @brief  初始化位置式 PID：检查配置合法性并清除所有状态
  * @param  pid 指向已设置好配置字段的 pid_t 实例
  * @retval PID_OK 成功；PID_ERR_INVALID_DT dt<=0；PID_ERR_INVALID_LIMIT 限幅非法
  */
pid_status_t pid_init(pid_t *pid);

/**
  * @brief  复位位置式 PID：清除所有状态，保留配置
  * @param  pid 指向 pid_t 实例
  */
void pid_reset(pid_t *pid);

/**
  * @brief  执行一个周期的位置式 PID 计算
  * @param  pid 指向 pid_t 实例
  * @param  setpoint 本周期设定值
  * @param  measurement 本周期测量值
  * @retval 限幅后的输出值；is_valid=0 时返回 0
  * @note   必须按 pid.dt 固定周期调用
  */
float pid_update(pid_t *pid, float setpoint, float measurement);

/* === 增量式函数 === */

/**
  * @brief  初始化增量式 PID：检查配置合法性并清除所有状态
  * @param  pid 指向已设置好配置字段的 pid_inc_t 实例
  * @retval PID_OK 成功；PID_ERR_INVALID_DT dt<=0；PID_ERR_INVALID_LIMIT 限幅非法
  */
pid_status_t pid_inc_init(pid_inc_t *pid);

/**
  * @brief  复位增量式 PID：清除所有状态（含累加器），保留配置
  * @param  pid 指向 pid_inc_t 实例
  */
void pid_inc_reset(pid_inc_t *pid);

/**
  * @brief  执行一个周期的增量式 PID 计算
  * @param  pid 指向 pid_inc_t 实例
  * @param  setpoint 本周期设定值
  * @param  measurement 本周期测量值
  * @retval 累加并限幅后的绝对输出值；is_valid=0 时返回 0
  * @note   必须按 pid.dt 固定周期调用
  */
float pid_inc_update(pid_inc_t *pid, float setpoint, float measurement);

#endif /* PID_H */
