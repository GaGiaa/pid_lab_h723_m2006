/**
  ******************************************************************************
  * @file    pid.c
  * @brief   通用 PID 算法库实现（位置式 + 增量式）
  *
  * 位置式执行流程：
  *   设定值斜坡 → 误差计算 → 死区状态机 → P 项 →
  *   I 项（梯形积分+条件积分+积分限幅）→ D 项（微分先行+滤波后微分）→
  *   输出合成 → 输出限幅 → 更新历史值
  *
  * 增量式执行流程：
  *   设定值斜坡 → 误差计算 → 死区状态机 →
  *   Δp（P增量）→ Δi（梯形积分增量）→ Δd（微分先行增量）→
  *   delta_u 合成 → 累加到 output → 输出限幅 → 更新历史值
  ******************************************************************************
  */
#include "pid.h"

/* ========================================================================== */
/*                              位置式 PID 实现                                */
/* ========================================================================== */

pid_status_t pid_init(pid_t *pid)
{
  /* 检查 dt：必须 > 0 */
  if (pid->dt <= 0.0f)
  {
    pid->is_valid = 0U;
    return PID_ERR_INVALID_DT;
  }

  /* 检查输出限幅（非零值时要求 min <= max） */
  if (!((pid->out_min == 0.0f) && (pid->out_max == 0.0f)))
  {
    if (pid->out_min > pid->out_max)
    {
      pid->is_valid = 0U;
      return PID_ERR_INVALID_LIMIT;
    }
  }

  /* 检查积分限幅（非零值时要求 min <= max） */
  if (!((pid->integral_min == 0.0f) && (pid->integral_max == 0.0f)))
  {
    if (pid->integral_min > pid->integral_max)
    {
      pid->is_valid = 0U;
      return PID_ERR_INVALID_LIMIT;
    }
  }

  /* 配置合法，清除状态 */
  pid_reset(pid);
  pid->is_valid = 1U;
  return PID_OK;
}

void pid_reset(pid_t *pid)
{
  pid->setpoint_raw = 0.0f;
  pid->setpoint_eff = 0.0f;
  pid->measurement = 0.0f;
  pid->measurement_filtered = 0.0f;
  pid->error = 0.0f;
  pid->prev_error = 0.0f;
  pid->p_term = 0.0f;
  pid->i_term = 0.0f;
  pid->d_term = 0.0f;
  pid->integral = 0.0f;
  pid->prev_measurement_filtered = 0.0f;
  pid->output_unsat = 0.0f;
  pid->output = 0.0f;
  pid->in_deadband = 0U;
  pid->prev_saturated = 0U;
  /* is_valid 不变：init 通过后 reset 不会让它变 0 */
}

float pid_update(pid_t *pid, float setpoint, float measurement)
{
  float error_raw;
  uint8_t integral_allowed;

  /* 防御检查：dt 非法则安全返回 0，不崩溃 */
  if (pid->dt <= 0.0f)
  {
    pid->is_valid = 0U;
    return 0.0f;
  }

  /* 保存原始设定值和测量值 */
  pid->setpoint_raw = setpoint;
  pid->measurement = measurement;

  /* ---- 1. 设定值斜坡 ---- */
  if (pid->setpoint_rate > 0.0f)
  {
    float max_step = pid->setpoint_rate * pid->dt;
    float diff = setpoint - pid->setpoint_eff;
    if (diff > max_step)
    {
      diff = max_step;
    }
    else if (diff < -max_step)
    {
      diff = -max_step;
    }
    pid->setpoint_eff += diff;
  }
  else
  {
    pid->setpoint_eff = setpoint;
  }

  /* ---- 2. 误差计算 ---- */
  error_raw = pid->setpoint_eff - measurement;

  /* ---- 3. 死区状态机 ---- */
  if (pid->in_deadband)
  {
    /* 当前在死区内：检查是否需要退出 */
    if (pid->deadband > 0.0f)
    {
      float exit_threshold = pid->deadband + pid->hysteresis;
      if ((error_raw > exit_threshold) || (error_raw < -exit_threshold))
      {
        pid->in_deadband = 0U;
      }
    }
    else
    {
      pid->in_deadband = 0U;
    }
  }
  else
  {
    /* 当前不在死区内：检查是否需要进入 */
    if (pid->deadband > 0.0f)
    {
      if ((error_raw <= pid->deadband) && (error_raw >= -pid->deadband))
      {
        pid->in_deadband = 1U;
      }
    }
  }

  /* 死区内误差置零（P/I/D 全停，积分冻结） */
  if (pid->in_deadband)
  {
    pid->error = 0.0f;
  }
  else
  {
    pid->error = error_raw;
  }

  /* ---- 4. P 项 ---- */
  pid->p_term = pid->kp * pid->error;

  /* ---- 5. I 项（梯形积分 + 条件积分） ---- */
  integral_allowed = 1U;

  /* 条件积分 (a)：上一周期输出饱和则停止积分 */
  if (pid->prev_saturated)
  {
    integral_allowed = 0U;
  }

  /* 条件积分 (b)：大误差停止积分（阈值 > 0 时启用） */
  if (pid->integral_hold_error > 0.0f)
  {
    if ((pid->error > pid->integral_hold_error) ||
        (pid->error < -pid->integral_hold_error))
    {
      integral_allowed = 0U;
    }
  }

  if (integral_allowed)
  {
    /* 梯形积分：(e_k + e_{k-1}) / 2 × dt */
    pid->integral += (pid->error + pid->prev_error) * 0.5f * pid->dt;

    /* 积分限幅（非零值时生效） */
    if (!((pid->integral_min == 0.0f) && (pid->integral_max == 0.0f)))
    {
      if (pid->integral > pid->integral_max)
      {
        pid->integral = pid->integral_max;
      }
      else if (pid->integral < pid->integral_min)
      {
        pid->integral = pid->integral_min;
      }
    }
  }
  pid->i_term = pid->ki * pid->integral;

  /* ---- 6. D 项（微分先行 + 对测量值滤波后再微分） ---- */
  pid->measurement_filtered = pid->d_filter_alpha * measurement
                             + (1.0f - pid->d_filter_alpha) * pid->prev_measurement_filtered;

  /* 微分先行：D 项基于滤波后测量值的变化，而非误差变化 */
  pid->d_term = -pid->kd * (pid->measurement_filtered - pid->prev_measurement_filtered)
               / pid->dt;

  /* ---- 7. 输出合成 ---- */
  pid->output_unsat = pid->p_term + pid->i_term + pid->d_term;

  /* ---- 8. 输出限幅 ---- */
  pid->prev_saturated = 0U;
  if ((pid->out_min == 0.0f) && (pid->out_max == 0.0f))
  {
    /* 不限幅 */
    pid->output = pid->output_unsat;
  }
  else
  {
    pid->output = pid->output_unsat;
    if (pid->output > pid->out_max)
    {
      pid->output = pid->out_max;
      pid->prev_saturated = 1U;
    }
    else if (pid->output < pid->out_min)
    {
      pid->output = pid->out_min;
      pid->prev_saturated = 1U;
    }
  }

  /* ---- 9. 更新历史值 ---- */
  pid->prev_error = pid->error;
  pid->prev_measurement_filtered = pid->measurement_filtered;

  return pid->output;
}

/* ========================================================================== */
/*                              增量式 PID 实现                                */
/* ========================================================================== */

pid_status_t pid_inc_init(pid_inc_t *pid)
{
  /* 检查 dt */
  if (pid->dt <= 0.0f)
  {
    pid->is_valid = 0U;
    return PID_ERR_INVALID_DT;
  }

  /* 检查输出限幅 */
  if (!((pid->out_min == 0.0f) && (pid->out_max == 0.0f)))
  {
    if (pid->out_min > pid->out_max)
    {
      pid->is_valid = 0U;
      return PID_ERR_INVALID_LIMIT;
    }
  }

  pid_inc_reset(pid);
  pid->is_valid = 1U;
  return PID_OK;
}

void pid_inc_reset(pid_inc_t *pid)
{
  pid->setpoint_raw = 0.0f;
  pid->setpoint_eff = 0.0f;
  pid->measurement = 0.0f;
  pid->measurement_filtered = 0.0f;
  pid->error = 0.0f;
  pid->prev_error = 0.0f;
  pid->p_term = 0.0f;
  pid->i_term = 0.0f;
  pid->d_term = 0.0f;
  pid->delta_u = 0.0f;
  pid->prev_d_term = 0.0f;
  pid->prev_measurement_filtered = 0.0f;
  pid->output_unsat = 0.0f;
  pid->output = 0.0f;
  pid->in_deadband = 0U;
  /* is_valid 不变 */
}

float pid_inc_update(pid_inc_t *pid, float setpoint, float measurement)
{
  float error_raw;
  float d_current;

  /* 防御检查：dt 非法则安全返回 0 */
  if (pid->dt <= 0.0f)
  {
    pid->is_valid = 0U;
    return 0.0f;
  }

  /* 保存原始设定值和测量值 */
  pid->setpoint_raw = setpoint;
  pid->measurement = measurement;

  /* ---- 1. 设定值斜坡 ---- */
  if (pid->setpoint_rate > 0.0f)
  {
    float max_step = pid->setpoint_rate * pid->dt;
    float diff = setpoint - pid->setpoint_eff;
    if (diff > max_step)
    {
      diff = max_step;
    }
    else if (diff < -max_step)
    {
      diff = -max_step;
    }
    pid->setpoint_eff += diff;
  }
  else
  {
    pid->setpoint_eff = setpoint;
  }

  /* ---- 2. 误差计算 ---- */
  error_raw = pid->setpoint_eff - measurement;

  /* ---- 3. 死区状态机 ---- */
  if (pid->in_deadband)
  {
    if (pid->deadband > 0.0f)
    {
      float exit_threshold = pid->deadband + pid->hysteresis;
      if ((error_raw > exit_threshold) || (error_raw < -exit_threshold))
      {
        pid->in_deadband = 0U;
      }
    }
    else
    {
      pid->in_deadband = 0U;
    }
  }
  else
  {
    if (pid->deadband > 0.0f)
    {
      if ((error_raw <= pid->deadband) && (error_raw >= -pid->deadband))
      {
        pid->in_deadband = 1U;
      }
    }
  }

  if (pid->in_deadband)
  {
    pid->error = 0.0f;
  }
  else
  {
    pid->error = error_raw;
  }

  /* ---- 4. P 项增量 Δp = Kp × (e_k - e_{k-1}) ---- */
  pid->p_term = pid->kp * (pid->error - pid->prev_error);

  /* ---- 5. I 项增量（梯形积分）Δi = Ki × (e_k + e_{k-1}) / 2 × dt ---- */
  pid->i_term = pid->ki * (pid->error + pid->prev_error) * 0.5f * pid->dt;

  /* ---- 6. D 项增量（微分先行 + 滤波后微分） ---- */
  pid->measurement_filtered = pid->d_filter_alpha * measurement
                             + (1.0f - pid->d_filter_alpha) * pid->prev_measurement_filtered;

  /* D 项绝对值（微分先行） */
  d_current = -pid->kd * (pid->measurement_filtered - pid->prev_measurement_filtered)
             / pid->dt;

  /* D 项增量 Δd = d_current - d_{k-1} */
  pid->d_term = d_current - pid->prev_d_term;
  pid->prev_d_term = d_current;

  /* ---- 7. 总增量合成 ---- */
  pid->delta_u = pid->p_term + pid->i_term + pid->d_term;

  /* ---- 8. 累加到输出（增量式内部维护累加器） ---- */
  pid->output_unsat = pid->output + pid->delta_u;

  /* ---- 9. 输出限幅（作用于绝对输出） ---- */
  if ((pid->out_min == 0.0f) && (pid->out_max == 0.0f))
  {
    pid->output = pid->output_unsat;
  }
  else
  {
    pid->output = pid->output_unsat;
    if (pid->output > pid->out_max)
    {
      pid->output = pid->out_max;
    }
    else if (pid->output < pid->out_min)
    {
      pid->output = pid->out_min;
    }
  }

  /* ---- 10. 更新历史值 ---- */
  pid->prev_error = pid->error;
  pid->prev_measurement_filtered = pid->measurement_filtered;

  return pid->output;
}
