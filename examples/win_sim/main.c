/**
 * @file    main.c
 * @brief   Windows 模拟示例主程序
 *
 * @details 演示完整的 OTA 升级流程，使用 TinyCrypt 加密驱动
 */

/*---------- includes ----------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#include "smota.h"
#include "port/smota_port.h"

/*---------- macro ----------*/

/*---------- type define ----------*/

/*---------- variable prototype ----------*/

/*---------- function prototype ----------*/

/*---------- Flash 驱动接口 ----------*/
static struct smota_flash_driver g_flash_driver = {
    .init = flash_init,
    .deinit = flash_deinit,
    .read = flash_read,
    .write = flash_write,
    .erase = flash_erase,
    .flash_lock = flash_lock,
    .flash_unlock = flash_unlock,
};

/*---------- 通信驱动接口 ----------*/
static struct smota_comm_driver g_comm_driver = {
    .init = comm_init,
    .deinit = comm_deinit,
    .send = comm_send,
    .receive = comm_receive,
};

/*---------- 加密驱动接口 ----------*/
static struct smota_crypto_driver g_crypto_driver = {
    .sha256_init = tc_port_sha256_init,
    .sha256_update = tc_port_sha256_update,
    .sha256_final = tc_port_sha256_final,
    .aes_init = tc_port_aes_init,
    .aes_crypt = tc_port_aes_crypt,
    .ecdsa_verify = tc_port_ecdsa_verify,
};

/*---------- 系统驱动接口 ----------*/
static struct smota_system_driver g_system_driver = {
    .get_tick_ms = system_get_tick_ms,
    .system_reset = system_reset,
};

/*---------- HAL 综合接口 ----------*/
static struct smota_hal g_smota_hal = {
    .flash = &g_flash_driver,
    .comm = &g_comm_driver,
    .crypto = &g_crypto_driver,
    .system = &g_system_driver,
};

/*---------- variable ----------*/

/*---------- function ----------*/

/**
 * @brief  打印使用帮助
 */
static void print_help(const char *prog)
{
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  -h, --help       Print this help message\n");
    printf("  -i, --init       Initialize Flash (clear all data)\n");
    printf("  -s, --status     Show OTA status\n");
    printf("  -r, --run        Run OTA poll loop (simulate device)\n");
    printf("  -t, --test       Run self-test\n");
    printf("\nExample:\n");
    printf("  %s -r    # Run as device, waiting for OTA commands\n", prog);
    printf("  %s -t    # Run self-test\n", prog);
}

/**
 * @brief  显示 OTA 状态
 */
static void show_status(void)
{
    smota_state_t state = smota_get_state();
    uint8_t progress = smota_get_progress();

    printf("\n=== OTA Status ===\n");
    printf("State: %s\n", smota_state_to_string(state));
    printf("Progress: %d%%\n", progress);

    if (smota_get_error() != SMOTA_ERR_OK) {
        printf("Last Error: %s (code=%d)\n",
               smota_get_error_string(),
               smota_get_error());
    }

    printf("==================\n\n");
}

/**
 * @brief  简单的自测试
 */
static int run_self_test(void)
{
    uint8_t test_data[] = "Hello, smOTA!";
    uint8_t hash[32];
    uint8_t version1[3] = { 1, 0, 0 };
    uint8_t version2[3] = { 1, 0, 1 };
    uint8_t version3[3] = { 0, 9, 0 };
    int ret = 0;

    printf("\n=== Self-Test ===\n");

    /* 测试 SHA256 */
    printf("Testing SHA256... ");
    ret = smota_sha256_compute(test_data, sizeof(test_data) - 1, hash);
    if (ret == 0) {
        printf("PASS\n");
        printf("  Hash: ");
        for (int i = 0; i < 32; i++) {
            printf("%02x", hash[i]);
        }
        printf("\n");
    } else {
        printf("FAIL (ret=%d)\n", ret);
        return -1;
    }

    /* 测试版本比较 */
    printf("Testing version check... ");
    if (smota_verify_version(version1, version2) == true && smota_verify_version(version1, version3) == false) {
        printf("PASS\n");
    } else {
        printf("FAIL\n");
        return -1;
    }

    /* 测试状态机 */
    printf("Testing state machine... ");
    smota_state_reset();
    if (smota_state_get() == SMOTA_STATE_IDLE) {
        printf("PASS\n");
    } else {
        printf("FAIL\n");
        return -1;
    }

    printf("==================\n");
    printf("All tests passed!\n\n");

    return 0;
}

/**
 * @brief  模拟设备运行
 */
static void simulate_device(void)
{
    clock_t last_tick = clock();
    const int tick_interval = CLOCKS_PER_SEC / 1000; /* 1ms */

    printf("\n=== Device Simulation Started ===\n");
    printf("Press Ctrl+C to exit\n\n");

    while (1) {
        /* 模拟 1ms 时间片 */
        clock_t current = clock();
        if (current - last_tick >= tick_interval) {
            last_tick = current;

            /* 调用 OTA poll */
            smota_poll();

            /* 可选: 打印状态变化 */
            static smota_state_t last_state = SMOTA_STATE_MAX;
            smota_state_t current_state = smota_get_state();
            if (current_state != last_state) {
                printf("[%llu] State: %s -> %s\n",
                       (unsigned long long)system_get_tick_ms(),
                       smota_state_to_string(last_state),
                       smota_state_to_string(current_state));
                last_state = current_state;
            }

            /* 错误处理 */
            if (smota_get_error() != SMOTA_ERR_OK) {
                printf("[%llu] Error: %s\n",
                       (unsigned long long)system_get_tick_ms(),
                       smota_get_error_string());
            }
        }

/* 让出 CPU */
#ifdef _WIN32
        Sleep(0);
#else
        usleep(1000);
#endif
    }
}

int main(int argc, char *argv[])
{
    int ret;
    bool init_flash = false;
    bool show_status_flag = false;
    bool run_device = false;
    bool run_test = false;

    /* 解析命令行参数 */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--init") == 0) {
            init_flash = true;
        } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--status") == 0) {
            show_status_flag = true;
        } else if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--run") == 0) {
            run_device = true;
        } else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--test") == 0) {
            run_test = true;
        }
    }

    /* 初始化 Flash */
    if (init_flash) {
        printf("Initializing Flash...\n");
        flash_init();
        flash_erase(0, SMOTA_FLASH_SIZE);
        flash_deinit();
        printf("Flash initialized.\n");
        return 0;
    }

    /* 注册 HAL 接口到 smOTA */
    ret = smota_hal_register(&g_smota_hal);
    if (ret < 0) {
        SMOTA_DEBUG_PRINTF("Error: smota_hal_register failed: %d\n", ret);
        return -1;
    }

    printf("smOTA initialized successfully (Win32 Simulation)\n");
    printf("HAL: Flash=%s, Comm=stdio, Crypto=OpenSSL\n",
           init_flash ? "file" : "memory");

    /* 初始化 OTA 模块 */
    ret = smota_init();
    if (ret < 0) {
        printf("Error: smota_init failed: %d\n", ret);
        return -1;
    }

    /* 执行选定的操作 */
    if (show_status_flag) {
        show_status();
    } else if (run_test) {
        run_self_test();
    } else if (run_device) {
        simulate_device();
    } else {
        /* 默认显示状态 */
        show_status();
        printf("Use -r to run device simulation, -t to run self-test\n");
        printf("Use -h for help\n");
    }

    /* 清理 */
    smota_deinit();
    flash_deinit();

    return 0;
}

/*---------- end of file ----------*/
