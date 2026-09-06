/**
  ******************************************************************************
  * @file    m2006_hal.c
  * @brief   M2006 总线在本工程的 FDCAN 适配层实现
  *
  * 使用 FDCAN2（PB12=RX / PB13=TX，AF9，经典 CAN 1Mbps）与 C610 电调通信。
  * 反馈帧标识符 0x201~0x208（= 0x200 + 电调ID），控制帧 0x200 / 0x1FF。
  *
  * 本模块只做外设收发，闭环计算与打包由 m2006_lib（m2006_motor / m2006_bus）
  * 完成，控制流程见 m2006_control_task.c。
  ******************************************************************************
  */
#include "m2006_hal.h"

#include "fdcan.h"
#include "main.h"
#include "cmsis_os.h"

/* 总线实例（本模块持有，RX 回调与任务共用） */
m2006_bus_t m2006_hal_bus;

/* 控制帧发送失败计数 */
uint32_t m2006_hal_tx_fail_count = 0U;

void m2006_hal_init(void)
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

uint32_t m2006_hal_tx_frame(uint32_t frame_id,
                            const uint8_t frame_data[M2006_PROTOCOL_FRAME_BYTES])
{
  FDCAN_TxHeaderTypeDef tx_header;

  if (frame_data == 0)
  {
    return 0U;
  }

  tx_header.Identifier = frame_id;
  tx_header.IdType = FDCAN_STANDARD_ID;
  tx_header.TxFrameType = FDCAN_DATA_FRAME;
  tx_header.DataLength = FDCAN_DLC_BYTES_8;
  tx_header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  tx_header.BitRateSwitch = FDCAN_BRS_OFF;
  tx_header.FDFormat = FDCAN_CLASSIC_CAN;
  tx_header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
  tx_header.MessageMarker = 0U;

  if (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan2, &tx_header, (uint8_t *)frame_data)
      != HAL_OK)
  {
    m2006_hal_tx_fail_count++;
    return 0U;
  }

  return 1U;
}

/**
  * @brief  FDCAN FIFO0 接收完成回调（HAL 弱回调，此处覆盖）
  * @note   只处理 FDCAN2 的电调反馈帧，取帧后交给总线实例分发到对应电机
  */
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan,
                               uint32_t rx_fifo0_it_flags)
{
  if ((hfdcan == &hfdcan2)
      && ((rx_fifo0_it_flags & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) != 0U))
  {
    FDCAN_RxHeaderTypeDef rx_header;
    uint8_t rx_data[M2006_PROTOCOL_FRAME_BYTES];

    if (HAL_FDCAN_GetRxMessage(&hfdcan2,
                               FDCAN_RX_FIFO0,
                               &rx_header,
                               rx_data) == HAL_OK)
    {
      m2006_bus_handle_rx_frame(&m2006_hal_bus,
                                rx_header.Identifier,
                                rx_data,
                                osKernelGetTickCount());
    }
  }
}
