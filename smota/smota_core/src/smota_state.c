/*
 * Copyright (c) 2026 by Lu Xianfan.
 * @FilePath     : smota_state.c
 * @Author       : lxf
 * @Date         : 2026-01-29 09:57:46
 * @LastEditors  : lxf_zjnb@qq.com
 * @LastEditTime : 2026-01-30 10:30:00
 * @Brief        : smOTA 状态机实现
 */

/*---------- includes ----------*/
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../inc/smota_types.h"
#include "../inc/smota_state.h"

/*---------- macro ----------*/

/*---------- type define ----------*/

/*---------- variable prototype ----------*/

/*---------- function prototype ----------*/

/*---------- variable ----------*/
/**
 * @brief  OTA 上下文实例
 * @note   全局唯一实例，由 smota_init 初始化
 */
static struct smota_ctx g_smota_ctx = {
    .state = SMOTA_STATE_IDLE,
    .firmware_size = 0,
    .received_size = 0,
    .firmware_version = {0},
    .flash_addr = 0,
    .timeout_ms = 0,
    .recv_buffer = NULL,
    .recv_len = 0,
    .last_packet_time = 0,
    .retry_count = 0,
};

/**
 * @brief  状态字符串映射表
 */
static const char *g_state_string[] = {
    "IDLE",
    "HANDSHAKE",
    "HEADER_INFO",
    "TRANSFER",
    "COMPLETE",
    "INSTALL",
    "ACTIVATE",
    "ERROR",
};

/**
 * @brief  有效的状态转换映射
 * @note   from -> to，允许的状态转换为 true
 */
static const bool g_state_transition[SMOTA_STATE_MAX][SMOTA_STATE_MAX] = {
    /* to:   IDLE  HANDS  HEAD  TRANS COMP INST ACT  ERR */
    /* IDLE   */ {false, true,  false, false, false, false, false, true},
    /* HANDS  */ {false, false, true,  false, false, false, false, true},
    /* HEAD   */ {false, false, false, true,  false, false, false, true},
    /* TRANS  */ {false, false, false, false, true,  false, false, true},
    /* COMP   */ {false, false, false, false, false, true,  false, true},
    /* INST   */ {false, false, false, false, false, false, true,  true},
    /* ACT    */ {true,  false, false, false, false, false, false, true},
    /* ERR    */ {true,  false, false, false, false, false, false, false},
};

/*---------- function ----------*/

/**
 * @brief       获取当前 OTA 状态
 * @return      smota_state_t 当前状态
 */
smota_state_t smota_state_get(void)
{
    return g_smota_ctx.state;
}

/**
 * @brief       设置 OTA 状态
 * @param[in]   state   目标状态
 * @return      smota_err_t 错误码
 */
smota_err_t smota_state_set(smota_state_t state)
{
    smota_state_t from = g_smota_ctx.state;

    /* 参数检查 */
    if (state >= SMOTA_STATE_MAX) {
        return SMOTA_ERR_INVALID_PARAM;
    }

    /* 检查状态转换是否有效 */
    if (!g_state_transition[from][state]) {
        g_smota_ctx.state = SMOTA_STATE_ERROR;
        return SMOTA_ERR_INVALID_STATE;
    }

    /* 设置新状态 */
    g_smota_ctx.state = state;

    return SMOTA_ERR_OK;
}

/**
 * @brief       获取状态对应的字符串描述
 * @param[in]   state   状态枚举值
 * @return      const char* 状态描述字符串
 */
const char *smota_state_to_string(smota_state_t state)
{
    if (state >= SMOTA_STATE_MAX) {
        return "UNKNOWN";
    }
    return g_state_string[state];
}

/**
 * @brief       检查状态转换是否有效
 * @param[in]   from    源状态
 * @param[in]   to      目标状态
 * @return      bool true=有效, false=无效
 */
bool smota_state_transition_is_valid(smota_state_t from, smota_state_t to)
{
    if (from >= SMOTA_STATE_MAX || to >= SMOTA_STATE_MAX) {
        return false;
    }
    return g_state_transition[from][to];
}

/**
 * @brief       重置状态机到空闲状态
 */
void smota_state_reset(void)
{
    g_smota_ctx.state = SMOTA_STATE_IDLE;
    g_smota_ctx.firmware_size = 0;
    g_smota_ctx.received_size = 0;
    g_smota_ctx.recv_len = 0;
    g_smota_ctx.retry_count = 0;
}

/**
 * @brief       获取 OTA 上下文指针
 * @return      struct smota_ctx* 上下文指针
 */
struct smota_ctx *smota_ctx_get(void)
{
    return &g_smota_ctx;
}

/*---------- end of file ----------*/
