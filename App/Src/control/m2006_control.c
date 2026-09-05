/**
  ******************************************************************************
  * @file    m2006_control.c
  * @brief   M2006 电机闭环控制层实现：位置环（位置式 P）→ 速度环（增量式 PI）级联
  * @note    控制流程（由 1kHz 任务周期调用 m2006_control_update()）：
  *             读反馈（driver 面板）→ 模式分支 → 级联 PID → m2006_driver 写目标电流
  *
  *         模式分工：
  *           - OPEN_LOOP：目标电流 = m2006_debug.current_setpoint（Watch 手动设定）
  *           - SPEED：速度设定 → 增量式速度环 → 电流
  *           - POSITION：位置设定 → 位置式位置环 → 速度设定 → 增量式速度环 → 电流
  *
  *         电流限幅单一来源：速度环输出限幅与最终钳位均取
  *         m2006_debug.current_limit（驱动层钳位值），不在本层复制。
  *         输出使能为 0 时强制输出 0；driver 内另有超时/超速安全门兜底。
  ******************************************************************************
  */
#include "m2006_control.h"

#include "m2006_driver.h"

/* 控制周期（秒），与 1kHz 控制任务一致 */
#define M2006_CONTROL_DT_SEC (0.001f)

/* ---- 调试变量面板定义（Keil Watch 添加 m2006_control_debug 即可查看/修改） ---- */
volatile m2006_control_debug_t m2006_control_debug = {
  .mode = M2006_CTRL_MODE_OPEN_LOOP,
  .pos_setpoint_deg = 0.0f,
  .speed_setpoint_rpm = 0.0f,
  .pos_pid_kp = 1.0f,
  .pos_deadband_deg = 0.5f,
  .pos_max_speed_rpm = 0.0f,
  .spd_pid_kp = 30.0f,
  .spd_pid_ki = 5.0f,
  .spd_setpoint_rate = 0.0f,
  .pos_feedback_deg = 0.0f,
  .speed_feedback_rpm = 0.0f,
  .speed_cmd_rpm = 0.0f,
  .current_cmd_raw = 0,
  .pos_in_deadband = 0U,
};

/* 两个 PID 实例（位置式 + 增量式，配置区运行时由 compute 同步调试面板） */
static pid_t m2006_control_pos_pid;
static pid_inc_t m2006_control_spd_pid;

/* 上一周期模式：用于切换检测（切换时复位 PID 并预置累加器） */
static m2006_ctrl_mode_t m2006_control_prev_mode = M2006_CTRL_MODE_OPEN_LOOP;

int16_t m2006_control_compute_current(
    volatile m2006_control_debug_t *cfg,
    float pos_feedback_deg,
    float speed_feedback_rpm,
    pid_t *pos_pid,
    pid_inc_t *spd_pid,
    int16_t current_limit,
    int16_t open_loop_current,
    uint8_t is_enabled)
{
  float current_cmd;
  float current_limit_f = (float)current_limit;

  /* 运行时参数同步：Keil Watch 修改配置区字段后立即生效 */
  pos_pid->kp = cfg->pos_pid_kp;
  pos_pid->deadband = cfg->pos_deadband_deg;
  pos_pid->out_min = -cfg->pos_max_speed_rpm;
  pos_pid->out_max = cfg->pos_max_speed_rpm;

  spd_pid->kp = cfg->spd_pid_kp;
  spd_pid->ki = cfg->spd_pid_ki;
  spd_pid->setpoint_rate = cfg->spd_setpoint_rate;
  spd_pid->out_min = -current_limit_f;
  spd_pid->out_max = current_limit_f;

  switch (cfg->mode)
  {
    case M2006_CTRL_MODE_OPEN_LOOP:
      /* 电流开环：目标电流直通（Watch 设定值），driver 仍会钳位 */
      cfg->speed_cmd_rpm = 0.0f;
      cfg->pos_in_deadband = 0U;
      current_cmd = (float)open_loop_current;
      break;

    case M2006_CTRL_MODE_SPEED:
      /* 速度闭环：位置环旁路，速度设定直通速度环 */
      cfg->speed_cmd_rpm = cfg->speed_setpoint_rpm;
      cfg->pos_in_deadband = 0U;
      current_cmd = pid_inc_update(spd_pid,
                                   cfg->speed_setpoint_rpm,
                                   speed_feedback_rpm);
      break;

    case M2006_CTRL_MODE_POSITION:
      /* 位置闭环：位置环输出作为速度环设定，级联计算 */
      cfg->speed_cmd_rpm = pid_update(pos_pid,
                                      cfg->pos_setpoint_deg,
                                      pos_feedback_deg);
      cfg->pos_in_deadband = (uint8_t)pos_pid->in_deadband;
      current_cmd = pid_inc_update(spd_pid,
                                   cfg->speed_cmd_rpm,
                                   speed_feedback_rpm);
      break;

    default:
      cfg->speed_cmd_rpm = 0.0f;
      cfg->pos_in_deadband = 0U;
      current_cmd = 0.0f;
      break;
  }

  /* 断使能强制断输出（driver 内另有超时/超速安全门） */
  if (!is_enabled)
  {
    current_cmd = 0.0f;
  }

  /* 最终电流钳位（双保险，与驱动层钳位一致） */
  if (current_cmd > current_limit_f)
  {
    current_cmd = current_limit_f;
  }
  else if (current_cmd < -current_limit_f)
  {
    current_cmd = -current_limit_f;
  }

  cfg->current_cmd_raw = (int16_t)current_cmd;
  return (int16_t)current_cmd;
}

void m2006_control_init(void)
{
  /* 位置环：位置式，纯 P + 死区，输出限幅 = 位置模式最大速度 */
  m2006_control_pos_pid.kp = m2006_control_debug.pos_pid_kp;
  m2006_control_pos_pid.ki = 0.0f;
  m2006_control_pos_pid.kd = 0.0f;
  m2006_control_pos_pid.dt = M2006_CONTROL_DT_SEC;
  m2006_control_pos_pid.out_min = -m2006_control_debug.pos_max_speed_rpm;
  m2006_control_pos_pid.out_max = m2006_control_debug.pos_max_speed_rpm;
  m2006_control_pos_pid.integral_min = 0.0f;
  m2006_control_pos_pid.integral_max = 0.0f;
  m2006_control_pos_pid.setpoint_rate = 0.0f;
  m2006_control_pos_pid.deadband = m2006_control_debug.pos_deadband_deg;
  m2006_control_pos_pid.hysteresis = 0.0f;
  m2006_control_pos_pid.d_filter_alpha = 1.0f;
  m2006_control_pos_pid.integral_hold_error = 0.0f;
  (void)pid_init(&m2006_control_pos_pid);

  /* 速度环：增量式，PI，输出限幅 = 电流钳位（单一来源 m2006_debug.current_limit） */
  m2006_control_spd_pid.kp = m2006_control_debug.spd_pid_kp;
  m2006_control_spd_pid.ki = m2006_control_debug.spd_pid_ki;
  m2006_control_spd_pid.kd = 0.0f;
  m2006_control_spd_pid.dt = M2006_CONTROL_DT_SEC;
  m2006_control_spd_pid.out_min = -(float)m2006_debug.current_limit;
  m2006_control_spd_pid.out_max = (float)m2006_debug.current_limit;
  m2006_control_spd_pid.setpoint_rate = m2006_control_debug.spd_setpoint_rate;
  m2006_control_spd_pid.deadband = 0.0f;
  m2006_control_spd_pid.hysteresis = 0.0f;
  m2006_control_spd_pid.d_filter_alpha = 1.0f;
  (void)pid_inc_init(&m2006_control_spd_pid);

  m2006_control_prev_mode = m2006_control_debug.mode;
}

void m2006_control_update(void)
{
  /* 位置反馈取 driver 维护的多圈累计角（angle_total_deg），不在本层累计 */
  m2006_control_debug.pos_feedback_deg = m2006_debug.angle_total_deg;
  /* 速度反馈复用 driver 已换算的输出轴转速（speed_out_rpm = speed_rpm ÷ 36），
     不在本层重复计算 */
  m2006_control_debug.speed_feedback_rpm = m2006_debug.speed_out_rpm;

  /* 模式切换：复位两个 PID，并把速度环累加器预置为当前输出电流，
     使切换瞬间电流不突变（如从开环 3000 切闭环，累加器从 3000 起步） */
  if (m2006_control_debug.mode != m2006_control_prev_mode)
  {
    pid_reset(&m2006_control_pos_pid);
    pid_inc_reset(&m2006_control_spd_pid);
    m2006_control_spd_pid.output = (float)m2006_debug.output_current;
    m2006_control_prev_mode = m2006_control_debug.mode;
  }

  m2006_driver_set_current_setpoint(m2006_control_compute_current(
      &m2006_control_debug,
      m2006_control_debug.pos_feedback_deg,
      m2006_control_debug.speed_feedback_rpm,
      &m2006_control_pos_pid,
      &m2006_control_spd_pid,
      m2006_debug.current_limit,
      m2006_debug.current_setpoint,
      m2006_debug.is_enabled));
}
