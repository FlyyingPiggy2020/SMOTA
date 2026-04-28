/*
 * Copyright (c) 2026 by Lu Xianfan.
 * @FilePath     : smota_core.c
 * @Author       : lxf
 * @Date         : 2026-01-29 09:57:46
 * @LastEditors  : lxf_zjnb@qq.com
 * @LastEditTime : 2026-03-06 10:26:24
 * @Brief        : smOTA 核心 API 实现
 */

/*---------- includes ----------*/
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "smota.h"

/*---------- macro ----------*/
#define SMOTA_RECV_BUFFER_SIZE 1024 /* 接收缓冲区大小 */
#define SMOTA_BOOT_PROJECT_ID "SMOTA_BOOT"

/*---------- type define ----------*/

/*---------- variable prototype ----------*/

/*---------- function prototype ----------*/
static bool smota_cmd_is_stateless(uint8_t cmd);
static bool smota_state_has_active_session(smota_state_t state);
static void smota_session_reset_after_error(void);
static void smota_load_current_app_info(struct smota_ctx *ctx);

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
 * @brief  通过系统驱动加载当前 App 信息
 * @param  ctx: OTA 上下文
 */
static void smota_load_current_app_info(struct smota_ctx *ctx)
{
    struct smota_app_info app_info;

    if (ctx == NULL) {
        return;
    }

    memset(ctx->current_version, 0, sizeof(ctx->current_version));
    memset(ctx->current_project_id, 0, sizeof(ctx->current_project_id));
    memcpy(ctx->current_project_id,
           SMOTA_BOOT_PROJECT_ID,
           sizeof(SMOTA_BOOT_PROJECT_ID) - 1U);

    if (g_hal == NULL ||
        g_hal->system == NULL ||
        g_hal->system->get_app_info == NULL) {
        return;
    }

    memset(&app_info, 0, sizeof(app_info));
    if (g_hal->system->get_app_info(&app_info) < 0) {
        return;
    }

    if (app_info.magic != SMOTA_APP_INFO_MAGIC) {
        return;
    }

    ctx->current_version[0] = app_info.fw_version_major;
    ctx->current_version[1] = app_info.fw_version_minor;
    ctx->current_version[2] = app_info.fw_version_patch;
    ctx->current_version[3] = 0U;
    memcpy(ctx->current_project_id,
           app_info.project_id,
           sizeof(ctx->current_project_id));
}

/**
 * @brief  判断命令是否为无状态查询命令
 * @param  cmd: 命令码
 * @return true=无状态, false=有状态
 */
static bool smota_cmd_is_stateless(uint8_t cmd)
{
    return (cmd == SMOTA_CMD_QUERY_VERSION);
}

/**
 * @brief  判断当前状态是否处于 OTA 会话中
 * @param  state: 当前状态
 * @return true=会话进行中, false=非会话态
 */
static bool smota_state_has_active_session(smota_state_t state)
{
    return (state == SMOTA_STATE_HANDSHAKE ||
            state == SMOTA_STATE_HEADER_INFO ||
            state == SMOTA_STATE_TRANSFER ||
            state == SMOTA_STATE_COMPLETE ||
            state == SMOTA_STATE_INSTALL ||
            state == SMOTA_STATE_ACTIVATE);
}

/**
 * @brief  会话异常后的恢复处理
 * @note   不销毁 smOTA 服务本身，只清空当前 OTA 会话
 */
static void smota_session_reset_after_error(void)
{
    smota_state_reset();
}

/**
 * @brief       初始化 OTA 模块
 * @return      smota_err_t 错误码
 */
smota_err_t smota_init(void)
{
    struct smota_ctx *ctx;
    int ret;

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

    if (g_hal->flash != NULL && g_hal->flash->init != NULL) {
        ret = g_hal->flash->init();
        if (ret < 0) {
            g_last_error = SMOTA_ERR_FLASH;
            return g_last_error;
        }
    }

    if (g_hal->comm != NULL && g_hal->comm->init != NULL) {
        ret = g_hal->comm->init();
        if (ret < 0) {
            if (g_hal->flash != NULL && g_hal->flash->deinit != NULL) {
                g_hal->flash->deinit();
            }
            g_last_error = SMOTA_ERR_INVALID_STATE;
            return g_last_error;
        }
    }

    /* 初始化上下文 */
    ctx = smota_ctx_get();
    ctx->state = SMOTA_STATE_IDLE;
    ctx->firmware_size = 0;
    ctx->received_size = 0;
    ctx->flash_addr = 0;
    ctx->timeout_ms = 5000; /* 默认 5 秒超时 */
    ctx->recv_buffer = g_recv_buffer;
    ctx->recv_len = 0;
    ctx->last_packet_time = 0;
    ctx->retry_count = 0;
    ctx->reset_pending = 0;
    ctx->should_stay_in_boot = 0;
    ctx->sync_error_count = 0;
    memset(ctx->expected_hash, 0, sizeof(ctx->expected_hash));
    memset(ctx->signature_r, 0, sizeof(ctx->signature_r));
    memset(ctx->signature_s, 0, sizeof(ctx->signature_s));
    smota_load_current_app_info(ctx);

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

    if (g_hal != NULL && g_hal->comm != NULL && g_hal->comm->deinit != NULL) {
        g_hal->comm->deinit();
    }

    if (g_hal != NULL && g_hal->flash != NULL && g_hal->flash->deinit != NULL) {
        g_hal->flash->deinit();
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
    smota_state_t state;
    struct smota_frame frame;
    struct smota_handshake_resp handshake_resp = {0};
    struct smota_header_info_resp header_resp = {0};
    struct smota_data_block_resp data_resp = {0};
    struct smota_transfer_complete_resp complete_resp = {0};
    struct smota_install_resp install_resp = {0};
    struct smota_query_version_resp query_version_resp = {0};
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
    if (system != NULL &&
        system->should_stay_in_boot != NULL &&
        system->should_stay_in_boot() != 0) {
        ctx->should_stay_in_boot = 1U;
    }

    state = smota_state_get();
    if (state == SMOTA_STATE_ERROR) {
        smota_session_reset_after_error();
        state = smota_state_get();
    }

    /* 获取当前时间 */
    if (system != NULL && system->get_tick_ms != NULL) {
        current_time = system->get_tick_ms();
    } else {
        current_time = 0;
    }

    /* 检查超时 */
    if (smota_state_has_active_session(state) &&
        ctx->last_packet_time > 0 &&
        ctx->timeout_ms > 0) {
        if ((current_time - ctx->last_packet_time) > ctx->timeout_ms) {
            g_last_error = SMOTA_ERR_TIMEOUT;
            smota_session_reset_after_error();
            return SMOTA_ERR_OK;
        }
    }

    /* 尝试接收数据 */
    if (g_hal != NULL && g_hal->comm != NULL && g_hal->comm->receive != NULL) {
        recv_len = g_hal->comm->receive(g_recv_buffer + ctx->recv_len,
                                        SMOTA_RECV_BUFFER_SIZE - ctx->recv_len,
                                        0); /* 非阻塞 */
        if (recv_len > 0) {
            ctx->recv_len += recv_len;
            ctx->last_packet_time = current_time;

            /* 循环处理缓冲区中的所有完整帧 */
            while (ctx->recv_len >= sizeof(struct smota_frame_header) + sizeof(uint16_t)) {
                uint32_t consumed; /* 已处理的帧长度 */
                int sof_offset;    /* SOF 搜索结果 */

                /* 解析帧 */
                ret = smota_frame_parse(g_recv_buffer, ctx->recv_len, &frame);

                if (ret == SMOTA_ERR_OK) {
                    /* 处理命令 */
                    switch (frame.header.cmd) {
                        case SMOTA_CMD_HANDSHAKE:
                            ret = smota_handle_handshake_req(
                                (struct smota_handshake_req *)frame.payload,
                                &handshake_resp);
                            resp_len = smota_frame_build(
                                SMOTA_CMD_HANDSHAKE_RESP,
                                (uint8_t *)&handshake_resp,
                                sizeof(handshake_resp),
                                resp_buffer,
                                sizeof(resp_buffer));
                            if (resp_len > 0 && g_hal->comm->send != NULL) {
                                g_hal->comm->send(resp_buffer, resp_len);
                            }
                            break;

                        case SMOTA_CMD_HEADER_INFO:
                            ret = smota_handle_header_info_req(
                                (struct smota_header_info_req *)frame.payload,
                                &header_resp);
                            resp_len = smota_frame_build(
                                SMOTA_CMD_HEADER_INFO_RESP,
                                (uint8_t *)&header_resp,
                                sizeof(header_resp),
                                resp_buffer,
                                sizeof(resp_buffer));
                            if (resp_len > 0 && g_hal->comm->send != NULL) {
                                g_hal->comm->send(resp_buffer, resp_len);
                            }
                            break;

                        case SMOTA_CMD_DATA_BLOCK:
                            ret = smota_handle_data_block_req(
                                (struct smota_data_block_req *)frame.payload,
                                frame.payload + sizeof(struct smota_data_block_req),
                                &data_resp);
                            resp_len = smota_frame_build(
                                SMOTA_CMD_DATA_BLOCK_RESP,
                                (uint8_t *)&data_resp,
                                sizeof(data_resp),
                                resp_buffer,
                                sizeof(resp_buffer));
                            if (resp_len > 0 && g_hal->comm->send != NULL) {
                                g_hal->comm->send(resp_buffer, resp_len);
                            }
                            break;

                        case SMOTA_CMD_DATA_COMPLETE:
                            ret = smota_handle_transfer_complete_req(
                                (struct smota_transfer_complete_req *)frame.payload,
                                &complete_resp);
                            resp_len = smota_frame_build(
                                SMOTA_CMD_DATA_COMPLETE_RESP,
                                (uint8_t *)&complete_resp,
                                sizeof(complete_resp),
                                resp_buffer,
                                sizeof(resp_buffer));
                            if (resp_len > 0 && g_hal->comm->send != NULL) {
                                g_hal->comm->send(resp_buffer, resp_len);
                            }
                            break;

                        case SMOTA_CMD_INSTALL:
                            ret = smota_handle_install_req(
                                (struct smota_install_req *)frame.payload,
                                &install_resp);
                            resp_len = smota_frame_build(
                                SMOTA_CMD_INSTALL_RESP,
                                (uint8_t *)&install_resp,
                                sizeof(install_resp),
                                resp_buffer,
                                sizeof(resp_buffer));
                            if (resp_len > 0 && g_hal->comm->send != NULL) {
                                g_hal->comm->send(resp_buffer, resp_len);
                            }
                            if (ret == SMOTA_ERR_OK &&
                                ctx->reset_pending != 0 &&
                                system != NULL &&
                                system->system_reset != NULL) {
                                ctx->reset_pending = 0;
                                system->system_reset();
                            }
                            break;

                        case SMOTA_CMD_QUERY_VERSION:
                            ret = smota_handle_query_version_req(
                                (struct smota_query_version_req *)frame.payload,
                                &query_version_resp);
                            resp_len = smota_frame_build(
                                SMOTA_CMD_QUERY_VERSION_RESP,
                                (uint8_t *)&query_version_resp,
                                sizeof(query_version_resp),
                                resp_buffer,
                                sizeof(resp_buffer));
                            if (resp_len > 0 && g_hal->comm->send != NULL) {
                                g_hal->comm->send(resp_buffer, resp_len);
                            }
                            break;

                        default:
                            /* 未知命令 */
                            break;
                    }

                    /* 更新最后错误码 */
                    if (ret != SMOTA_ERR_OK) {
                        g_last_error = ret;
                        if (!smota_cmd_is_stateless(frame.header.cmd)) {
                            smota_session_reset_after_error();
                        }
                    }

                    ctx->should_stay_in_boot = 1U;

                    /* 移动缓冲区，移除已处理的帧 */
                    consumed = sizeof(struct smota_frame_header) + frame.header.length + sizeof(uint16_t);
                    ctx->recv_len -= consumed;
                    if (ctx->recv_len > 0) {
                        memmove(g_recv_buffer,
                                g_recv_buffer + consumed,
                                ctx->recv_len);
                    }

                } else if (ret == SMOTA_ERR_LENGTH) {
                    /* 帧不完整，检查是否为异常长度值 */
                    struct smota_frame_header *hdr = (struct smota_frame_header *)g_recv_buffer;
                    uint16_t max_possible_len = SMOTA_RECV_BUFFER_SIZE - sizeof(struct smota_frame_header) - sizeof(uint16_t);

                    /* 检查长度是否异常：
                     * 1. 长度为 0
                     * 2. 长度超过协议定义的最大值
                     * 3. 长度超过接收缓冲区容量（无论如何都接收不完）
                     */
                    if (hdr->length == 0 ||
                        hdr->length > SMOTA_MAX_PAYLOAD_LEN ||
                        hdr->length > max_possible_len) {
                        /* 尝试搜索下一个 SOF 进行恢复 */
                        ctx->sync_error_count++;
                        sof_offset = smota_find_sof(g_recv_buffer + 1, ctx->recv_len - 1);
                        if (sof_offset >= 0) {
                            /* 找到下一个 SOF（sof_offset 是相对偏移+1） */
                            ctx->recv_len -= (sof_offset + 1);
                            if (ctx->recv_len > 0) {
                                memmove(g_recv_buffer, g_recv_buffer + sof_offset + 1, ctx->recv_len);
                            }
                            continue;
                        } else {
                            /* 未找到 SOF，清空缓冲区 */
                            ctx->recv_len = 0;
                            break;
                        }
                    }
                    /* 长度合理，等待更多数据 */
                    break;

                } else {
                    /* 帧解析错误，尝试搜索下一个 SOF 进行乱码恢复 */
                    ctx->sync_error_count++;
                    sof_offset = smota_find_sof(g_recv_buffer, ctx->recv_len);
                    if (sof_offset > 0) {
                        /* 找到下一个 SOF，丢弃前面的无效数据 */
                        ctx->recv_len -= sof_offset;
                        if (ctx->recv_len > 0) {
                            memmove(g_recv_buffer, g_recv_buffer + sof_offset, ctx->recv_len);
                        }
                        /* 继续循环尝试解析下一帧 */
                        continue;
                    } else if (sof_offset == 0) {
                        /* 当前位置就是 SOF，但解析失败（可能是 CRC 错误或其他问题）
                         * 跳过当前字节，继续搜索 */
                        ctx->recv_len -= 1;
                        if (ctx->recv_len > 0) {
                            memmove(g_recv_buffer, g_recv_buffer + 1, ctx->recv_len);
                        }
                        /* 继续循环尝试解析 */
                        continue;
                    } else {
                        /* 未找到任何 SOF，清空缓冲区等待新数据 */
                        ctx->recv_len = 0;
                        break;
                    }
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
