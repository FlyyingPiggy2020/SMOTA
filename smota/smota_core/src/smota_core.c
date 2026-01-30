/*
 * Copyright (c) 2026 by Lu Xianfan.
 * @FilePath     : smota_core.c
 * @Author       : lxf
 * @Date         : 2026-01-29 09:57:46
 * @LastEditors  : lxf_zjnb@qq.com
 * @LastEditTime : 2026-01-30 11:00:00
 * @Brief        : smOTA 核心 API 实现
 */

/*---------- includes ----------*/
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "smota_packet.h"
#include "smota_state.h"
#include "smota_types.h"
#include "smota_config.h"
#include "../smota_hal/smota_hal.h"

/*---------- macro ----------*/
#define SMOTA_RECV_BUFFER_SIZE    1024    /* 接收缓冲区大小 */

/*---------- type define ----------*/

/*---------- variable prototype ----------*/

/*---------- function prototype ----------*/

/*---------- variable ----------*/
/**
 * @brief  HAL 实例
 */
static const struct smota_hal *g_hal = NULL;

/**
 * @brief  OTA 状态码
 */
static smota_err_t g_last_error = SMOTA_ERR_OK;

/**
 * @brief  接收缓冲区
 */
static uint8_t g_recv_buffer[SMOTA_RECV_BUFFER_SIZE];

/**
 * @brief  OTA 是否已初始化
 */
static bool g_initialized = false;

/*---------- function ----------*/

/**
 * @brief       初始化 OTA 模块
 * @return      smota_err_t 错误码
 */
smota_err_t smota_init(void)
{
    struct smota_ctx *ctx;

    /* 防止重复初始化 */
    if (g_initialized) {
        return SMOTA_ERR_OK;
    }

    /* 检查 HAL 是否已注册 */
    g_hal = smota_hal_get();
    if (g_hal == NULL) {
        g_last_error = SMOTA_ERR_INVALID_STATE;
        return g_last_error;
    }

    /* 初始化上下文 */
    ctx = smota_ctx_get();
    ctx->state = SMOTA_STATE_IDLE;
    ctx->firmware_size = 0;
    ctx->received_size = 0;
    ctx->flash_addr = 0;
    ctx->timeout_ms = 5000;  /* 默认 5 秒超时 */
    ctx->recv_buffer = g_recv_buffer;
    ctx->recv_len = 0;
    ctx->last_packet_time = 0;
    ctx->retry_count = 0;

    /* 重置状态机 */
    smota_state_reset();

    g_initialized = true;
    g_last_error = SMOTA_ERR_OK;

    return SMOTA_ERR_OK;
}

/**
 * @brief       去初始化 OTA 模块
 * @return      smota_err_t 错误码
 */
smota_err_t smota_deinit(void)
{
    if (!g_initialized) {
        return SMOTA_ERR_OK;
    }

    /* 重置状态机 */
    smota_state_reset();

    /* 清除缓冲区 */
    memset(g_recv_buffer, 0, sizeof(g_recv_buffer));

    g_initialized = false;
    g_last_error = SMOTA_ERR_OK;

    return SMOTA_ERR_OK;
}

/**
 * @brief       主轮询函数
 * @return      smota_err_t 错误码
 * @note        需在主循环中每 1-10ms 调用一次
 */
smota_err_t smota_poll(void)
{
    struct smota_ctx *ctx;
    const struct smota_system_driver *system;
    struct smota_frame frame;
    struct smota_handshake_resp handshake_resp;
    struct smota_header_info_resp header_resp;
    struct smota_data_block_resp data_resp;
    struct smota_transfer_complete_resp complete_resp;
    struct smota_install_resp install_resp;
    struct smota_activate_check_resp activate_resp;
    uint8_t resp_buffer[256];
    int resp_len;
    int recv_len;
    smota_err_t ret;
    uint64_t current_time;

    /* 检查初始化状态 */
    if (!g_initialized) {
        return SMOTA_ERR_INVALID_STATE;
    }

    ctx = smota_ctx_get();
    system = (g_hal != NULL) ? g_hal->system : NULL;

    /* 获取当前时间 */
    if (system != NULL && system->get_tick_ms != NULL) {
        current_time = system->get_tick_ms();
    } else {
        current_time = 0;
    }

    /* 检查超时 */
    if (ctx->last_packet_time > 0 && ctx->timeout_ms > 0) {
        if ((current_time - ctx->last_packet_time) > ctx->timeout_ms) {
            g_last_error = SMOTA_ERR_TIMEOUT;
            smota_state_set(SMOTA_STATE_ERROR);
            return SMOTA_ERR_TIMEOUT;
        }
    }

    /* 尝试接收数据 */
    if (g_hal != NULL && g_hal->comm != NULL && g_hal->comm->receive != NULL) {
        recv_len = g_hal->comm->receive(g_recv_buffer + ctx->recv_len,
                                         SMOTA_RECV_BUFFER_SIZE - ctx->recv_len,
                                         0);  /* 非阻塞 */
        if (recv_len > 0) {
            ctx->recv_len += recv_len;
            ctx->last_packet_time = current_time;

            /* 检查是否收到完整帧 (最小帧长度) */
            if (ctx->recv_len >= 12) {  /* header(10) + length(2) */
                /* 解析帧 */
                ret = smota_frame_parse(g_recv_buffer, ctx->recv_len, &frame);
                if (ret == 0) {
                    /* 处理命令 */
                    switch (frame.header.cmd) {
                    case SMOTA_CMD_HANDSHAKE:
                        ret = smota_handle_handshake_req(
                            (struct smota_handshake_req *)frame.payload,
                            &handshake_resp);
                        if (ret == SMOTA_ERR_OK) {
                            resp_len = smota_frame_build(
                                SMOTA_CMD_HANDSHAKE_RESP,
                                (uint8_t *)&handshake_resp,
                                sizeof(handshake_resp),
                                resp_buffer, sizeof(resp_buffer));
                            if (resp_len > 0 && g_hal->comm->send != NULL) {
                                g_hal->comm->send(resp_buffer, resp_len);
                            }
                        }
                        break;

                    case SMOTA_CMD_HEADER_INFO:
                        ret = smota_handle_header_info_req(
                            (struct smota_header_info_req *)frame.payload,
                            &header_resp);
                        if (ret == SMOTA_ERR_OK) {
                            resp_len = smota_frame_build(
                                SMOTA_CMD_HEADER_INFO_RESP,
                                (uint8_t *)&header_resp,
                                sizeof(header_resp),
                                resp_buffer, sizeof(resp_buffer));
                            if (resp_len > 0 && g_hal->comm->send != NULL) {
                                g_hal->comm->send(resp_buffer, resp_len);
                            }
                        }
                        break;

                    case SMOTA_CMD_DATA_BLOCK:
                        ret = smota_handle_data_block_req(
                            (struct smota_data_block_req *)frame.payload,
                            frame.payload + sizeof(struct smota_data_block_req),
                            &data_resp);
                        if (ret == SMOTA_ERR_OK) {
                            resp_len = smota_frame_build(
                                SMOTA_CMD_DATA_BLOCK_RESP,
                                (uint8_t *)&data_resp,
                                sizeof(data_resp),
                                resp_buffer, sizeof(resp_buffer));
                            if (resp_len > 0 && g_hal->comm->send != NULL) {
                                g_hal->comm->send(resp_buffer, resp_len);
                            }
                        }
                        break;

                    case SMOTA_CMD_DATA_COMPLETE:
                        ret = smota_handle_transfer_complete_req(
                            (struct smota_transfer_complete_req *)frame.payload,
                            &complete_resp);
                        if (ret == SMOTA_ERR_OK) {
                            resp_len = smota_frame_build(
                                SMOTA_CMD_DATA_COMPLETE_RESP,
                                (uint8_t *)&complete_resp,
                                sizeof(complete_resp),
                                resp_buffer, sizeof(resp_buffer));
                            if (resp_len > 0 && g_hal->comm->send != NULL) {
                                g_hal->comm->send(resp_buffer, resp_len);
                            }
                        }
                        break;

                    case SMOTA_CMD_INSTALL:
                        ret = smota_handle_install_req(
                            (struct smota_install_req *)frame.payload,
                            &install_resp);
                        if (ret == SMOTA_ERR_OK) {
                            resp_len = smota_frame_build(
                                SMOTA_CMD_INSTALL_RESP,
                                (uint8_t *)&install_resp,
                                sizeof(install_resp),
                                resp_buffer, sizeof(resp_buffer));
                            if (resp_len > 0 && g_hal->comm->send != NULL) {
                                g_hal->comm->send(resp_buffer, resp_len);
                            }
                        }
                        break;

                    case SMOTA_CMD_ACTIVATE_CHECK:
                        ret = smota_handle_activate_check_req(
                            (struct smota_activate_check_req *)frame.payload,
                            &activate_resp);
                        if (ret == SMOTA_ERR_OK) {
                            resp_len = smota_frame_build(
                                SMOTA_CMD_ACTIVATE_CHECK_RESP,
                                (uint8_t *)&activate_resp,
                                sizeof(activate_resp),
                                resp_buffer, sizeof(resp_buffer));
                            if (resp_len > 0 && g_hal->comm->send != NULL) {
                                g_hal->comm->send(resp_buffer, resp_len);
                            }
                        }
                        break;

                    default:
                        /* 未知命令 */
                        break;
                    }

                    /* 更新最后错误码 */
                    if (ret != SMOTA_ERR_OK) {
                        g_last_error = ret;
                    }

                    /* 移动缓冲区 */
                    ctx->recv_len -= (frame.header.length + sizeof(struct smota_frame_header) + sizeof(uint16_t));
                    if (ctx->recv_len > 0) {
                        memmove(g_recv_buffer,
                                g_recv_buffer + frame.header.length + sizeof(struct smota_frame_header) + sizeof(uint16_t),
                                ctx->recv_len);
                    }
                } else if (ret == -5) {
                    /* 帧不完整，继续接收 */
                } else {
                    /* 帧解析错误，丢弃缓冲区 */
                    ctx->recv_len = 0;
                }
            }
        }
    }

    return SMOTA_ERR_OK;
}

/**
 * @brief       启动 OTA（主动触发）
 * @return      smota_err_t 错误码
 */
smota_err_t smota_start(void)
{
    if (!g_initialized) {
        return SMOTA_ERR_INVALID_STATE;
    }

    /* 重置状态机 */
    smota_state_reset();

    return SMOTA_ERR_OK;
}

/**
 * @brief       中止 OTA
 * @return      smota_err_t 错误码
 */
smota_err_t smota_abort(void)
{
    if (!g_initialized) {
        return SMOTA_ERR_INVALID_STATE;
    }

    /* 重置状态机 */
    smota_state_reset();

    /* 清除错误码 */
    g_last_error = SMOTA_ERR_OK;

    return SMOTA_ERR_OK;
}

/**
 * @brief       获取 OTA 进度
 * @return      uint8_t 进度百分比 (0-100)
 */
uint8_t smota_get_progress(void)
{
    struct smota_ctx *ctx = smota_ctx_get();

    if (ctx->firmware_size == 0) {
        return 0;
    }

    return (uint8_t)((ctx->received_size * 100) / ctx->firmware_size);
}

/**
 * @brief       获取当前状态
 * @return      smota_state_t 当前状态
 */
smota_state_t smota_get_state(void)
{
    return smota_state_get();
}

/**
 * @brief       获取最后错误码
 * @return      smota_err_t 最后错误码
 */
smota_err_t smota_get_error(void)
{
    return g_last_error;
}

/**
 * @brief       获取最后错误的字符串描述
 * @return      const char* 错误描述
 */
const char *smota_get_error_string(void)
{
    return smota_err_to_string(g_last_error);
}

/**
 * @brief       检查 OTA 是否正在运行
 * @return      bool true=运行中, false=空闲
 */
bool smota_is_running(void)
{
    smota_state_t state = smota_state_get();
    return (state != SMOTA_STATE_IDLE && state != SMOTA_STATE_ERROR);
}

/*---------- end of file ----------*/
