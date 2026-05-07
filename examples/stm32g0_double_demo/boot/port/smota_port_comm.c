/*
 * Copyright (c) 2025 by Lu Xianfan.
 * @FilePath     : smota_port_comm.c
 * @Author       : lxf
 * @Date         : 2026-04-28 16:10:00
 * @LastEditors  : lxf_zjnb@qq.com
 * @LastEditTime : 2026-04-28 16:10:00
 * @Brief        : STM32G0 平台 smOTA 通信驱动实现
 */

/*---------- includes ----------*/
#include "usbd_cdc_if.h"
#include "smota_port_internal.h"

/*---------- macro ----------*/
#define SMOTA_PORT_TX_TIMEOUT_MS     100U
#define SMOTA_PORT_RX_POLL_DELAY_MS  1U

/*---------- type define ----------*/

/*---------- variable prototype ----------*/

/*---------- function prototype ----------*/
static int comm_init(void);
static int comm_deinit(void);
static int comm_send(const uint8_t *data, uint32_t size);
static int comm_receive(uint8_t *data, uint32_t size, uint32_t timeout);

/*---------- variable ----------*/
struct smota_comm_driver g_smota_comm_driver = {
    .init = comm_init,
    .deinit = comm_deinit,
    .send = comm_send,
    .receive = comm_receive,
};

/*---------- function ----------*/
/**
 * @brief  通信初始化
 * @return 0=成功
 */
static int comm_init(void)
{
    return 0;
}

/**
 * @brief  通信去初始化
 * @return 0=成功
 */
static int comm_deinit(void)
{
    return 0;
}

/**
 * @brief  通过 USB CDC 发送数据
 * @param  data: 数据指针
 * @param  size: 数据长度
 * @return 实际发送字节数，<0=失败
 */
static int comm_send(const uint8_t *data, uint32_t size)
{
    uint32_t start_tick;
    uint8_t ret;

    if (data == NULL || size == 0U || size > 0xFFFFU) {
        return -1;
    }

    start_tick = HAL_GetTick();
    while (1) {
        ret = CDC_Transmit_FS((uint8_t *)data, (uint16_t)size);
        if (ret == USBD_OK) {
            return (int)size;
        }

        if (ret != USBD_BUSY) {
            return -2;
        }

        if ((HAL_GetTick() - start_tick) >= SMOTA_PORT_TX_TIMEOUT_MS) {
            return -3;
        }
    }
}

/**
 * @brief  从 USB CDC 接收数据
 * @param  data: 输出缓冲区
 * @param  size: 期望接收长度
 * @param  timeout: 超时时间
 * @return 实际接收字节数，<0=失败
 */
static int comm_receive(uint8_t *data, uint32_t size, uint32_t timeout)
{
    uint32_t start_tick;
    uint32_t read_len;

    if (data == NULL || size == 0U) {
        return 0;
    }

    read_len = CDC_Read_FS(data, size);
    if (read_len > 0U || timeout == 0U) {
        return (int)read_len;
    }

    start_tick = HAL_GetTick();
    while ((HAL_GetTick() - start_tick) < timeout) {
        HAL_Delay(SMOTA_PORT_RX_POLL_DELAY_MS);
        read_len = CDC_Read_FS(data, size);
        if (read_len > 0U) {
            return (int)read_len;
        }
    }

    return 0;
}

/*---------- end of file ----------*/
