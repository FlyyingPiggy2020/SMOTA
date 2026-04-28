/*
 * Copyright (c) 2025 by Lu Xianfan.
 * @FilePath     : main_app.c
 * @Author       : lxf
 * @Date         : 2026-04-27 10:00:00
 * @LastEditors  : lxf_zjnb@qq.com
 * @LastEditTime : 2026-04-28 15:45:59
 * @Brief        : Boot Demo 主循环任务实现
 */

/*---------- includes ----------*/
#include "main.h"
#include "app/main_app.h"
#include "smota.h"
#include "smota_port.h"

/*---------- macro ----------*/
#define MAIN_APP_POLL_DELAY_MS 1U
#define MAIN_APP_OTA_WINDOW_MS 3000U

/*---------- type define ----------*/

/*---------- variable prototype ----------*/

/*---------- function prototype ----------*/

/*---------- variable ----------*/
static uint32_t g_boot_window_start_ms = 0U;

/*---------- function ----------*/
void main_app_init(void)
{
    if (smota_port_init() != 0) {
        return;
    }

    if (smota_init() != SMOTA_ERR_OK) {
        (void)smota_port_deinit();
        return;
    }

    g_boot_window_start_ms = HAL_GetTick();
}

void main_app_poll(void)
{
    struct smota_ctx *ctx;
    (void)smota_poll();
    ctx = smota_ctx_get();

    do {
        if (ctx == NULL) {
            break;
        }
        if (ctx->should_stay_in_boot) {
            break;
        }

        if ((HAL_GetTick() - g_boot_window_start_ms) >= MAIN_APP_OTA_WINDOW_MS) {
            if (smota_port_jump_to_app() != 0) {
                ctx->should_stay_in_boot = 1U;
            }
            break;
        }
    } while (0);

    HAL_Delay(MAIN_APP_POLL_DELAY_MS);
}
/*---------- end of file ----------*/
