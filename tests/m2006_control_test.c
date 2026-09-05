/**
  ******************************************************************************
  * @file    m2006_control_test.c
  * @brief   M2006 闭环控制层主机端单元测试（gcc 编译运行，不依赖嵌入式平台）
  *
  * 编译：gcc -Wall -Wextra -I App/Inc/control -I App/Inc/driver
  *       -I Lib/pid_lib/include -o tests/m2006_control_test.exe
  *       tests/m2006_control_test.c App/Src/control/m2006_control.c
  *       Lib/pid_lib/src/pid.c
  * 运行：./tests/m2006_control_test.exe
  *
  * 覆盖：开环直通/钳位/断使能、速度环输出方向与限幅、（角度回绕用例已迁至
  *       m2006_protocol_test.c）
  *       位置环死区、位置环输出限幅、级联电流限幅、非法模式。
  ******************************************************************************
  */
#include "m2006_control.h"
#include "m2006_driver.h"
#include "pid.h"

#include <math.h>
#include <stdio.h>

/* ---- 测试桩：避免链接 m2006_driver 实现 ---- */
volatile m2006_debug_t m2006_debug;

void m2006_driver_set_current_setpoint(int16_t current_raw)
{
  (void)current_raw;
}

/* ---- 简易测试框架 ---- */
static int g_test_count = 0;
static int g_test_fail = 0;

#define M2006_CONTROL_TEST_ASSERT(cond, msg)                                  \
  do {                                                          \
    g_test_count++;                                            \
    if (!(cond)) {                                             \
      fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__); \
      g_test_fail++;                                           \
    }                                                           \
  } while (0)

#define M2006_CONTROL_TEST_ASSERT_FLOAT_NEAR(actual, expected, eps, msg)       \
  do {                                                          \
    g_test_count++;                                            \
    if (fabsf((float)(actual) - (float)(expected)) > (float)(eps)) { \
      fprintf(stderr, "FAIL: %s (line %d) actual=%f expected=%f\n", \
              msg, __LINE__, (double)(actual), (double)(expected)); \
      g_test_fail++;                                           \
    }                                                           \
  } while (0)

/* ---- 测试配置辅助 ---- */

static void pos_pid_config(pid_t *p)
{
  p->kp = 5.0f;
  p->ki = 0.0f;
  p->kd = 0.0f;
  p->dt = 0.001f;
  p->out_min = -300.0f;
  p->out_max = 300.0f;
  p->integral_min = 0.0f;
  p->integral_max = 0.0f;
  p->setpoint_rate = 0.0f;
  p->deadband = 0.5f;
  p->hysteresis = 0.0f;
  p->d_filter_alpha = 1.0f;
  p->integral_hold_error = 0.0f;
  (void)pid_init(p);
}

static void spd_pid_config(pid_inc_t *p)
{
  p->kp = 30.0f;
  p->ki = 5.0f;
  p->kd = 0.0f;
  p->dt = 0.001f;
  p->out_min = -3000.0f;
  p->out_max = 3000.0f;
  p->setpoint_rate = 0.0f;   /* 测试禁用斜坡，便于精确断言 */
  p->deadband = 0.0f;
  p->hysteresis = 0.0f;
  p->d_filter_alpha = 1.0f;
  (void)pid_inc_init(p);
}

static void cfg_reset(volatile m2006_control_debug_t *cfg)
{
  cfg->mode = M2006_CTRL_MODE_OPEN_LOOP;
  cfg->pos_setpoint_deg = 0.0f;
  cfg->speed_setpoint_rpm = 100.0f;
  cfg->pos_pid_kp = 5.0f;
  cfg->pos_deadband_deg = 0.5f;
  cfg->pos_max_speed_rpm = 300.0f;
  cfg->spd_pid_kp = 30.0f;
  cfg->spd_pid_ki = 5.0f;
  cfg->spd_setpoint_rate = 0.0f;   /* 测试禁用斜坡，便于精确断言 */
  cfg->pos_feedback_deg = 0.0f;
  cfg->speed_feedback_rpm = 0.0f;
  cfg->speed_cmd_rpm = 0.0f;
  cfg->current_cmd_raw = 0;
  cfg->pos_in_deadband = 0U;
}

/* ---- 开环模式 ---- */

static int test_open_loop_direct(void)
{
  volatile m2006_control_debug_t cfg;
  pid_t pos_pid;
  pid_inc_t spd_pid;
  int16_t result;

  cfg_reset(&cfg);
  cfg.mode = M2006_CTRL_MODE_OPEN_LOOP;
  pos_pid_config(&pos_pid);
  spd_pid_config(&spd_pid);

  result = m2006_control_compute_current(&cfg, 0.0f, 0.0f,
                                         &pos_pid, &spd_pid,
                                         3000, 1000, 1U);
  return (result == 1000) && (cfg.current_cmd_raw == 1000);
}

static int test_open_loop_clamped(void)
{
  volatile m2006_control_debug_t cfg;
  pid_t pos_pid;
  pid_inc_t spd_pid;

  cfg_reset(&cfg);
  cfg.mode = M2006_CTRL_MODE_OPEN_LOOP;
  pos_pid_config(&pos_pid);
  spd_pid_config(&spd_pid);

  return m2006_control_compute_current(&cfg, 0.0f, 0.0f,
                                       &pos_pid, &spd_pid,
                                       3000, 5000, 1U) == 3000;
}

static int test_open_loop_disabled(void)
{
  volatile m2006_control_debug_t cfg;
  pid_t pos_pid;
  pid_inc_t spd_pid;

  cfg_reset(&cfg);
  cfg.mode = M2006_CTRL_MODE_OPEN_LOOP;
  pos_pid_config(&pos_pid);
  spd_pid_config(&spd_pid);

  return m2006_control_compute_current(&cfg, 0.0f, 0.0f,
                                       &pos_pid, &spd_pid,
                                       3000, 1000, 0U) == 0;
}

/* ---- 速度闭环 ---- */

static int test_speed_loop_zero_steady(void)
{
  volatile m2006_control_debug_t cfg;
  pid_t pos_pid;
  pid_inc_t spd_pid;
  int16_t result;

  cfg_reset(&cfg);
  cfg.mode = M2006_CTRL_MODE_SPEED;
  cfg.speed_setpoint_rpm = 0.0f;
  pos_pid_config(&pos_pid);
  spd_pid_config(&spd_pid);

  result = m2006_control_compute_current(&cfg, 0.0f, 0.0f,
                                         &pos_pid, &spd_pid,
                                         3000, 0, 1U);
  return (result == 0) && (cfg.speed_cmd_rpm == 0.0f);
}

static int test_speed_loop_positive_drive_clamped(void)
{
  volatile m2006_control_debug_t cfg;
  pid_t pos_pid;
  pid_inc_t spd_pid;
  int16_t result;

  cfg_reset(&cfg);
  cfg.mode = M2006_CTRL_MODE_SPEED;
  cfg.speed_setpoint_rpm = 100.0f;
  pos_pid_config(&pos_pid);
  spd_pid_config(&spd_pid);

  /* 设定 100 rpm、反馈 0：误差 100 → 增量输出冲向限幅 */
  result = m2006_control_compute_current(&cfg, 0.0f, 0.0f,
                                         &pos_pid, &spd_pid,
                                         3000, 0, 1U);
  return (result == 3000) && (result > 0);
}

static int test_speed_loop_reaches_setpoint(void)
{
  volatile m2006_control_debug_t cfg;
  pid_t pos_pid;
  pid_inc_t spd_pid;
  int16_t result;

  cfg_reset(&cfg);
  cfg.mode = M2006_CTRL_MODE_SPEED;
  cfg.speed_setpoint_rpm = 100.0f;
  pos_pid_config(&pos_pid);
  spd_pid_config(&spd_pid);

  /* 第一次：误差 100 → 输出冲向限幅 */
  (void)m2006_control_compute_current(&cfg, 0.0f, 0.0f,
                                      &pos_pid, &spd_pid,
                                      3000, 0, 1U);
  /* 第二次：反馈等于设定 → 误差 0，增量反向回落，输出不再增加 */
  result = m2006_control_compute_current(&cfg, 0.0f, 100.0f,
                                         &pos_pid, &spd_pid,
                                         3000, 0, 1U);
  return result < 3000;
}

/* ---- 位置闭环 ---- */

static int test_position_deadband_zero_output(void)
{
  volatile m2006_control_debug_t cfg;
  pid_t pos_pid;
  pid_inc_t spd_pid;
  int16_t result;

  cfg_reset(&cfg);
  cfg.mode = M2006_CTRL_MODE_POSITION;
  cfg.pos_setpoint_deg = 10.4f;
  pos_pid_config(&pos_pid);
  spd_pid_config(&spd_pid);

  /* 误差 0.4° ≤ 死区 0.5° → 位置环输出 0 → 速度环无增量 → 电流 0 */
  result = m2006_control_compute_current(&cfg, 10.0f, 0.0f,
                                         &pos_pid, &spd_pid,
                                         3000, 0, 1U);
  return (result == 0)
      && (cfg.pos_in_deadband == 1U)
      && (cfg.speed_cmd_rpm == 0.0f);
}

static int test_position_error_maps_to_speed_cmd(void)
{
  volatile m2006_control_debug_t cfg;
  pid_t pos_pid;
  pid_inc_t spd_pid;
  int16_t result;

  cfg_reset(&cfg);
  cfg.mode = M2006_CTRL_MODE_POSITION;
  cfg.pos_setpoint_deg = 10.0f;
  pos_pid_config(&pos_pid);
  spd_pid_config(&spd_pid);

  /* 误差 10° × kp 5 = 50 rpm，未超限幅 */
  result = m2006_control_compute_current(&cfg, 0.0f, 0.0f,
                                         &pos_pid, &spd_pid,
                                         3000, 0, 1U);
  M2006_CONTROL_TEST_ASSERT_FLOAT_NEAR(cfg.speed_cmd_rpm, 50.0f, 0.01f,
                                 "position error -> speed cmd");
  return result != 0;
}

static int test_position_clamps_speed_cmd(void)
{
  volatile m2006_control_debug_t cfg;
  pid_t pos_pid;
  pid_inc_t spd_pid;
  int16_t result;

  cfg_reset(&cfg);
  cfg.mode = M2006_CTRL_MODE_POSITION;
  cfg.pos_setpoint_deg = 100.0f;
  pos_pid_config(&pos_pid);
  spd_pid_config(&spd_pid);

  /* 误差 100° × kp 5 = 500 rpm → 限幅到 pos_max_speed_rpm = 300 */
  result = m2006_control_compute_current(&cfg, 0.0f, 0.0f,
                                         &pos_pid, &spd_pid,
                                         3000, 0, 1U);
  M2006_CONTROL_TEST_ASSERT_FLOAT_NEAR(cfg.speed_cmd_rpm, 300.0f, 0.01f,
                                 "position speed cmd clamped");
  M2006_CONTROL_TEST_ASSERT(result == 3000, "cascade current clamped to limit");
  return 1;
}

static int test_position_no_clamp_when_max_speed_zero(void)
{
  volatile m2006_control_debug_t cfg;
  pid_t pos_pid;
  pid_inc_t spd_pid;
  int16_t result;

  cfg_reset(&cfg);
  cfg.mode = M2006_CTRL_MODE_POSITION;
  cfg.pos_setpoint_deg = 100.0f;
  cfg.pos_pid_kp = 5.0f;
  cfg.pos_max_speed_rpm = 0.0f;   /* 0 = 不限幅（PID 库 out_min==out_max==0 语义） */
  pos_pid_config(&pos_pid);
  spd_pid_config(&spd_pid);

  /* 误差 100° × kp 5 = 500 rpm：不限幅时应原样输出，不应被钳到 300 */
  result = m2006_control_compute_current(&cfg, 0.0f, 0.0f,
                                         &pos_pid, &spd_pid,
                                         3000, 0, 1U);
  M2006_CONTROL_TEST_ASSERT_FLOAT_NEAR(cfg.speed_cmd_rpm, 500.0f, 0.01f,
                                       "pos no clamp when max speed zero");
  M2006_CONTROL_TEST_ASSERT(result != 0, "pos no-clamp still drives current");
  return 1;
}

static int test_unknown_mode_outputs_zero(void)
{
  volatile m2006_control_debug_t cfg;
  pid_t pos_pid;
  pid_inc_t spd_pid;

  cfg_reset(&cfg);
  cfg.mode = (m2006_ctrl_mode_t)99;
  pos_pid_config(&pos_pid);
  spd_pid_config(&spd_pid);

  return m2006_control_compute_current(&cfg, 0.0f, 0.0f,
                                       &pos_pid, &spd_pid,
                                       3000, 1000, 1U) == 0;
}

int main(void)
{
  M2006_CONTROL_TEST_ASSERT(test_open_loop_direct(), "open loop direct");
  M2006_CONTROL_TEST_ASSERT(test_open_loop_clamped(), "open loop clamped");
  M2006_CONTROL_TEST_ASSERT(test_open_loop_disabled(), "open loop disabled");

  M2006_CONTROL_TEST_ASSERT(test_speed_loop_zero_steady(), "speed zero steady");
  M2006_CONTROL_TEST_ASSERT(test_speed_loop_positive_drive_clamped(),
                      "speed positive drive clamped");
  M2006_CONTROL_TEST_ASSERT(test_speed_loop_reaches_setpoint(),
                      "speed reaches setpoint");

  M2006_CONTROL_TEST_ASSERT(test_position_deadband_zero_output(),
                      "position deadband zero output");
  M2006_CONTROL_TEST_ASSERT(test_position_error_maps_to_speed_cmd(),
                      "position error maps to speed cmd");
  M2006_CONTROL_TEST_ASSERT(test_position_clamps_speed_cmd(),
                      "position clamps speed cmd");
  M2006_CONTROL_TEST_ASSERT(test_position_no_clamp_when_max_speed_zero(),
                      "position no clamp when max speed zero");
  M2006_CONTROL_TEST_ASSERT(test_unknown_mode_outputs_zero(), "unknown mode zero");

  if (g_test_fail != 0)
  {
    (void)printf("m2006_control_test: FAIL (%d/%d)\n",
                 g_test_fail, g_test_count);
    return 1;
  }

  (void)printf("m2006_control_test: PASS (%d asserts)\n", g_test_count);
  return 0;
}
