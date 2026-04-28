/*
 * Copyright (c) 2025 by Lu Xianfan.
 * @FilePath     : main_app.c
 * @Author       : lxf
 * @Date         : 2026-04-27 10:00:00
 * @LastEditors  : lxf_zjnb@qq.com
 * @LastEditTime : 2026-04-27 10:00:00
 * @Brief        : App Demo 主循环任务实现
 */

/*---------- includes ----------*/
#include <string.h>
#include "main.h"
#include "app/main_app.h"
#include "usbd_cdc_if.h"
#include "smota_app_info.h"
#include "smota_boot.h"

/*---------- macro ----------*/
#define MAIN_APP_POLL_DELAY_MS 10U
#define MAIN_APP_LOG_PERIOD_MS 1000U

/*---------- type define ----------*/

/*---------- variable prototype ----------*/

/*---------- function prototype ----------*/
static void app_try_log_hello_world(void);

/*---------- variable ----------*/
SMOTA_APP_INFO_DEFINE();

__attribute__((used, section(".smota_boot_request")))
static volatile struct smota_boot_request g_smota_boot_request_area;

static uint32_t g_last_log_tick_ms = 0U;
static uint8_t g_hello_message[] = "hello world\r\n";

/*---------- function ----------*/
void main_app_init(void)
{
    g_last_log_tick_ms = HAL_GetTick();
}

void main_app_poll(void)
{
    if ((HAL_GetTick() - g_last_log_tick_ms) >= MAIN_APP_LOG_PERIOD_MS) {
        g_last_log_tick_ms = HAL_GetTick();
        app_try_log_hello_world();
    }

    HAL_Delay(MAIN_APP_POLL_DELAY_MS);
}

void smota_request_bootloader_and_reset(void)
{
    smota_boot_request_set();
    NVIC_SystemReset();
}

static void app_try_log_hello_world(void)
{
    if (CDC_Transmit_FS(g_hello_message, (uint16_t)strlen((const char *)g_hello_message)) == USBD_BUSY) {
        return;
    }
}
/*---------- end of file ----------*/
