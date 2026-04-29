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
#include <string.h>
#include "usb_device.h"
#include "usbd_core.h"
#include "smota_boot.h"
#include "smota_port.h"
#include "smota_port_internal.h"

/*---------- macro ----------*/

/*---------- type define ----------*/
struct smota_port_boot_state_record {
    uint32_t state;
    uint32_t commit;
};

/*---------- variable prototype ----------*/

/*---------- function prototype ----------*/
static int boot_set_state(uint32_t state);
static int boot_get_state(uint32_t *state);
static int boot_should_stay_in_boot(void);
static int boot_jump_to_app(void);
static int boot_force_entry_requested(void);
static uint8_t smota_port_boot_state_is_valid(uint32_t state);
static int smota_port_boot_state_append(uint32_t state);
static uint8_t smota_port_is_app_vector_valid(void);

/*---------- variable ----------*/
struct smota_boot_driver g_smota_boot_driver = {
    .set_state = boot_set_state,
    .get_state = boot_get_state,
    .should_stay_in_boot = boot_should_stay_in_boot,
    .jump_to_app = boot_jump_to_app,
};

__attribute__((used, section(".smota_boot_request")))
static volatile struct smota_boot_request g_smota_boot_request_area;

/*---------- function ----------*/
/**
 * @brief  判断 Boot 状态是否有效
 * @param  state: Boot 状态
 * @return 1=有效, 0=无效
 */
static uint8_t smota_port_boot_state_is_valid(uint32_t state)
{
    return (state == SMOTA_BOOT_STATE_IN_PROGRESS ||
            state == SMOTA_BOOT_STATE_APP_VALID) ? 1U : 0U;
}

/**
 * @brief  追加写入 Boot 持久状态
 * @param  state: Boot 状态
 * @return 0=成功, <0=失败
 */
static int smota_port_boot_state_append(uint32_t state)
{
    struct smota_port_boot_state_record record;
    const struct smota_port_boot_state_record *slot;
    uint32_t write_addr = SMOTA_PORT_BOOT_CONTROL_ADDR;
    int ret;

    if (smota_port_boot_state_is_valid(state) == 0U) {
        return -1;
    }

    slot = (const struct smota_port_boot_state_record *)SMOTA_PORT_BOOT_CONTROL_ADDR;
    while ((uint32_t)slot < SMOTA_PORT_BOOT_CONTROL_END) {
        if (slot->state == SMOTA_PORT_BOOT_STATE_EMPTY &&
            slot->commit == SMOTA_PORT_BOOT_STATE_EMPTY) {
            write_addr = (uint32_t)slot;
            break;
        }
        slot++;
    }

    if ((uint32_t)slot >= SMOTA_PORT_BOOT_CONTROL_END) {
        write_addr = SMOTA_PORT_BOOT_CONTROL_ADDR;
    }

    if (HAL_FLASH_Unlock() != HAL_OK) {
        return -2;
    }

    if (write_addr == SMOTA_PORT_BOOT_CONTROL_ADDR &&
        (*(const uint32_t *)SMOTA_PORT_BOOT_CONTROL_ADDR != SMOTA_PORT_BOOT_STATE_EMPTY ||
         *(const uint32_t *)(SMOTA_PORT_BOOT_CONTROL_ADDR + 4U) != SMOTA_PORT_BOOT_STATE_EMPTY)) {
        ret = smota_port_flash_erase_pages(SMOTA_PORT_BOOT_CONTROL_ADDR, SMOTA_FLASH_PAGE_SIZE);
        if (ret < 0) {
            (void)HAL_FLASH_Lock();
            return -3;
        }
    }

    record.state = state;
    record.commit = ~state;
    ret = smota_port_flash_program_doubleword(write_addr, (const uint8_t *)&record);
    (void)HAL_FLASH_Lock();

    return ret;
}

/**
 * @brief  设置 Boot 持久状态
 * @param  state: Boot 状态
 * @return 0=成功, <0=失败
 */
static int boot_set_state(uint32_t state)
{
    return smota_port_boot_state_append(state);
}

/**
 * @brief  读取最新 Boot 持久状态
 * @param  state: Boot 状态输出
 * @return 0=成功, <0=无有效状态
 */
static int boot_get_state(uint32_t *state)
{
    const struct smota_port_boot_state_record *slot;
    uint32_t latest_state = SMOTA_PORT_BOOT_STATE_EMPTY;
    uint8_t found = 0U;

    if (state == NULL) {
        return -1;
    }

    slot = (const struct smota_port_boot_state_record *)SMOTA_PORT_BOOT_CONTROL_ADDR;
    while ((uint32_t)slot < SMOTA_PORT_BOOT_CONTROL_END) {
        if (slot->state == SMOTA_PORT_BOOT_STATE_EMPTY &&
            slot->commit == SMOTA_PORT_BOOT_STATE_EMPTY) {
            break;
        }

        if (smota_port_boot_state_is_valid(slot->state) != 0U &&
            slot->commit == ~slot->state) {
            latest_state = slot->state;
            found = 1U;
        }
        slot++;
    }

    if (found == 0U) {
        return -2;
    }

    *state = latest_state;
    return 0;
}

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
    uint32_t boot_state;

    if (boot_get_state(&boot_state) == 0 &&
        boot_state == SMOTA_BOOT_STATE_IN_PROGRESS) {
        return 0U;
    }

    return (smota_port_is_app_vector_valid() != 0U &&
            smota_port_get_app_info() != NULL) ? 1U : 0U;
}

static int boot_should_stay_in_boot(void)
{
    uint32_t boot_state;

    if (smota_boot_request_is_set() != 0U) {
        smota_boot_request_clear();
        return 1;
    }

    if (boot_force_entry_requested() != 0) {
        return 1;
    }

    if (boot_get_state(&boot_state) == 0 &&
        boot_state == SMOTA_BOOT_STATE_IN_PROGRESS) {
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
