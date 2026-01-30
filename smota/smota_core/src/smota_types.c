/*
 * Copyright (c) 2026 by Lu Xianfan.
 * @FilePath     : smota_types.c
 * @Author       : lxf
 * @Date         : 2026-01-30 10:30:00
 * @LastEditors  : lxf_zjnb@qq.com
 * @LastEditTime : 2026-01-30 10:30:00
 * @Brief        : smOTA 类型定义实现
 */

/*---------- includes ----------*/
#include "../inc/smota_types.h"

/*---------- macro ----------*/

/*---------- type define ----------*/

/*---------- variable prototype ----------*/

/*---------- function prototype ----------*/

/*---------- variable ----------*/
/**
 * @brief  错误码字符串映射表
 */
static const char *g_err_string[] = {
    "OK",
    "INVALID_STATE",
    "INVALID_PARAM",
    "TIMEOUT",
    "CRC",
    "VERSION",
    "SPACE",
    "FLASH",
    "SIGNATURE",
    "NOT_SUPPORTED",
    "BUSY",
};

/*---------- function ----------*/

/**
 * @brief       获取错误码对应的字符串描述
 * @param[in]   err     错误码
 * @return      const char* 错误描述字符串
 */
const char *smota_err_to_string(smota_err_t err)
{
    if (err >= SMOTA_ERR_MAX) {
        return "UNKNOWN";
    }
    return g_err_string[err];
}

/*---------- end of file ----------*/
