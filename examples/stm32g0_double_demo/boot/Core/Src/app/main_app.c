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
#define MAIN_APP_BOOT_LED_PORT GPIOC
#define MAIN_APP_BOOT_LED_PIN  GPIO_PIN_8
#define MAIN_APP_BOOT_LED_TOGGLE_MS 500U

/*---------- type define ----------*/

/*---------- variable prototype ----------*/

/*---------- function prototype ----------*/
static void boot_led_init(void);
static void boot_led_poll(void);

/*---------- variable ----------*/
static uint32_t g_boot_led_last_tick_ms = 0U;

/*---------- function ----------*/
void main_app_init(void)
{
    boot_led_init();

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
    boot_led_poll();
    (void)smota_poll();
}

static void boot_led_init(void)
{
    GPIO_InitTypeDef gpio_init;

    __HAL_RCC_GPIOC_CLK_ENABLE();

    HAL_GPIO_WritePin(MAIN_APP_BOOT_LED_PORT, MAIN_APP_BOOT_LED_PIN, GPIO_PIN_RESET);

    gpio_init.Pin = MAIN_APP_BOOT_LED_PIN;
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(MAIN_APP_BOOT_LED_PORT, &gpio_init);

    g_boot_led_last_tick_ms = HAL_GetTick();
}

static void boot_led_poll(void)
{
    uint32_t current_tick = HAL_GetTick();

    if ((current_tick - g_boot_led_last_tick_ms) >= MAIN_APP_BOOT_LED_TOGGLE_MS) {
        g_boot_led_last_tick_ms = current_tick;
        HAL_GPIO_TogglePin(MAIN_APP_BOOT_LED_PORT, MAIN_APP_BOOT_LED_PIN);
    }
}
/*---------- end of file ----------*/
