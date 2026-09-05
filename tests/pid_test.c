/**
  ******************************************************************************
  * @file    pid_test.c
  * @brief   PID 库主机端单元测试（gcc 编译运行，不依赖嵌入式平台）
  *
  * 编译：gcc -Wall -Wextra -I Lib/pid_lib -o tests/pid_test.exe
  *       tests/pid_test.c Lib/pid_lib/pid.c
  * 运行：./tests/pid_test.exe
  ******************************************************************************
  */
#include "pid.h"

#include <math.h>
#include <stdio.h>

/* ---- 简易测试框架 ---- */
static int g_test_count = 0;
static int g_test_fail = 0;

#define PID_TEST_ASSERT(cond, msg)                                  \
  do {                                                          \
    g_test_count++;                                            \
    if (!(cond)) {                                             \
      fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__); \
      g_test_fail++;                                           \
    }                                                           \
  } while (0)

#define PID_PID_TEST_ASSERT_FLOAT_NEAR(actual, expected, eps, msg)     \
  do {                                                          \
    g_test_count++;                                            \
    if (fabsf((actual) - (expected)) > (eps)) {               \
      fprintf(stderr, "FAIL: %s (line %d): expected %f, got %f\n", \
              msg, __LINE__, (double)(expected), (double)(actual)); \
      g_test_fail++;                                           \
    }                                                           \
  } while (0)

/* ---- 辅助：创建一个基本 P 控制器 ---- */
static void setup_pid(pid_t *pid, float kp, float ki, float kd)
{
  pid->kp = kp;
  pid->ki = ki;
  pid->kd = kd;
  pid->dt = 0.001f;
  pid->out_min = -10000.0f;
  pid->out_max = 10000.0f;
  pid->integral_min = 0.0f;
  pid->integral_max = 0.0f;
  pid->setpoint_rate = 0.0f;
  pid->deadband = 0.0f;
  pid->hysteresis = 0.0f;
  pid->d_filter_alpha = 1.0f;
  pid->integral_hold_error = 0.0f;
}

/* ========================================================================== */
/*                              位置式测试                                    */
/* ========================================================================== */

static void test_positional_p_control(void)
{
  pid_t pid;
  float output;

  setup_pid(&pid, 2.0f, 0.0f, 0.0f);
  PID_TEST_ASSERT(pid_init(&pid) == PID_OK, "p init ok");

  output = pid_update(&pid, 10.0f, 0.0f);
  /* error = 10, P = 2*10 = 20 */
  PID_PID_TEST_ASSERT_FLOAT_NEAR(output, 20.0f, 0.001f, "p output = kp*error");
  PID_PID_TEST_ASSERT_FLOAT_NEAR(pid.error, 10.0f, 0.001f, "p error");
  PID_PID_TEST_ASSERT_FLOAT_NEAR(pid.p_term, 20.0f, 0.001f, "p p_term");
  PID_PID_TEST_ASSERT_FLOAT_NEAR(pid.i_term, 0.0f, 0.001f, "p i_term=0");
  PID_PID_TEST_ASSERT_FLOAT_NEAR(pid.d_term, 0.0f, 0.001f, "p d_term=0");
}

static void test_positional_pi_integral_accumulation(void)
{
  pid_t pid;
  float output;

  setup_pid(&pid, 0.0f, 10.0f, 0.0f);
  pid_init(&pid);

  /* 第一周期：error=5, 梯形积分 = (5+0)/2*0.001 = 0.0025, I = 10*0.0025 = 0.025 */
  output = pid_update(&pid, 5.0f, 0.0f);
  PID_PID_TEST_ASSERT_FLOAT_NEAR(output, 0.025f, 0.0001f, "pi first cycle output");

  /* 第二周期：error=5, 积分 += (5+5)/2*0.001 = 0.005, integral=0.0075, I=0.075 */
  output = pid_update(&pid, 5.0f, 0.0f);
  PID_PID_TEST_ASSERT_FLOAT_NEAR(pid.integral, 0.0075f, 0.0001f, "pi integral after 2 cycles");
  PID_PID_TEST_ASSERT_FLOAT_NEAR(output, 0.075f, 0.001f, "pi second cycle output");
}

static void test_positional_pd_derivative_kick_prevention(void)
{
  pid_t pid;
  float output;

  setup_pid(&pid, 0.0f, 0.0f, 1.0f);
  pid_init(&pid);

  /* 第一周期：measurement=0, filtered=0, d = -1*(0-0)/0.001 = 0 */
  output = pid_update(&pid, 100.0f, 0.0f);
  PID_PID_TEST_ASSERT_FLOAT_NEAR(output, 0.0f, 0.001f, "pd first cycle d=0");

  /* 第二周期：setpoint 不变(100)，measurement 跳到 10
     微分先行：d = -1*(10-0)/0.001 = -10000
     注意：如果是微分 on error，setpoint 不变则 error 不变，d=0
     微分先行基于 measurement，所以 measurement 变化会产生 d 项 */
  output = pid_update(&pid, 100.0f, 10.0f);
  PID_PID_TEST_ASSERT_FLOAT_NEAR(pid.d_term, -10000.0f, 1.0f, "pd derivative on measurement");
}

static void test_positional_output_clamping(void)
{
  pid_t pid;
  float output;

  setup_pid(&pid, 100.0f, 0.0f, 0.0f);
  pid.out_min = -50.0f;
  pid.out_max = 50.0f;
  pid_init(&pid);

  /* error=10, P=1000, 限幅到 50 */
  output = pid_update(&pid, 10.0f, 0.0f);
  PID_PID_TEST_ASSERT_FLOAT_NEAR(output, 50.0f, 0.001f, "output clamped to max");
  PID_TEST_ASSERT(pid.prev_saturated == 1U, "prev_saturated set after clamp");

  /* error=-10, P=-1000, 限幅到 -50 */
  output = pid_update(&pid, -10.0f, 0.0f);
  PID_PID_TEST_ASSERT_FLOAT_NEAR(output, -50.0f, 0.001f, "output clamped to min");
}

static void test_positional_no_clamp_when_both_zero(void)
{
  pid_t pid;
  float output;

  setup_pid(&pid, 100.0f, 0.0f, 0.0f);
  pid.out_min = 0.0f;
  pid.out_max = 0.0f;  /* 不限幅 */
  pid_init(&pid);

  /* error=10, P=1000, 不限幅 */
  output = pid_update(&pid, 10.0f, 0.0f);
  PID_PID_TEST_ASSERT_FLOAT_NEAR(output, 1000.0f, 0.001f, "no output clamp when both zero");
  PID_TEST_ASSERT(pid.prev_saturated == 0U, "not saturated when no clamp");
}

static void test_positional_integral_clamping(void)
{
  pid_t pid;

  setup_pid(&pid, 0.0f, 1.0f, 0.0f);
  pid.integral_min = -0.01f;
  pid.integral_max = 0.01f;
  pid_init(&pid);

  /* 连续多周期大误差，积分应该被限幅到 0.01 */
  for (int i = 0; i < 100; i++)
  {
    pid_update(&pid, 100.0f, 0.0f);
  }
  PID_TEST_ASSERT(pid.integral <= 0.0101f, "integral clamped to max");
  PID_PID_TEST_ASSERT_FLOAT_NEAR(pid.integral, 0.01f, 0.001f, "integral at max clamp");
}

static void test_positional_setpoint_ramp(void)
{
  pid_t pid;

  setup_pid(&pid, 1.0f, 0.0f, 0.0f);
  pid.setpoint_rate = 1000.0f;  /* 1000 单位/秒，dt=0.001 → 每周期最多 +1 */
  pid_init(&pid);

  /* 设定值从 0 跳到 100，第一周期 setpoint_eff 只能到 1 */
  pid_update(&pid, 100.0f, 0.0f);
  PID_PID_TEST_ASSERT_FLOAT_NEAR(pid.setpoint_eff, 1.0f, 0.001f, "ramp first cycle +1");

  /* 第二周期 +1 = 2 */
  pid_update(&pid, 100.0f, 0.0f);
  PID_PID_TEST_ASSERT_FLOAT_NEAR(pid.setpoint_eff, 2.0f, 0.001f, "ramp second cycle +2");

  /* 50 周期后应该到 51（初始 0 + 50*1 + 第一周期的 1 = 51? 实际是 50 次后 50+1=51）
     实际上第一周期到 1，第 50 次调用后到 50 */
  for (int i = 0; i < 48; i++)
  {
    pid_update(&pid, 100.0f, 0.0f);
  }
  PID_PID_TEST_ASSERT_FLOAT_NEAR(pid.setpoint_eff, 50.0f, 0.5f, "ramp reaches 50 after 50 cycles");
}

static void test_positional_deadband(void)
{
  pid_t pid;
  float output;

  setup_pid(&pid, 1.0f, 0.0f, 0.0f);
  pid.deadband = 5.0f;
  pid.hysteresis = 0.0f;
  pid_init(&pid);

  /* error=3 在死区内，输出应为 0 */
  output = pid_update(&pid, 3.0f, 0.0f);
  PID_TEST_ASSERT(pid.in_deadband == 1U, "in deadband when |error|<=5");
  PID_PID_TEST_ASSERT_FLOAT_NEAR(output, 0.0f, 0.001f, "output zero in deadband");
  PID_PID_TEST_ASSERT_FLOAT_NEAR(pid.error, 0.0f, 0.001f, "error zeroed in deadband");

  /* error=10 超出死区，退出 */
  output = pid_update(&pid, 10.0f, 0.0f);
  PID_TEST_ASSERT(pid.in_deadband == 0U, "exit deadband when |error|>5");
  PID_PID_TEST_ASSERT_FLOAT_NEAR(output, 10.0f, 0.001f, "output normal after exit");
}

static void test_positional_deadband_hysteresis(void)
{
  pid_t pid;

  setup_pid(&pid, 1.0f, 0.0f, 0.0f);
  pid.deadband = 5.0f;
  pid.hysteresis = 2.0f;  /* 退出阈值 = 5+2 = 7 */
  pid_init(&pid);

  /* 进入死区：error=3 */
  pid_update(&pid, 3.0f, 0.0f);
  PID_TEST_ASSERT(pid.in_deadband == 1U, "enter deadband at error=3");

  /* error=6 在死区内但 > deadband(5)，由于滞回不退出（需要 >7） */
  pid_update(&pid, 6.0f, 0.0f);
  PID_TEST_ASSERT(pid.in_deadband == 1U, "stay in deadband at error=6 (hysteresis)");

  /* error=8 > 7，退出死区 */
  pid_update(&pid, 8.0f, 0.0f);
  PID_TEST_ASSERT(pid.in_deadband == 0U, "exit deadband at error=8 (>7)");
}

static void test_positional_dt_zero_returns_error(void)
{
  pid_t pid;
  pid_status_t status;

  setup_pid(&pid, 1.0f, 0.0f, 0.0f);
  pid.dt = 0.0f;
  status = pid_init(&pid);
  PID_TEST_ASSERT(status == PID_ERR_INVALID_DT, "init returns invalid dt error");
  PID_TEST_ASSERT(pid.is_valid == 0U, "is_valid=0 after dt error");

  /* update 应该安全返回 0 */
  float output = pid_update(&pid, 10.0f, 0.0f);
  PID_PID_TEST_ASSERT_FLOAT_NEAR(output, 0.0f, 0.001f, "update returns 0 when invalid");
}

static void test_positional_reset_clears_state(void)
{
  pid_t pid;

  setup_pid(&pid, 1.0f, 1.0f, 1.0f);
  pid_init(&pid);

  /* 运行几周期积累状态 */
  for (int i = 0; i < 5; i++)
  {
    pid_update(&pid, 10.0f, 0.0f);
  }
  PID_TEST_ASSERT(pid.integral != 0.0f, "integral non-zero before reset");
  PID_TEST_ASSERT(pid.output != 0.0f, "output non-zero before reset");

  /* 复位 */
  pid_reset(&pid);
  PID_PID_TEST_ASSERT_FLOAT_NEAR(pid.integral, 0.0f, 0.001f, "integral cleared after reset");
  PID_PID_TEST_ASSERT_FLOAT_NEAR(pid.output, 0.0f, 0.001f, "output cleared after reset");
  PID_PID_TEST_ASSERT_FLOAT_NEAR(pid.error, 0.0f, 0.001f, "error cleared after reset");
  PID_TEST_ASSERT(pid.is_valid == 1U, "is_valid preserved after reset");
}

static void test_positional_conditional_integration_hold_error(void)
{
  pid_t pid;

  setup_pid(&pid, 0.0f, 1.0f, 0.0f);
  pid.integral_hold_error = 5.0f;  /* |error| > 5 时停止积分 */
  pid_init(&pid);

  /* error=10 > 5，积分应该停止 */
  pid_update(&pid, 10.0f, 0.0f);
  PID_PID_TEST_ASSERT_FLOAT_NEAR(pid.integral, 0.0f, 0.0001f, "no integral when error > hold threshold");

  /* error=3 <= 5，积分应该进行 */
  pid_update(&pid, 3.0f, 0.0f);
  PID_TEST_ASSERT(pid.integral > 0.0f, "integral accumulates when error <= hold threshold");
}

static void test_positional_derivative_filter(void)
{
  pid_t pid;

  setup_pid(&pid, 0.0f, 0.0f, 1.0f);
  pid.d_filter_alpha = 0.5f;  /* 一阶低通，alpha=0.5 */
  pid_init(&pid);

  /* 第一周期：measurement=10, filtered = 0.5*10 + 0.5*0 = 5
     d = -1*(5-0)/0.001 = -5000 */
  pid_update(&pid, 0.0f, 10.0f);
  PID_PID_TEST_ASSERT_FLOAT_NEAR(pid.measurement_filtered, 5.0f, 0.01f, "filtered measurement first cycle");
  PID_PID_TEST_ASSERT_FLOAT_NEAR(pid.d_term, -5000.0f, 1.0f, "d term with filter first cycle");

  /* 第二周期：measurement=10, filtered = 0.5*10 + 0.5*5 = 7.5
     d = -1*(7.5-5)/0.001 = -2500 */
  pid_update(&pid, 0.0f, 10.0f);
  PID_PID_TEST_ASSERT_FLOAT_NEAR(pid.measurement_filtered, 7.5f, 0.01f, "filtered measurement second cycle");
  PID_PID_TEST_ASSERT_FLOAT_NEAR(pid.d_term, -2500.0f, 1.0f, "d term with filter second cycle");
}

/* ========================================================================== */
/*                              增量式测试                                    */
/* ========================================================================== */

static void setup_pid_inc(pid_inc_t *pid, float kp, float ki, float kd)
{
  pid->kp = kp;
  pid->ki = ki;
  pid->kd = kd;
  pid->dt = 0.001f;
  pid->out_min = -10000.0f;
  pid->out_max = 10000.0f;
  pid->setpoint_rate = 0.0f;
  pid->deadband = 0.0f;
  pid->hysteresis = 0.0f;
  pid->d_filter_alpha = 1.0f;
}

static void test_incremental_p_control(void)
{
  pid_inc_t pid;
  float output;

  setup_pid_inc(&pid, 2.0f, 0.0f, 0.0f);
  PID_TEST_ASSERT(pid_inc_init(&pid) == PID_OK, "inc init ok");

  /* 第一周期：error=10, prev_error=0, Δp = 2*(10-0) = 20, output = 0+20 = 20 */
  output = pid_inc_update(&pid, 10.0f, 0.0f);
  PID_PID_TEST_ASSERT_FLOAT_NEAR(output, 20.0f, 0.001f, "inc first cycle output");
  PID_PID_TEST_ASSERT_FLOAT_NEAR(pid.p_term, 20.0f, 0.001f, "inc delta_p first cycle");
  PID_PID_TEST_ASSERT_FLOAT_NEAR(pid.delta_u, 20.0f, 0.001f, "inc delta_u first cycle");

  /* 第二周期：error=10, prev_error=10, Δp = 2*(10-10) = 0, output = 20+0 = 20 */
  output = pid_inc_update(&pid, 10.0f, 0.0f);
  PID_PID_TEST_ASSERT_FLOAT_NEAR(output, 20.0f, 0.001f, "inc second cycle output holds");
  PID_PID_TEST_ASSERT_FLOAT_NEAR(pid.p_term, 0.0f, 0.001f, "inc delta_p second cycle=0");
}

static void test_incremental_pi_control(void)
{
  pid_inc_t pid;
  float output;

  setup_pid_inc(&pid, 0.0f, 10.0f, 0.0f);
  pid_inc_init(&pid);

  /* 第一周期：error=5, Δi = 10*(5+0)/2*0.001 = 0.025, output = 0.025 */
  output = pid_inc_update(&pid, 5.0f, 0.0f);
  PID_PID_TEST_ASSERT_FLOAT_NEAR(output, 0.025f, 0.0001f, "inc pi first cycle");

  /* 第二周期：error=5, Δi = 10*(5+5)/2*0.001 = 0.05, output = 0.025+0.05 = 0.075 */
  output = pid_inc_update(&pid, 5.0f, 0.0f);
  PID_PID_TEST_ASSERT_FLOAT_NEAR(output, 0.075f, 0.001f, "inc pi second cycle");
}

static void test_incremental_output_clamping(void)
{
  pid_inc_t pid;
  float output;

  setup_pid_inc(&pid, 100.0f, 0.0f, 0.0f);
  pid.out_min = -50.0f;
  pid.out_max = 50.0f;
  pid_inc_init(&pid);

  /* 第一周期：Δp=1000, output_unsat=1000, 限幅到 50 */
  output = pid_inc_update(&pid, 10.0f, 0.0f);
  PID_PID_TEST_ASSERT_FLOAT_NEAR(output, 50.0f, 0.001f, "inc output clamped");

  /* 第二周期：error 不变，Δp=0, output_unsat=50+0=50, 仍为 50 */
  output = pid_inc_update(&pid, 10.0f, 0.0f);
  PID_PID_TEST_ASSERT_FLOAT_NEAR(output, 50.0f, 0.001f, "inc output holds at clamp");
}

static void test_incremental_reset_clears_accumulator(void)
{
  pid_inc_t pid;

  setup_pid_inc(&pid, 1.0f, 1.0f, 0.0f);
  pid_inc_init(&pid);

  for (int i = 0; i < 5; i++)
  {
    pid_inc_update(&pid, 10.0f, 0.0f);
  }
  PID_TEST_ASSERT(pid.output != 0.0f, "inc output non-zero before reset");

  pid_inc_reset(&pid);
  PID_PID_TEST_ASSERT_FLOAT_NEAR(pid.output, 0.0f, 0.001f, "inc accumulator cleared after reset");
}

static void test_incremental_dt_zero_error(void)
{
  pid_inc_t pid;

  setup_pid_inc(&pid, 1.0f, 0.0f, 0.0f);
  pid.dt = 0.0f;
  PID_TEST_ASSERT(pid_inc_init(&pid) == PID_ERR_INVALID_DT, "inc init invalid dt");
  PID_PID_TEST_ASSERT_FLOAT_NEAR(pid_inc_update(&pid, 10.0f, 0.0f), 0.0f, 0.001f,
                          "inc update returns 0 when invalid");
}

/* ========================================================================== */
/*                              main                                          */
/* ========================================================================== */

int main(void)
{
  printf("PID library unit tests\n");
  printf("=======================\n");

  /* 位置式 */
  test_positional_p_control();
  test_positional_pi_integral_accumulation();
  test_positional_pd_derivative_kick_prevention();
  test_positional_output_clamping();
  test_positional_no_clamp_when_both_zero();
  test_positional_integral_clamping();
  test_positional_setpoint_ramp();
  test_positional_deadband();
  test_positional_deadband_hysteresis();
  test_positional_dt_zero_returns_error();
  test_positional_reset_clears_state();
  test_positional_conditional_integration_hold_error();
  test_positional_derivative_filter();

  /* 增量式 */
  test_incremental_p_control();
  test_incremental_pi_control();
  test_incremental_output_clamping();
  test_incremental_reset_clears_accumulator();
  test_incremental_dt_zero_error();

  printf("=======================\n");
  printf("Total: %d, Passed: %d, Failed: %d\n",
         g_test_count, g_test_count - g_test_fail, g_test_fail);

  if (g_test_fail > 0)
  {
    printf("RESULT: FAIL\n");
    return 1;
  }
  printf("RESULT: PASS\n");
  return 0;
}
