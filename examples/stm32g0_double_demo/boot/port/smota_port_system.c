/*
 * Copyright (c) 2025 by Lu Xianfan.
 * @FilePath     : smota_port_system.c
 * @Author       : lxf
 * @Date         : 2026-04-28 16:10:00
 * @LastEditors  : lxf_zjnb@qq.com
 * @LastEditTime : 2026-04-28 18:00:00
 * @Brief        : STM32G0 平台 smOTA 系统驱动实现
 */

/*---------- includes ----------*/
#include "smota_port_internal.h"

/*---------- macro ----------*/

/*---------- type define ----------*/

/*---------- variable prototype ----------*/

/*---------- function prototype ----------*/
static uint64_t system_get_tick_ms(void);
static void system_reset(void);

/*---------- variable ----------*/
struct smota_system_driver g_smota_system_driver = {
    .get_tick_ms = system_get_tick_ms,
    .system_reset = system_reset,
};

/*---------- function ----------*/
/**
 * @brief  获取系统毫秒节拍
 * @return 毫秒计数
 */
static uint64_t system_get_tick_ms(void)
{
    return (uint64_t)HAL_GetTick();
}

/**
 * @brief  复位系统
 */
static void system_reset(void)
{
    (void)smota_port_flash_flush_staged_write();

    HAL_Delay(20);
    NVIC_SystemReset();
}

/*---------- end of file ----------*/
