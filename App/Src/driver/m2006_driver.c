/**
  ******************************************************************************
  * @file    m2006_driver.c
  * @brief   M2006 电机（C610 电调）驱动层实现
  *
  * 使用 FDCAN2（PB12=RX / PB13=TX，AF9，经典 CAN 1Mbps）与 C610 电调通信。
  * 电调 ID 为 2，反馈帧标识符 0x202，控制帧标识符 0x200。
  *
  * 控制流程（由 1kHz 任务周期调用 m2006_driver_update()）：
  *   目标电流（开环：Watch 设定；闭环：m2006_control 层写入）-> 电流钳位
 *   -> 使能/超时/超速安全门 -> 编码 -> FDCAN2 发送
  *
  * 所有调试变量集中在结构体 m2006_debug 中，Keil Watch 添加该实例即可
  * 一次查看并修改全部成员。
  ******************************************************************************
  */
#include "m2006_driver.h"
#include "m2006_protocol.h"
#include "fdcan.h"
#include "main.h"
#include "cmsis_os.h"

/* 本电机电调参数 */
#define M2006_DRIVER_MOTOR_ID (2U)          /* 当前 C610 电调 ID */
#define M2006_DRIVER_FEEDBACK_ID (0x202U)   /* 反馈帧标识符 = 0x200 + 电调ID */

/* 安全保护默认值 */
#define M2006_DRIVER_RX_TIMEOUT_MS (500U)   /* 反馈超时判定时间 */
#define M2006_DRIVER_CURRENT_LIMIT_DEFAULT (3000)   /* 默认电流钳位 3A */
#define M2006_DRIVER_SPEED_LIMIT_DEFAULT_RPM (500)  /* 默认输出轴转速限幅 */

/* 输出轴角度换算：转子一圈 360° 经 36:1 减速 = 输出轴 10°/圈，再按 8191 归一 */
#define M2006_DRIVER_ANGLE_SCALE_DEG \
    (360.0f / (float)M2006_PROTOCOL_GEAR_RATIO / 8191.0f)
/* 输出轴力矩换算（基于 M2006 官方转矩常数）：
 * 1) C610 手册口径：实际电流 A = torque_raw / 1000（10000 LSB = 10A）；
 * 2) M2006 官方手册转矩常数 = 0.18 N·m/A（输出轴等效值，已含 36:1 减速比与传动效率）；
 * 3) 输出轴力矩 = 电流(A) * 0.18 N·m/A；
 * 4) 综合：torque_out_nm = torque_raw * 0.18 / 1000。
 * 说明：0.18 N·m/A 为输出轴等效转矩常数；若为电机本体值则经减速后会远超额定，
 *       故判定为输出轴等效值，与额定点（3A -> 约 0.54 N·m）自洽。 */
#define M2006_DRIVER_TORQUE_SCALE_NM (0.18f / 1000.0f)

/* ---- 调试变量面板定义（Keil Watch 添加 m2006_debug 即可查看/修改） ---- */
volatile m2006_debug_t m2006_debug = {
  .is_enabled = 0U,
  .current_setpoint = 0,
  .current_limit = M2006_DRIVER_CURRENT_LIMIT_DEFAULT,
  .speed_limit_rpm = M2006_DRIVER_SPEED_LIMIT_DEFAULT_RPM,
  .angle_raw = 0U,
  .speed_rpm = 0,
  .torque_raw = 0,
  .angle_out_deg = 0.0f,
  .speed_out_rpm = 0.0f,
  .torque_out_nm = 0.0f,
  .output_current = 0,
  .rx_msg_count = 0U,
  .is_rx_timeout = 1U,
  .tx_fail_count = 0U,
};

/* 最近一次收到反馈的时刻，由接收中断更新 */
static uint32_t m2006_last_rx_tick = 0U;

void m2006_driver_init(void)
{
  FDCAN_FilterTypeDef filter_config;

  /* 标准帧过滤器 0：掩码模式接收 0x200~0x207 的反馈帧，进入 FIFO0 */
  filter_config.IdType = FDCAN_STANDARD_ID;
  filter_config.FilterIndex = 0U;
  filter_config.FilterType = FDCAN_FILTER_MASK;
  filter_config.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
  filter_config.FilterID1 = 0x200U;   /* 匹配基准标识符 */
  filter_config.FilterID2 = 0x1F8U;   /* 掩码：高 8 位精确匹配，低 3 位任意 */
  (void)HAL_FDCAN_ConfigFilter(&hfdcan2, &filter_config);

  /* 启动 FDCAN2 并启用 FIFO0 新消息中断（收到反馈时进入回调取帧） */
  (void)HAL_FDCAN_Start(&hfdcan2);
  (void)HAL_FDCAN_ActivateNotification(&hfdcan2,
                                       FDCAN_IT_RX_FIFO0_NEW_MESSAGE,
                                       0U);
}

void m2006_driver_update(void)
{
  int16_t output_current;
  int16_t current_limit;
  int16_t speed_limit_rpm;
  uint8_t control_data[M2006_PROTOCOL_FRAME_BYTES];
  FDCAN_TxHeaderTypeDef tx_header;

  /* 反馈超时判定：超过 M2006_DRIVER_RX_TIMEOUT_MS 未收到反馈则断输出 */
  if ((osKernelGetTickCount() - m2006_last_rx_tick) > M2006_DRIVER_RX_TIMEOUT_MS)
  {
    m2006_debug.is_rx_timeout = 1U;
  }
  else
  {
    m2006_debug.is_rx_timeout = 0U;
  }

  /* 换算输出轴物理量（角度/转速/力矩） */
  m2006_debug.angle_out_deg = (float)m2006_debug.angle_raw
                              * M2006_DRIVER_ANGLE_SCALE_DEG;
  m2006_debug.speed_out_rpm = (float)m2006_debug.speed_rpm
                              / (float)M2006_PROTOCOL_GEAR_RATIO;
  m2006_debug.torque_out_nm = (float)m2006_debug.torque_raw
                              * M2006_DRIVER_TORQUE_SCALE_NM;

  /* 目标电流钳位到 ±current_limit */
  current_limit = m2006_debug.current_limit;
  output_current = m2006_debug.current_setpoint;
  if (output_current > current_limit)
  {
    output_current = current_limit;
  }
  else if (output_current < -current_limit)
  {
    output_current = -current_limit;
  }

  /* 安全门：断使能、反馈超时、超速任一成立则输出 0 */
  if ((!m2006_debug.is_enabled) || (m2006_debug.is_rx_timeout != 0U))
  {
    output_current = 0;
  }
  else
  {
    speed_limit_rpm = m2006_debug.speed_limit_rpm;
    if ((m2006_debug.speed_out_rpm > (float)speed_limit_rpm)
        || (m2006_debug.speed_out_rpm < -(float)speed_limit_rpm))
    {
      output_current = 0;
    }
  }

  m2006_debug.output_current = output_current;

  /* 编码控制帧并发送到 FDCAN2 发送 FIFO */
  (void)m2006_protocol_encode_control(M2006_DRIVER_MOTOR_ID,
                                      output_current,
                                      control_data);

  tx_header.Identifier = M2006_PROTOCOL_CONTROL_ID;
  tx_header.IdType = FDCAN_STANDARD_ID;
  tx_header.TxFrameType = FDCAN_DATA_FRAME;
  tx_header.DataLength = FDCAN_DLC_BYTES_8;
  tx_header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  tx_header.BitRateSwitch = FDCAN_BRS_OFF;
  tx_header.FDFormat = FDCAN_CLASSIC_CAN;
  tx_header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
  tx_header.MessageMarker = 0U;

  if (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan2, &tx_header, control_data)
      != HAL_OK)
  {
    m2006_debug.tx_fail_count++;
  }
}

/**
  * @brief  FDCAN FIFO0 接收完成回调（HAL 弱回调，此处覆盖）
  * @note   只处理 FDCAN2 的电调反馈帧，解析后更新调试变量面板
  */
void m2006_driver_set_current_setpoint(int16_t current_raw)
{
  m2006_debug.current_setpoint = current_raw;
}

void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan,
                               uint32_t rx_fifo0_it_flags)
{
  if ((hfdcan == &hfdcan2)
      && ((rx_fifo0_it_flags & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) != 0U))
  {
    FDCAN_RxHeaderTypeDef rx_header;
    uint8_t rx_data[M2006_PROTOCOL_FRAME_BYTES];
    m2006_measure_t measure;

    if (HAL_FDCAN_GetRxMessage(&hfdcan2,
                               FDCAN_RX_FIFO0,
                               &rx_header,
                               rx_data) == HAL_OK)
    {
      if (rx_header.Identifier == M2006_DRIVER_FEEDBACK_ID)
      {
        if (m2006_protocol_parse_feedback(rx_data, &measure) == 1U)
        {
          m2006_debug.angle_raw = measure.angle_raw;
          m2006_debug.speed_rpm = measure.speed_rpm;
          m2006_debug.torque_raw = measure.torque_raw;
          m2006_debug.rx_msg_count++;
          m2006_last_rx_tick = osKernelGetTickCount();
        }
      }
    }
  }
}
