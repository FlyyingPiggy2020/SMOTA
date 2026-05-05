/*
 * Copyright (c) 2025 by Lu Xianfan.
 * @FilePath     : smota_port_boot.c
 * @Author       : lxf
 * @Date         : 2026-04-28 18:00:00
 * @LastEditors  : lxf_zjnb@qq.com
 * @LastEditTime : 2026-04-28 18:00:00
 * @Brief        : STM32G0 平台 smOTA Boot 策略驱动实现
 */

/*---------- includes ----------*/
#include "usb_device.h"
#include "usbd_core.h"
#include "smota_boot.h"
#include "smota_port.h"
#include "smota_port_internal.h"

/*---------- macro ----------*/

/*---------- type define ----------*/

/*---------- variable prototype ----------*/

/*---------- function prototype ----------*/
static int boot_should_stay_in_boot(void);
static int boot_jump_to_app(void);
static int boot_force_entry_requested(void);
static uint8_t smota_port_is_app_vector_valid(void);

/*---------- variable ----------*/
struct smota_boot_driver g_smota_boot_driver = {
    .should_stay_in_boot = boot_should_stay_in_boot,
    .jump_to_app = boot_jump_to_app,
};

__attribute__((used, section(".smota_boot_request")))
static volatile struct smota_boot_request g_smota_boot_request_area;

/*---------- function ----------*/
/**
 * @brief  板级强制进入 Boot 判断
 * @return 1=强制进入 Boot, 0=不强制
 */
static int boot_force_entry_requested(void)
{
    return 0;
}

/**
 * @brief  检查 App 向量表是否有效
 * @return 1=有效, 0=无效
 */
static uint8_t smota_port_is_app_vector_valid(void)
{
    uint32_t app_msp = *(const uint32_t *)SMOTA_PORT_APP_BASE_ADDR;
    uint32_t app_reset = *(const uint32_t *)(SMOTA_PORT_APP_BASE_ADDR + 4U);

    if ((app_msp < SRAM_BASE) || (app_msp >= SMOTA_PORT_SRAM_END_ADDR)) {
        return 0U;
    }

    if ((app_reset < SMOTA_PORT_APP_BASE_ADDR) ||
        (app_reset >= (SMOTA_PORT_APP_BASE_ADDR + SMOTA_APP_SIZE))) {
        return 0U;
    }

    return 1U;
}

uint8_t smota_port_is_app_valid(void)
{
    return (smota_port_is_app_vector_valid() != 0U &&
            smota_port_get_app_info() != NULL) ? 1U : 0U;
}

static int boot_should_stay_in_boot(void)
{
    if (smota_boot_request_is_set() != 0U) {
        smota_boot_request_clear();
        return 1;
    }

    smota_boot_request_clear();

    if (boot_force_entry_requested() != 0) {
        return 1;
    }

    return (smota_port_is_app_valid() == 0U) ? 1 : 0;
}

static int boot_jump_to_app(void)
{
    uint32_t i;
    uint32_t app_msp;
    uint32_t app_reset;
    void (*app_entry)(void);

    if (smota_port_is_app_valid() == 0U) {
        return -1;
    }

    app_msp = *(const uint32_t *)SMOTA_PORT_APP_BASE_ADDR;
    app_reset = *(const uint32_t *)(SMOTA_PORT_APP_BASE_ADDR + 4U);
    app_entry = (void (*)(void))app_reset;

    (void)USBD_Stop(&hUsbDeviceFS);
    (void)USBD_DeInit(&hUsbDeviceFS);
    HAL_RCC_DeInit();

    __disable_irq();
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;

    for (i = 0U; i < 8U; i++) {
        NVIC->ICER[i] = 0xFFFFFFFFU;
        NVIC->ICPR[i] = 0xFFFFFFFFU;
    }
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk | SCB_ICSR_PENDSVCLR_Msk;
    SCB->VTOR = SMOTA_PORT_APP_BASE_ADDR;
    __set_MSP(app_msp);
    __set_CONTROL(0U);
    __enable_irq();
    app_entry();

    while (1) {
    }
}

/*---------- end of file ----------*/
