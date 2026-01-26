/**
 * @file    main.c
 * @brief   Windows 模拟示例主程序
 *
 * @details 演示完整的 OTA 升级流程
 */
#include <stdio.h>
#include "smota_config.h"

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    SMOTA_DEBUG_PRINTF("==============================================\n");
    SMOTA_DEBUG_PRINTF("    smOTA Windows Simulation Example\n");
    SMOTA_DEBUG_PRINTF("==============================================\n\n");

    SMOTA_DEBUG_PRINTF("Flash base address: 0x%08X\n", SMOTA_FLASH_BASE_ADDR);
    SMOTA_DEBUG_PRINTF("Bootloader size: %u KB\n", SMOTA_BOOTLOADER_SIZE / 1024);
    SMOTA_DEBUG_PRINTF("App size: %u KB\n", SMOTA_APP_SIZE / 1024);
    SMOTA_DEBUG_PRINTF("Page size: %u KB\n", SMOTA_FLASH_PAGE_SIZE / 1024);

#if SMOTA_MODE == 0
    SMOTA_DEBUG_PRINTF("Mode: Dual-Bank\n");
#elif SMOTA_MODE == 1
    SMOTA_DEBUG_PRINTF("Mode: Dual-Slot\n");
#else
    SMOTA_DEBUG_PRINTF("Mode: Single-Slot\n");
#endif

    SMOTA_DEBUG_PRINTF("\nHAL implementation ready.\n");
    SMOTA_DEBUG_PRINTF("Waiting for smOTA core API implementation...\n");
    SMOTA_DEBUG_PRINTF("TODO: Implement OTA_Init(), OTA_DownloadStart(), etc.\n");

    return 0;
}
