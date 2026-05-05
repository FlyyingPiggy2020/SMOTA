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
#include "smota_internal.h"

/*---------- macro ----------*/
#define SMOTA_RECV_BUFFER_SIZE 1024 /* 接收缓冲区大小 */

/*---------- type define ----------*/

/*---------- variable prototype ----------*/

/*---------- function prototype ----------*/
static bool smota_cmd_is_stateless(uint8_t cmd);
static bool smota_state_has_active_session(smota_state_t state);
static bool smota_frame_payload_is_valid(const struct smota_frame *frame);
static void smota_session_reset_after_error(void);
static void smota_load_current_firmware_info(struct smota_ctx *ctx);
static uint64_t smota_get_current_time_ms(const struct smota_system_driver *system);
static void smota_boot_poll_jump(struct smota_ctx *ctx, const struct smota_boot_driver *boot, uint64_t current_time);

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
static uint8_t g_rx_buffer[SMOTA_RECV_BUFFER_SIZE];

/**
 * @brief  OTA 是否已初始化
 */
static bool g_initialized = false;

/*---------- function ----------*/

/**
 * @brief  通过身份驱动加载当前固件信息
 * @param  ctx: OTA 上下文
 */
static void smota_load_current_firmware_info(struct smota_ctx *ctx)
{
    struct smota_firmware_info info;

    if (ctx == NULL) {
        return;
    }

    memset(&ctx->current_info, 0, sizeof(ctx->current_info));

    if (g_hal == NULL ||
        g_hal->identity == NULL) {
        return;
    }

    memset(&info, 0, sizeof(info));
    if (g_hal->identity->get_default_info != NULL &&
        g_hal->identity->get_default_info(&info) == 0) {
        memcpy(&ctx->current_info, &info, sizeof(ctx->current_info));
    }

    memset(&info, 0, sizeof(info));
    if (g_hal->identity->get_running_info != NULL &&
        g_hal->identity->get_running_info(&info) == 0) {
        memcpy(&ctx->current_info, &info, sizeof(ctx->current_info));
    }
}

/**
 * @brief  判断命令是否为无状态查询命令
 * @param  cmd: 命令码
 * @return true=无状态, false=有状态
 */
static bool smota_cmd_is_stateless(uint8_t cmd)
{
    return (cmd == SMOTA_CMD_QUERY);
}

/**
 * @brief  判断当前状态是否处于 OTA 会话中
 * @param  state: 当前状态
 * @return true=会话进行中, false=非会话态
 */
static bool smota_state_has_active_session(smota_state_t state)
{
    return (state == SMOTA_STATE_STARTED ||
            state == SMOTA_STATE_TRANSFER ||
            state == SMOTA_STATE_FINISHED);
}

/**
 * @brief  检查命令 Payload 长度是否满足结构体读取要求
 * @param  frame: 已解析帧
 * @return true=有效, false=无效
 */
static bool smota_frame_payload_is_valid(const struct smota_frame *frame)
{
    const struct smota_data_req *data_req;

    if (frame == NULL) {
        return false;
    }

    switch (frame->header.cmd) {
        case SMOTA_CMD_QUERY:
            return (frame->header.length == sizeof(struct smota_query_req));

        case SMOTA_CMD_START:
            return (frame->header.length == sizeof(struct smota_start_req));

        case SMOTA_CMD_DATA:
            if (frame->header.length < sizeof(struct smota_data_req)) {
                return false;
            }
            data_req = (const struct smota_data_req *)frame->payload;
            return (frame->header.length ==
                    (uint16_t)(sizeof(struct smota_data_req) + data_req->length));

        case SMOTA_CMD_FINISH:
            return (frame->header.length == sizeof(struct smota_finish_req));

        default:
            return true;
    }
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
 * @brief  获取当前系统时间
 * @param  system: system driver
 * @return 当前毫秒时间，无法获取时返回 0
 */
static uint64_t smota_get_current_time_ms(const struct smota_system_driver *system)
{
    if (system != NULL && system->get_tick_ms != NULL) {
        return system->get_tick_ms();
    }

    return 0U;
}

/**
 * @brief  执行 Boot 停留与 App 跳转决策
 * @param  ctx: OTA 上下文
 * @param  boot: boot driver
 * @param  current_time: 当前毫秒时间
 */
static void smota_boot_poll_jump(struct smota_ctx *ctx, const struct smota_boot_driver *boot, uint64_t current_time)
{
    if (ctx == NULL ||
        boot == NULL) {
        return;
    }

    if (ctx->should_stay_in_boot == 0U &&
        boot->should_stay_in_boot != NULL &&
        boot->should_stay_in_boot() != 0) {
        ctx->should_stay_in_boot = 1U;
    }

    if (ctx->should_stay_in_boot != 0U ||
        boot->jump_to_app == NULL) {
        return;
    }

    if ((current_time - ctx->boot_window_start_time) < SMOTA_BOOT_CAPTURE_WINDOW_MS) {
        return;
    }

    if (boot->jump_to_app() != 0) {
        g_last_error = SMOTA_ERR_INVALID_STATE;
    }

    ctx->should_stay_in_boot = 1U;
}

/**
 * @brief       初始化 OTA 模块
 * @return      smota_err_t 错误码
 */
smota_err_t smota_init(void)
{
    struct smota_ctx *ctx;
    const struct smota_system_driver *system;
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
    ctx->timeout_ms = 5000; /* 默认 5 秒超时 */
    ctx->recv_len = 0;
    ctx->last_packet_time = 0;
    system = (g_hal != NULL) ? g_hal->system : NULL;
    ctx->boot_window_start_time = smota_get_current_time_ms(system);
    ctx->reset_pending = 0;
    ctx->should_stay_in_boot = 0;
    ctx->sync_error_count = 0;
    ctx->query_allowed = 0;
    memset(ctx->target_project_id, 0, sizeof(ctx->target_project_id));
    memset(ctx->expected_hash, 0, sizeof(ctx->expected_hash));
    ctx->verify_hash = 0;
    smota_load_current_firmware_info(ctx);

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
    memset(g_rx_buffer, 0, sizeof(g_rx_buffer));

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
    const struct smota_boot_driver *boot;
    smota_state_t state;
    struct smota_frame frame;
    struct smota_query_resp query_resp = {0};
    struct smota_start_resp start_resp = {0};
    struct smota_data_resp data_resp = {0};
    struct smota_finish_resp finish_resp = {0};
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
    boot = (g_hal != NULL) ? g_hal->boot : NULL;

    state = smota_state_get();
    if (state == SMOTA_STATE_ERROR) {
        smota_session_reset_after_error();
        state = smota_state_get();
    }

    /* 获取当前时间 */
    current_time = smota_get_current_time_ms(system);

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
        recv_len = g_hal->comm->receive(g_rx_buffer + ctx->recv_len,
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
                ret = smota_frame_parse(g_rx_buffer, ctx->recv_len, &frame);

                if (ret == SMOTA_ERR_OK) {
                    if (!smota_frame_payload_is_valid(&frame)) {
                        ret = SMOTA_ERR_LENGTH;
                        g_last_error = ret;
                        consumed = sizeof(struct smota_frame_header) + frame.header.length + sizeof(uint16_t);
                        ctx->recv_len -= consumed;
                        if (ctx->recv_len > 0) {
                            memmove(g_rx_buffer,
                                    g_rx_buffer + consumed,
                                    ctx->recv_len);
                        }
                        continue;
                    }

                    /* 处理命令 */
                    switch (frame.header.cmd) {
                        case SMOTA_CMD_QUERY:
                            ret = smota_handle_query_req(
                                (struct smota_query_req *)frame.payload,
                                &query_resp);
                            resp_len = smota_frame_build(
                                SMOTA_CMD_QUERY_RESP,
                                (uint8_t *)&query_resp,
                                sizeof(query_resp),
                                resp_buffer,
                                sizeof(resp_buffer));
                            if (resp_len > 0 && g_hal->comm->send != NULL) {
                                g_hal->comm->send(resp_buffer, resp_len);
                            }
                            break;

                        case SMOTA_CMD_START:
                            ret = smota_handle_start_req(
                                (struct smota_start_req *)frame.payload,
                                &start_resp);
                            resp_len = smota_frame_build(
                                SMOTA_CMD_START_RESP,
                                (uint8_t *)&start_resp,
                                sizeof(start_resp),
                                resp_buffer,
                                sizeof(resp_buffer));
                            if (resp_len > 0 && g_hal->comm->send != NULL) {
                                g_hal->comm->send(resp_buffer, resp_len);
                            }
                            break;

                        case SMOTA_CMD_DATA:
                            ret = smota_handle_data_req(
                                (struct smota_data_req *)frame.payload,
                                frame.payload + sizeof(struct smota_data_req),
                                &data_resp);
                            resp_len = smota_frame_build(
                                SMOTA_CMD_DATA_RESP,
                                (uint8_t *)&data_resp,
                                sizeof(data_resp),
                                resp_buffer,
                                sizeof(resp_buffer));
                            if (resp_len > 0 && g_hal->comm->send != NULL) {
                                g_hal->comm->send(resp_buffer, resp_len);
                            }
                            break;

                        case SMOTA_CMD_FINISH:
                            ret = smota_handle_finish_req(
                                (struct smota_finish_req *)frame.payload,
                                &finish_resp);
                            resp_len = smota_frame_build(
                                SMOTA_CMD_FINISH_RESP,
                                (uint8_t *)&finish_resp,
                                sizeof(finish_resp),
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
                        memmove(g_rx_buffer,
                                g_rx_buffer + consumed,
                                ctx->recv_len);
                    }

                } else if (ret == SMOTA_ERR_LENGTH) {
                    /* 帧不完整，检查是否为异常长度值 */
                    struct smota_frame_header *hdr = (struct smota_frame_header *)g_rx_buffer;
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
                        sof_offset = smota_find_sof(g_rx_buffer + 1, ctx->recv_len - 1);
                        if (sof_offset >= 0) {
                            /* 找到下一个 SOF（sof_offset 是相对偏移+1） */
                            ctx->recv_len -= (sof_offset + 1);
                            if (ctx->recv_len > 0) {
                                memmove(g_rx_buffer, g_rx_buffer + sof_offset + 1, ctx->recv_len);
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
                    sof_offset = smota_find_sof(g_rx_buffer, ctx->recv_len);
                    if (sof_offset > 0) {
                        /* 找到下一个 SOF，丢弃前面的无效数据 */
                        ctx->recv_len -= sof_offset;
                        if (ctx->recv_len > 0) {
                            memmove(g_rx_buffer, g_rx_buffer + sof_offset, ctx->recv_len);
                        }
                        /* 继续循环尝试解析下一帧 */
                        continue;
                    } else if (sof_offset == 0) {
                        /* 当前位置就是 SOF，但解析失败（可能是 CRC 错误或其他问题）
                         * 跳过当前字节，继续搜索 */
                        ctx->recv_len -= 1;
                        if (ctx->recv_len > 0) {
                            memmove(g_rx_buffer, g_rx_buffer + 1, ctx->recv_len);
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

    current_time = smota_get_current_time_ms(system);
    smota_boot_poll_jump(ctx, boot, current_time);

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

void smota_get_current_firmware_info(struct smota_firmware_info *info)
{
    struct smota_ctx *ctx;

    if (info == NULL) {
        return;
    }

    ctx = smota_ctx_get();
    memcpy(info, &ctx->current_info, sizeof(*info));
}

void smota_get_target_version(uint8_t version[4])
{
    struct smota_ctx *ctx;

    if (version == NULL) {
        return;
    }

    ctx = smota_ctx_get();
    memcpy(version, ctx->firmware_version, sizeof(ctx->firmware_version));
}

bool smota_should_stay_in_boot(void)
{
    struct smota_ctx *ctx = smota_ctx_get();

    return (ctx->should_stay_in_boot != 0U) ? true : false;
}

void smota_set_stay_in_boot(bool stay)
{
    struct smota_ctx *ctx = smota_ctx_get();

    ctx->should_stay_in_boot = (stay == true) ? 1U : 0U;
}

/*---------- end of file ----------*/
