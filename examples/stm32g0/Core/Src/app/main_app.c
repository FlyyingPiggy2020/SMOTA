/*
 * Copyright (c) 2025 by Lu Xianfan.
 * @FilePath     : app/main_app.c
 * @Author       : lxf
 * @Date         : 2026-04-27 10:00:00
 * @LastEditors  : lxf_zjnb@qq.com
 * @LastEditTime : 2026-04-27 10:00:00
 * @Brief        : app层主循环任务实现
 */

/*---------- includes ----------*/
#include <string.h>
#include "main.h"
#include "app/main_app.h"
#include "usbd_cdc_if.h"
#include "smota_core/inc/smota_packet.h"

/*---------- macro ----------*/
#define MAIN_APP_OTA_RX_CACHE_SIZE      2048U
#define MAIN_APP_OTA_TX_FRAME_SIZE      320U
#define MAIN_APP_OTA_FLASH_FREE_SIZE    (256U * 1024U)
#define MAIN_APP_USB_HEARTBEAT_ENABLE   0

/*---------- type define ----------*/
struct ota_runtime_ctx {
    uint8_t current_version[3];
    uint8_t target_version[3];
    uint32_t firmware_size;
    uint32_t received_size;
    uint8_t handshake_done;
    uint8_t header_done;
    uint8_t transfer_complete;
};

/*---------- variable prototype ----------*/

/*---------- function prototype ----------*/
static void ota_reset_transfer(void);
static int ota_send_response(uint8_t cmd, const void *payload, uint16_t payload_len);
static void ota_process_frame(const struct smota_frame *frame);
static void ota_poll_serial(void);

/*---------- variable ----------*/
static struct ota_runtime_ctx g_ota_ctx = {
    .current_version = {1, 0, 0},
    .target_version = {1, 0, 0},
    .firmware_size = 0,
    .received_size = 0,
    .handshake_done = 0,
    .header_done = 0,
    .transfer_complete = 0,
};

static uint8_t g_ota_rx_cache[MAIN_APP_OTA_RX_CACHE_SIZE];
static uint32_t g_ota_rx_len = 0;
static uint8_t g_ota_tx_frame[MAIN_APP_OTA_TX_FRAME_SIZE];

#if MAIN_APP_USB_HEARTBEAT_ENABLE
static uint32_t g_last_heartbeat_tick = 0;
#endif

/*---------- function ----------*/

/**
 * @brief  Initialize app tasks.
 */
void main_app_init(void)
{
    ota_reset_transfer();
}

/**
 * @brief  Poll app tasks.
 */
void main_app_poll(void)
{
    ota_poll_serial();

#if MAIN_APP_USB_HEARTBEAT_ENABLE
    uint32_t now_tick = HAL_GetTick();
    static uint8_t heartbeat_msg[] = "SMOTA_STM32G0_CDC_OK\r\n";

    if ((now_tick - g_last_heartbeat_tick) >= 1000U) {
        if (CDC_Transmit_FS(heartbeat_msg, (uint16_t)(sizeof(heartbeat_msg) - 1U)) == USBD_OK) {
            g_last_heartbeat_tick = now_tick;
        }
    }
#endif

    HAL_Delay(1);
}

/**
 * @brief  重置一次升级会话上下文
 */
static void ota_reset_transfer(void)
{
    g_ota_ctx.firmware_size = 0;
    g_ota_ctx.received_size = 0;
    g_ota_ctx.handshake_done = 0;
    g_ota_ctx.header_done = 0;
    g_ota_ctx.transfer_complete = 0;
}

/**
 * @brief  发送OTA协议响应帧
 * @param  cmd: 响应命令字
 * @param  payload: 响应负载
 * @param  payload_len: 负载长度
 * @return 0=成功, <0=失败
 */
static int ota_send_response(uint8_t cmd, const void *payload, uint16_t payload_len)
{
    int frame_len;
    uint32_t start_tick;
    uint8_t ret;

    frame_len = smota_frame_build(cmd,
                                  (const uint8_t *)payload,
                                  payload_len,
                                  g_ota_tx_frame,
                                  sizeof(g_ota_tx_frame));
    if (frame_len <= 0) {
        return -1;
    }

    start_tick = HAL_GetTick();
    do {
        ret = CDC_Transmit_FS(g_ota_tx_frame, (uint16_t)frame_len);
        if (ret == USBD_OK) {
            return 0;
        }
        if ((HAL_GetTick() - start_tick) > 200U) {
            return -2;
        }
    } while (ret == USBD_BUSY);

    return -3;
}

/**
 * @brief  处理单帧OTA协议命令
 * @param  frame: 已完成CRC校验的协议帧
 */
static void ota_process_frame(const struct smota_frame *frame)
{
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
            resp.flash_free_size = MAIN_APP_OTA_FLASH_FREE_SIZE;
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
                memcpy(g_ota_ctx.current_version,
                       g_ota_ctx.target_version,
                       sizeof(g_ota_ctx.current_version));
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

/**
 * @brief  轮询CDC缓冲并解析OTA帧
 */
static void ota_poll_serial(void)
{
    uint8_t read_buffer[64];
    uint32_t read_len = 0;

    if (read_len > 0) {
        if ((g_ota_rx_len + read_len) > sizeof(g_ota_rx_cache)) {
            g_ota_rx_len = 0;
        }
        memcpy(g_ota_rx_cache + g_ota_rx_len, read_buffer, read_len);
        g_ota_rx_len += read_len;
    }

    while (g_ota_rx_len >= (SMOTA_FRAME_HEADER_SIZE + 2U)) {
        struct smota_frame_header *header;
        struct smota_frame frame;
        smota_err_t parse_ret;
        int sof_offset;
        uint32_t frame_len;

        header = (struct smota_frame_header *)g_ota_rx_cache;
        sof_offset = smota_find_sof(g_ota_rx_cache, (uint16_t)g_ota_rx_len);
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

/*---------- end of file ----------*/
