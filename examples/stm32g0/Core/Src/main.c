/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
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
#include "main.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include "smota_core/inc/smota_packet.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
FDCAN_HandleTypeDef hfdcan2;

I2C_HandleTypeDef hi2c1;

SPI_HandleTypeDef hspi2;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;

/* USER CODE BEGIN PV */
// FDCAN1 接收相关变量
FDCAN_RxHeaderTypeDef RxHeader1;
uint8_t RxData1[8];
uint8_t rx_flag1 = 0;

// FDCAN2 接收相关变量
FDCAN_RxHeaderTypeDef RxHeader2;
uint8_t RxData2[8];
uint8_t rx_flag2 = 0;

// 发送相关变量
FDCAN_TxHeaderTypeDef TxHeader;
uint8_t TxData_To_CAN2[8] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11};
uint8_t TxData_To_CAN1[8] = {0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22};

struct ota_runtime_ctx {
  uint8_t current_version[3];
  uint8_t target_version[3];
  uint32_t firmware_size;
  uint32_t received_size;
  uint8_t handshake_done;
  uint8_t header_done;
  uint8_t transfer_complete;
};

static struct ota_runtime_ctx g_ota_ctx = {
  .current_version = {1, 0, 0},
  .target_version = {1, 0, 0},
  .firmware_size = 0,
  .received_size = 0,
  .handshake_done = 0,
  .header_done = 0,
  .transfer_complete = 0,
};

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_FDCAN2_Init(void);
static void MX_I2C1_Init(void);
static void MX_SPI2_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART3_UART_Init(void);
/* USER CODE BEGIN PFP */
static void ota_reset_transfer(void);
static int ota_send_response(uint8_t cmd, const void *payload, uint16_t payload_len);
static void ota_process_frame(const struct smota_frame *frame);
static void ota_poll_serial(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#define RX_BUF_SIZE 128
uint8_t rx_buffer1[RX_BUF_SIZE];
uint8_t rx_buffer2[RX_BUF_SIZE];

#define OTA_RX_CACHE_SIZE 2048
#define OTA_TX_FRAME_SIZE 320
#define OTA_FLASH_FREE_SIZE (256U * 1024U)
#define USB_CDC_HEARTBEAT_ENABLE 0
static uint8_t g_ota_rx_cache[OTA_RX_CACHE_SIZE];
static uint32_t g_ota_rx_len = 0;
static uint8_t g_ota_tx_frame[OTA_TX_FRAME_SIZE];
/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick.
   */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_FDCAN2_Init();
  MX_I2C1_Init();
  MX_SPI2_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_USART3_UART_Init();
  MX_USB_Device_Init();
  /* USER CODE BEGIN 2 */
  FDCAN_FilterTypeDef sFilterConfig;

  // --- 配置 FDCAN1 过滤器并启动 ---
  sFilterConfig.IdType = FDCAN_STANDARD_ID;
  sFilterConfig.FilterIndex = 0;
  sFilterConfig.FilterType = FDCAN_FILTER_MASK;
  sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
  sFilterConfig.FilterID1 = 0x000; // 接收所有 ID
  sFilterConfig.FilterID2 = 0x000;

  // --- 配置 FDCAN2 过滤器并启动 ---
  HAL_FDCAN_ConfigFilter(&hfdcan2, &sFilterConfig);
  HAL_FDCAN_Start(&hfdcan2);
  HAL_FDCAN_ActivateNotification(&hfdcan2, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0);

  // --- 初始化公用发送 Header (Classic CAN 模式) ---
  TxHeader.IdType = FDCAN_STANDARD_ID;
  TxHeader.TxFrameType = FDCAN_DATA_FRAME;
  TxHeader.DataLength = FDCAN_DLC_BYTES_8;
  TxHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  TxHeader.BitRateSwitch = FDCAN_BRS_OFF;
  TxHeader.FDFormat = FDCAN_CLASSIC_CAN;
  TxHeader.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
  TxHeader.MessageMarker = 0;

  ota_reset_transfer();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1) {
    ota_poll_serial();
#if USB_CDC_HEARTBEAT_ENABLE
    static uint32_t last_heartbeat_tick = 0;
    uint32_t now_tick = HAL_GetTick();
    if ((now_tick - last_heartbeat_tick) >= 1000U) {
      static uint8_t heartbeat_msg[] = "SMOTA_STM32G0_CDC_OK\r\n";
      if (CDC_Transmit_FS(heartbeat_msg, (uint16_t)(sizeof(heartbeat_msg) - 1U)) == USBD_OK) {
        last_heartbeat_tick = now_tick;
      }
    }
#endif
    HAL_Delay(1);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
   */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType =
      RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_HSI48;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV1;
  RCC_OscInitStruct.PLL.PLLN = 16;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType =
      RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
    Error_Handler();
  }
}

/**
 * @brief FDCAN2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_FDCAN2_Init(void) {

  /* USER CODE BEGIN FDCAN2_Init 0 */

  /* USER CODE END FDCAN2_Init 0 */

  /* USER CODE BEGIN FDCAN2_Init 1 */

  /* USER CODE END FDCAN2_Init 1 */
  hfdcan2.Instance = FDCAN2;
  hfdcan2.Init.ClockDivider = FDCAN_CLOCK_DIV1;
  hfdcan2.Init.FrameFormat = FDCAN_FRAME_CLASSIC;
  hfdcan2.Init.Mode = FDCAN_MODE_NORMAL;
  hfdcan2.Init.AutoRetransmission = DISABLE;
  hfdcan2.Init.TransmitPause = DISABLE;
  hfdcan2.Init.ProtocolException = DISABLE;
  hfdcan2.Init.NominalPrescaler = 4;
  hfdcan2.Init.NominalSyncJumpWidth = 1;
  hfdcan2.Init.NominalTimeSeg1 = 12;
  hfdcan2.Init.NominalTimeSeg2 = 3;
  hfdcan2.Init.DataPrescaler = 1;
  hfdcan2.Init.DataSyncJumpWidth = 1;
  hfdcan2.Init.DataTimeSeg1 = 1;
  hfdcan2.Init.DataTimeSeg2 = 1;
  hfdcan2.Init.StdFiltersNbr = 1;
  hfdcan2.Init.ExtFiltersNbr = 0;
  hfdcan2.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
  if (HAL_FDCAN_Init(&hfdcan2) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN FDCAN2_Init 2 */

  /* USER CODE END FDCAN2_Init 2 */
}

/**
 * @brief I2C1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_I2C1_Init(void) {

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x10B17DB5;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
    Error_Handler();
  }

  /** Configure Analogue filter
   */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK) {
    Error_Handler();
  }

  /** Configure Digital filter
   */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */
}

/**
 * @brief SPI2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_SPI2_Init(void) {

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_4BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 7;
  hspi2.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi2.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  if (HAL_SPI_Init(&hspi2) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */
}

/**
 * @brief USART1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART1_UART_Init(void) {

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_RS485Ex_Init(&huart1, UART_DE_POLARITY_HIGH, 1, 1) != HAL_OK) {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) !=
      HAL_OK) {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) !=
      HAL_OK) {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */
}

/**
 * @brief USART2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART2_UART_Init(void) {

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_RS485Ex_Init(&huart2, UART_DE_POLARITY_HIGH, 0, 0) != HAL_OK) {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart2, UART_TXFIFO_THRESHOLD_1_8) !=
      HAL_OK) {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart2, UART_RXFIFO_THRESHOLD_1_8) !=
      HAL_OK) {
    Error_Handler();
  }
  if (HAL_UARTEx_EnableFifoMode(&huart2) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */
}

/**
 * @brief USART3 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART3_UART_Init(void) {

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart3.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart3) != HAL_OK) {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart3, UART_TXFIFO_THRESHOLD_1_8) !=
      HAL_OK) {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart3, UART_RXFIFO_THRESHOLD_1_8) !=
      HAL_OK) {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart3) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */
}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(SPI2_NSS_GPIO_Port, SPI2_NSS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : SPI2_NSS_Pin */
  GPIO_InitStruct.Pin = SPI2_NSS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(SPI2_NSS_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
static void ota_reset_transfer(void) {
  g_ota_ctx.firmware_size = 0;
  g_ota_ctx.received_size = 0;
  g_ota_ctx.handshake_done = 0;
  g_ota_ctx.header_done = 0;
  g_ota_ctx.transfer_complete = 0;
}

static int ota_send_response(uint8_t cmd, const void *payload, uint16_t payload_len) {
  int frame_len = smota_frame_build(cmd, (const uint8_t *)payload, payload_len, g_ota_tx_frame, sizeof(g_ota_tx_frame));
  uint32_t start_tick = HAL_GetTick();
  uint8_t ret;

  if (frame_len <= 0) {
    return -1;
  }

  do {
    ret = CDC_Transmit_FS(g_ota_tx_frame, (uint16_t)frame_len);
    if (ret == USBD_OK) {
      return 0;
    }
    if (HAL_GetTick() - start_tick > 200U) {
      return -2;
    }
  } while (ret == USBD_BUSY);

  return -3;
}

static void ota_process_frame(const struct smota_frame *frame) {
  if (frame == NULL) {
    return;
  }

  switch (frame->header.cmd) {
    case SMOTA_CMD_HANDSHAKE: {
      struct smota_handshake_resp resp;
      const struct smota_handshake_req *req;

      if (frame->header.length < sizeof(struct smota_handshake_req)) {
        return;
      }

      req = (const struct smota_handshake_req *)frame->payload;
      g_ota_ctx.target_version[0] = req->fw_version_major;
      g_ota_ctx.target_version[1] = req->fw_version_minor;
      g_ota_ctx.target_version[2] = req->fw_version_patch;
      g_ota_ctx.firmware_size = req->firmware_size;
      g_ota_ctx.received_size = 0;
      g_ota_ctx.handshake_done = 1;
      g_ota_ctx.header_done = 0;
      g_ota_ctx.transfer_complete = 0;

      memset(&resp, 0, sizeof(resp));
      resp.error_code = 0;
      resp.next_offset = 0;
      resp.max_packet_size = 256;
      resp.mtu_size = 512;
      resp.flash_free_size = OTA_FLASH_FREE_SIZE;
      resp.block_timeout = req->block_timeout;
      resp.install_timeout = req->install_timeout;
      resp.capabilities = 0;
      ota_send_response(SMOTA_CMD_HANDSHAKE_RESP, &resp, sizeof(resp));
      break;
    }

    case SMOTA_CMD_HEADER_INFO: {
      struct smota_header_info_resp resp;
      memset(&resp, 0, sizeof(resp));

      if (g_ota_ctx.handshake_done == 0) {
        resp.error_code = SMOTA_ERR_FLASH_WRITE;
      } else {
        resp.error_code = 0;
        g_ota_ctx.header_done = 1;
        g_ota_ctx.transfer_complete = 0;
      }
      ota_send_response(SMOTA_CMD_HEADER_INFO_RESP, &resp, sizeof(resp));
      break;
    }

    case SMOTA_CMD_DATA_BLOCK: {
      struct smota_data_block_resp resp;
      const struct smota_data_block_req *req;
      uint32_t req_data_total;
      memset(&resp, 0, sizeof(resp));

      if (frame->header.length < sizeof(struct smota_data_block_req)) {
        return;
      }

      req = (const struct smota_data_block_req *)frame->payload;
      req_data_total = sizeof(struct smota_data_block_req) + req->length;

      if (g_ota_ctx.header_done == 0 || g_ota_ctx.handshake_done == 0) {
        resp.error_code = SMOTA_ERR_FLASH_WRITE;
        resp.received_offset = g_ota_ctx.received_size;
      } else if (frame->header.length < req_data_total) {
        resp.error_code = SMOTA_ERR_FLASH_WRITE;
        resp.received_offset = g_ota_ctx.received_size;
      } else if (req->offset != g_ota_ctx.received_size) {
        resp.error_code = SMOTA_ERR_FLASH_WRITE;
        resp.received_offset = g_ota_ctx.received_size;
      } else if ((req->offset + req->length) > g_ota_ctx.firmware_size) {
        resp.error_code = SMOTA_ERR_FLASH_INSUFFICIENT;
        resp.received_offset = g_ota_ctx.received_size;
      } else {
        g_ota_ctx.received_size += req->length;
        resp.error_code = 0;
        resp.received_offset = g_ota_ctx.received_size;
      }

      ota_send_response(SMOTA_CMD_DATA_BLOCK_RESP, &resp, sizeof(resp));
      break;
    }

    case SMOTA_CMD_DATA_COMPLETE: {
      struct smota_transfer_complete_resp resp;
      const struct smota_transfer_complete_req *req;
      memset(&resp, 0, sizeof(resp));

      if (frame->header.length < sizeof(struct smota_transfer_complete_req)) {
        return;
      }

      req = (const struct smota_transfer_complete_req *)frame->payload;
      if (g_ota_ctx.header_done == 0 || g_ota_ctx.handshake_done == 0) {
        resp.error_code = SMOTA_ERR_VERIFY_SHA256_FAILED;
      } else if (req->total_size != g_ota_ctx.firmware_size) {
        resp.error_code = SMOTA_ERR_VERIFY_SHA256_FAILED;
      } else if (g_ota_ctx.received_size != g_ota_ctx.firmware_size) {
        resp.error_code = SMOTA_ERR_VERIFY_SHA256_FAILED;
      } else {
        resp.error_code = 0;
        g_ota_ctx.transfer_complete = 1;
      }
      ota_send_response(SMOTA_CMD_DATA_COMPLETE_RESP, &resp, sizeof(resp));
      break;
    }

    case SMOTA_CMD_INSTALL: {
      struct smota_install_resp resp;
      memset(&resp, 0, sizeof(resp));

      if (g_ota_ctx.transfer_complete == 0) {
        resp.error_code = SMOTA_ERR_INSTALL_BUSY;
        resp.estimated_time_s = 0;
      } else {
        resp.error_code = 0;
        resp.estimated_time_s = 1;
        memcpy(g_ota_ctx.current_version, g_ota_ctx.target_version, sizeof(g_ota_ctx.current_version));
      }
      ota_send_response(SMOTA_CMD_INSTALL_RESP, &resp, sizeof(resp));
      break;
    }

    case SMOTA_CMD_ACTIVATE_CHECK: {
      struct smota_activate_check_resp resp;
      memset(&resp, 0, sizeof(resp));
      resp.error_code = 0;
      resp.fw_version_major = g_ota_ctx.current_version[0];
      resp.fw_version_minor = g_ota_ctx.current_version[1];
      resp.fw_version_patch = g_ota_ctx.current_version[2];
      ota_send_response(SMOTA_CMD_ACTIVATE_CHECK_RESP, &resp, sizeof(resp));
      break;
    }

    case SMOTA_CMD_QUERY_VERSION: {
      struct smota_query_version_resp resp;
      memset(&resp, 0, sizeof(resp));
      resp.error_code = 0;
      resp.fw_version_major = g_ota_ctx.current_version[0];
      resp.fw_version_minor = g_ota_ctx.current_version[1];
      resp.fw_version_patch = g_ota_ctx.current_version[2];
      ota_send_response(SMOTA_CMD_QUERY_VERSION_RESP, &resp, sizeof(resp));
      break;
    }

    default:
      break;
  }
}

static void ota_poll_serial(void) {
  uint8_t read_buffer[64];
  uint32_t read_len = 0;

  if (read_len > 0) {
    if (g_ota_rx_len + read_len > sizeof(g_ota_rx_cache)) {
      g_ota_rx_len = 0;
    }
    memcpy(g_ota_rx_cache + g_ota_rx_len, read_buffer, read_len);
    g_ota_rx_len += read_len;
  }

  while (g_ota_rx_len >= (SMOTA_FRAME_HEADER_SIZE + 2U)) {
    struct smota_frame_header *header = (struct smota_frame_header *)g_ota_rx_cache;
    struct smota_frame frame;
    smota_err_t parse_ret;
    int sof_offset = smota_find_sof(g_ota_rx_cache, (uint16_t)g_ota_rx_len);
    uint32_t frame_len;

    if (sof_offset < 0) {
      g_ota_rx_len = 0;
      break;
    }

    if (sof_offset > 0) {
      g_ota_rx_len -= (uint32_t)sof_offset;
      memmove(g_ota_rx_cache, g_ota_rx_cache + sof_offset, g_ota_rx_len);
      continue;
    }

    frame_len = (uint32_t)SMOTA_FRAME_HEADER_SIZE + (uint32_t)header->length + 2U;
    if (frame_len > sizeof(g_ota_rx_cache)) {
      g_ota_rx_len = 0;
      break;
    }

    if (g_ota_rx_len < frame_len) {
      break;
    }

    parse_ret = smota_frame_parse(g_ota_rx_cache, (uint16_t)frame_len, &frame);
    if (parse_ret != SMOTA_ERR_OK) {
      g_ota_rx_len -= 1U;
      memmove(g_ota_rx_cache, g_ota_rx_cache + 1, g_ota_rx_len);
      continue;
    }

    ota_process_frame(&frame);
    g_ota_rx_len -= frame_len;
    if (g_ota_rx_len > 0) {
      memmove(g_ota_rx_cache, g_ota_rx_cache + frame_len, g_ota_rx_len);
    }
  }
}

/**
 * @brief FDCAN 中断回调函数
 */
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan,
                               uint32_t RxFifo0ITs) {
  if ((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) != 0) {
    if (hfdcan->Instance == FDCAN1) {
      HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &RxHeader1, RxData1);
      rx_flag1 = 1;
    } else if (hfdcan->Instance == FDCAN2) {
      HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &RxHeader2, RxData2);
      rx_flag2 = 1;
    }
  }
}

/**
 * @brief UART发送完成回调函数
 * @note  当一包数据发送完成时调用，用于RS485切换回接收模式
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
  if (huart == &huart1) {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);
  } else if (huart == &huart2) {
  } else if (huart == &huart3) {
  }
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
  if (huart->Instance == USART1) {
    // 1. 发送接收到的所有数据 (阻塞发送是安全的，因为此时对方已经不说话了)
    // 硬件 DE 会自动处理收发切换
    // 3. 【关键】再次开启 IDLE 接收，准备下一条消息
    // 注意：必须重新调用这个函数
    HAL_UARTEx_ReceiveToIdle_IT(&huart1, rx_buffer1, RX_BUF_SIZE);
  }

  if (huart->Instance == USART2) {
    // 1. 发送接收到的所有数据 (阻塞发送是安全的，因为此时对方已经不说话了)
    // 硬件 DE 会自动处理收发切换
    // 3. 【关键】再次开启 IDLE 接收，准备下一条消息
    // 注意：必须重新调用这个函数
    HAL_UARTEx_ReceiveToIdle_IT(&huart2, rx_buffer2, RX_BUF_SIZE);
  }
}

// 可选：处理错误回调，防止 ORE 错误导致死机
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
  if (huart->Instance == USART1) {
    // 如果发生了 ORE (Overrun) 等错误，尝试重启接收
    // 在强干扰环境下很有用
    uint32_t isrflags = READ_REG(huart->Instance->ISR);
    uint32_t cr1its = READ_REG(huart->Instance->CR1);

    // 清除错误标志
    __HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF | UART_CLEAR_NEF |
                                     UART_CLEAR_PEF | UART_CLEAR_FEF);

    // 重新启动接收
    HAL_UARTEx_ReceiveToIdle_IT(&huart1, rx_buffer1, RX_BUF_SIZE);
  }
  if (huart->Instance == USART2) {
    // 如果发生了 ORE (Overrun) 等错误，尝试重启接收
    // 在强干扰环境下很有用
    uint32_t isrflags = READ_REG(huart->Instance->ISR);
    uint32_t cr1its = READ_REG(huart->Instance->CR1);

    // 清除错误标志
    __HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF | UART_CLEAR_NEF |
                                     UART_CLEAR_PEF | UART_CLEAR_FEF);

    // 重新启动接收
    HAL_UARTEx_ReceiveToIdle_IT(&huart2, rx_buffer2, RX_BUF_SIZE);
  }
}

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1) {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line) {
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line
     number, ex: printf("Wrong parameters value: file %s on line %d\r\n", file,
     line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
