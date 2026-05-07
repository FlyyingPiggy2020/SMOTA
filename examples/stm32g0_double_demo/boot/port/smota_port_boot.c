/*
 * Copyright (c) 2025 by Lu Xianfan.
 * @FilePath     : smota_port_boot.c
 * @Author       : lxf
 * @Date         : 2026-04-28 18:00:00
 * @LastEditors  : lxf_zjnb@qq.com
 * @LastEditTime : 2026-05-05 10:40:24
 * @Brief        : STM32G0 平台 smOTA Boot 策略驱动实现
 */

/*---------- includes ----------*/
#include "usb_device.h"
#include "usbd_core.h"
#include "smota_port.h"
#include "smota_port_internal.h"

/*---------- macro ----------*/
#define SMOTA_PORT_BOOT_REQUEST_MAGIC 0x53424F54UL
#define SMOTA_PORT_BOOT_RESET_MAGIC   0x3F4089FCUL

/*---------- type define ----------*/

/*---------- variable prototype ----------*/

/*---------- function prototype ----------*/
static int boot_should_stay_in_boot(void);
static int boot_jump_to_app(void);

/*---------- variable ----------*/
struct smota_boot_driver g_smota_boot_driver = {
    .should_stay_in_boot = boot_should_stay_in_boot,
    .jump_to_app = boot_jump_to_app,
};

static volatile uint32_t request[2] __attribute__( ( section( "NoInit"),zero_init) );

/*---------- function ----------*/
static int boot_should_stay_in_boot(void)
{
    // 程序主动进入
    if (request[0] == SMOTA_PORT_BOOT_REQUEST_MAGIC && request[1] == ~SMOTA_PORT_BOOT_REQUEST_MAGIC) {
        request[0] = 0U;
        request[1] = 0U;
        return 1;
    }
    // 未断电情况下，重新复位
    if (request[0] == SMOTA_PORT_BOOT_RESET_MAGIC && request[1] == ~SMOTA_PORT_BOOT_RESET_MAGIC) {
        return 2;
    }
    // 断电情况下重新复位
    return 0;
}

uint8_t smota_port_is_app_valid(void)
{
    uint32_t app_msp = *(const uint32_t *)SMOTA_PORT_APP_BASE_ADDR;
    uint32_t app_reset = *(const uint32_t *)(SMOTA_PORT_APP_BASE_ADDR + 4U);

    if ((app_msp < SRAM_BASE) || (app_msp >= SMOTA_PORT_SRAM_END_ADDR)) {
        return 0U;
    }

    if ((app_reset < SMOTA_PORT_APP_BASE_ADDR) || (app_reset >= (SMOTA_PORT_APP_BASE_ADDR + SMOTA_APP_SIZE))) {
        return 0U;
    }

    return (smota_port_get_app_info() != NULL) ? 1U : 0U;
}

static int boot_jump_to_app(void)
{
    if (smota_port_is_app_valid() == 0U) {
        return -1;
    }
    uint32_t i = 0;
    void (*SysMemBootJump)(void);            /* 声明一个函数指针 */
    __IO uint32_t BootAddr = SMOTA_PORT_APP_BASE_ADDR; /* STM32H7的系统BootLoader地址 */

    /* 设置所有时钟到默认状态，使用HSI时钟 */
    HAL_RCC_DeInit();

    /* 关闭全局中断 */
    __disable_irq();

    /* 关闭滴答定时器，复位到默认值 */
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;

    /* 关闭所有中断，清除所有中断挂起标志 */
    for (i = 0; i < 8; i++) {
        NVIC->ICER[i] = 0xFFFFFFFF;
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }

    /* 使能全局中断 */
    __enable_irq();

    /* 跳转到系统BootLoader，首地址是MSP，地址+4是复位中断服务程序地址 */
    SysMemBootJump = (void (*)(void))(*((uint32_t *)(BootAddr + 4)));

    /* 设置主堆栈指针 */
    __set_MSP(*(uint32_t *)BootAddr);

    /* 在RTOS工程，这条语句很重要，设置为特权级模式，使用MSP指针 */
    __set_CONTROL(0);

    /* 跳转到系统BootLoader */
    SysMemBootJump();

    /* 跳转成功的话，不会执行到这里，用户可以在这里添加代码 */
    while (1) {
    }
}

/*---------- end of file ----------*/
