/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    can.c
  * @brief   This file provides code for the configuration
  *          of the CAN instances.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "can.h"

/* USER CODE BEGIN 0 */

#define CAN_TX_TIMEOUT_MS 100U

static uint32_t CAN_GetTxCompleteFlag(uint32_t mailbox)
{
  if (mailbox == CAN_TX_MAILBOX0)
  {
    return CAN_FLAG_RQCP0;
  }
  if (mailbox == CAN_TX_MAILBOX1)
  {
    return CAN_FLAG_RQCP1;
  }
  return CAN_FLAG_RQCP2;
}

static uint32_t CAN_GetTxOkFlag(uint32_t mailbox)
{
  if (mailbox == CAN_TX_MAILBOX0)
  {
    return CAN_FLAG_TXOK0;
  }
  if (mailbox == CAN_TX_MAILBOX1)
  {
    return CAN_FLAG_TXOK1;
  }
  return CAN_FLAG_TXOK2;
}

/* USER CODE END 0 */

CAN_HandleTypeDef hcan;

/* CAN init function */
void MX_CAN_Init(void)
{

  /* USER CODE BEGIN CAN_Init 0 */

  /* USER CODE END CAN_Init 0 */

  /* USER CODE BEGIN CAN_Init 1 */

  /* USER CODE END CAN_Init 1 */
  hcan.Instance = CAN1;
  hcan.Init.Prescaler = 4;
  hcan.Init.Mode = CAN_MODE_NORMAL;
  hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan.Init.TimeSeg1 = CAN_BS1_13TQ;
  hcan.Init.TimeSeg2 = CAN_BS2_4TQ;
  hcan.Init.TimeTriggeredMode = DISABLE;
  hcan.Init.AutoBusOff = ENABLE;
  hcan.Init.AutoWakeUp = DISABLE;
  hcan.Init.AutoRetransmission = ENABLE;
  hcan.Init.ReceiveFifoLocked = DISABLE;
  hcan.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN_Init 2 */

  /* USER CODE END CAN_Init 2 */

}

void HAL_CAN_MspInit(CAN_HandleTypeDef* canHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(canHandle->Instance==CAN1)
  {
  /* USER CODE BEGIN CAN1_MspInit 0 */

  /* USER CODE END CAN1_MspInit 0 */
    /* CAN1 clock enable */
    __HAL_RCC_CAN1_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**CAN GPIO Configuration
    PA11     ------> CAN_RX
    PA12     ------> CAN_TX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN CAN1_MspInit 1 */

  /* USER CODE END CAN1_MspInit 1 */
  }
}

void HAL_CAN_MspDeInit(CAN_HandleTypeDef* canHandle)
{

  if(canHandle->Instance==CAN1)
  {
  /* USER CODE BEGIN CAN1_MspDeInit 0 */

  /* USER CODE END CAN1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_CAN1_CLK_DISABLE();

    /**CAN GPIO Configuration
    PA11     ------> CAN_RX
    PA12     ------> CAN_TX
    */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_11|GPIO_PIN_12);

  /* USER CODE BEGIN CAN1_MspDeInit 1 */

  /* USER CODE END CAN1_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */
HAL_StatusTypeDef CAN_Start(void)
{
  return HAL_CAN_Start(&hcan);
}

/* Send one standard data frame and wait for a real transmission result. */
HAL_StatusTypeDef CAN_SendMessage(uint16_t id, const uint8_t *data, uint8_t len)
{
  CAN_TxHeaderTypeDef tx_header = {0};
  uint8_t tx_data[8] = {0};
  uint8_t index;
  uint32_t tx_mailbox;
  uint32_t complete_flag;
  uint32_t tx_ok_flag;
  uint32_t tick_start;
  HAL_StatusTypeDef status;

  if ((id > 0x7FFU) || (len > 8U) || ((data == NULL) && (len > 0U)))
  {
    return HAL_ERROR;
  }

  if (HAL_CAN_GetState(&hcan) != HAL_CAN_STATE_LISTENING)
  {
    return HAL_ERROR;
  }

  for (index = 0U; index < len; index++)
  {
    tx_data[index] = data[index];
  }

  tx_header.StdId = id;
  tx_header.ExtId = 0U;
  tx_header.RTR = CAN_RTR_DATA;
  tx_header.IDE = CAN_ID_STD;
  tx_header.DLC = len;
  tx_header.TransmitGlobalTime = DISABLE;

  status = HAL_CAN_AddTxMessage(&hcan, &tx_header, tx_data, &tx_mailbox);
  if (status != HAL_OK)
  {
    return status;
  }

  complete_flag = CAN_GetTxCompleteFlag(tx_mailbox);
  tx_ok_flag = CAN_GetTxOkFlag(tx_mailbox);
  tick_start = HAL_GetTick();

  while (HAL_CAN_IsTxMessagePending(&hcan, tx_mailbox) != 0U)
  {
    if ((HAL_GetTick() - tick_start) >= CAN_TX_TIMEOUT_MS)
    {
      (void)HAL_CAN_AbortTxRequest(&hcan, tx_mailbox);
      return HAL_TIMEOUT;
    }
  }

  status = (__HAL_CAN_GET_FLAG(&hcan, tx_ok_flag) != 0U) ? HAL_OK : HAL_ERROR;
  __HAL_CAN_CLEAR_FLAG(&hcan, complete_flag);

  return status;
}

/* USER CODE END 1 */

