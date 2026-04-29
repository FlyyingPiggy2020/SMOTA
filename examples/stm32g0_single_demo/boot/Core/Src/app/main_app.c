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

/*---------- type define ----------*/

/*---------- variable prototype ----------*/

/*---------- function prototype ----------*/

/*---------- variable ----------*/

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
}

void main_app_poll(void)
{
    (void)smota_poll();
    HAL_Delay(MAIN_APP_POLL_DELAY_MS);
}
/*---------- end of file ----------*/
