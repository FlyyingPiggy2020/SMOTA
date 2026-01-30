/*
 * Copyright (c) 2026 by Lu Xianfan.
 * @FilePath     : smota_state.h
 * @Author       : lxf
 * @Date         : 2026-01-29 09:57:46
 * @LastEditors  : lxf_zjnb@qq.com
 * @LastEditTime : 2026-01-29 09:57:46
 * @Brief        : smOTA 状态机定义
 */

#ifndef SMOTA_STATE_H
#define SMOTA_STATE_H

#ifdef __cplusplus
extern "C" {
#endif

/*---------- includes ----------*/

/*---------- macro ----------*/

/*---------- includes ----------*/
#include <stdbool.h>
#include "smota_types.h"

/*---------- macro ----------*/

/*---------- type define ----------*/

/*---------- variable prototype ----------*/

/*---------- function prototype ----------*/

/**
 * @brief       获取当前 OTA 状态
 * @return      smota_state_t 当前状态
 */
smota_state_t smota_state_get(void);

/**
 * @brief       设置 OTA 状态
 * @param[in]   state   目标状态
 * @return      smota_err_t 错误码
 */
smota_err_t smota_state_set(smota_state_t state);

/**
 * @brief       获取状态对应的字符串描述
 * @param[in]   state   状态枚举值
 * @return      const char* 状态描述字符串
 */
const char *smota_state_to_string(smota_state_t state);

/**
 * @brief       检查状态转换是否有效
 * @param[in]   from    源状态
 * @param[in]   to      目标状态
 * @return      bool true=有效, false=无效
 */
bool smota_state_transition_is_valid(smota_state_t from, smota_state_t to);

/**
 * @brief       重置状态机到空闲状态
 */
void smota_state_reset(void);

/**
 * @brief       获取 OTA 上下文指针
 * @return      struct smota_ctx* 上下文指针
 */
struct smota_ctx *smota_ctx_get(void);

/*---------- end of file ----------*/

#ifdef __cplusplus
}
#endif

#endif // SMOTA_STATE_H
