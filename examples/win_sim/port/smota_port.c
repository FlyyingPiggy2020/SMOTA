/*
 * Copyright (c) 2026 by Lu Xianfan.
 * @FilePath     : smota_port.c
 * @Author       : lxf
 * @Date         : 2026-01-29 18:00:00
 * @LastEditors  : lxf_zjnb@qq.com
 * @LastEditTime : 2026-01-30 10:00:00
 * @Brief        : smOTA Windows 模拟平台 HAL 驱动实现
 * @details      使用文件模拟 Flash，标准输入输出模拟通信， TinyCrypt 加密驱动
 */

/*---------- includes ----------*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#include <unistd.h>
#endif

#include "smota_user_config.h"

/* TinyCrypt 加密库头文件 */
#include <tinycrypt/sha256.h>
#include <tinycrypt/aes.h>
#include <tinycrypt/ctr_mode.h>
#include <tinycrypt/hmac.h>
#include <tinycrypt/ecc.h>
#include <tinycrypt/ecc_dsa.h>

/*---------- macro ----------*/

/**
 * @brief  Flash 模拟文件路径
 */
#define SMOTA_PORT_FLASH_FILE "flash_sim.bin"

/*---------- type define ----------*/

/**
 * @brief  Flash 模拟器上下文
 */
struct smota_port_flash_ctx {
    FILE *fp;        /* Flash 文件句柄 */
    uint8_t *buffer; /* 内存缓冲区 */
    uint32_t size;   /* Flash 大小 */
    int is_open;     /* 是否已打开 */
};

/**
 * @brief  通信模拟器上下文
 */
struct smota_port_comm_ctx {
    int is_init; /* 是否已初始化 */
};

/**
 * @brief  TinyCrypt SHA256 上下文
 */
struct tc_sha256_ctx {
    struct tc_sha256_state_struct state;
};

/**
 * @brief  TinyCrypt AES-CTR 上下文
 */
struct tc_aes_ctx {
    TCAesKeySched_t sched;  /* AES 密钥调度 */
    uint8_t ctr[16];        /* 计数器 (IV) */
};

/*---------- variable prototype ----------*/

static struct smota_port_flash_ctx g_flash_ctx = {0};
static struct smota_port_comm_ctx  g_comm_ctx = {0};

/*---------- function prototype ----------*/

/*---------- TinyCrypt SHA-256 驱动函数 (端口封装) ----------*/
void *tc_port_sha256_init(void);
int tc_port_sha256_update(void *ctx, const uint8_t *data, uint32_t size);
int tc_port_sha256_final(void *ctx, uint8_t hash[32]);

/*---------- TinyCrypt AES-128-CTR 驱动函数 (端口封装) ----------*/
void *tc_port_aes_init(const uint8_t *key, const uint8_t *iv);
int tc_port_aes_crypt(void *ctx, const uint8_t *input, uint8_t *output, uint32_t size);

/*---------- TinyCrypt ECDSA-P256 驱动函数 (端口封装) ----------*/
int tc_port_ecdsa_verify(const uint8_t *hash,
                         const uint8_t *sig_r,
                         const uint8_t *sig_s,
                         const uint8_t *pub_key);

/*---------- KDF 密钥派生函数 ----------*/
int smota_kdf_derive(const uint8_t *master_key,
                     const uint8_t *uid, uint32_t uid_len,
                     const char *context,
                     uint8_t output[32]);

/*---------- Flash 驱动实现 ----------*/

/**
 * @brief  Flash 初始化
 */
int flash_init(void)
{
    if (g_flash_ctx.is_open) {
        return 0; /* 已初始化 */
    }

    g_flash_ctx.size = SMOTA_FLASH_SIZE;
    g_flash_ctx.buffer = (uint8_t *)calloc(1, g_flash_ctx.size);
    if (g_flash_ctx.buffer == NULL) {
        SMOTA_DEBUG_PRINTF("Error: Failed to allocate flash buffer\r\n");
        return -1;
    }

    /* 尝试从文件加载已有数据 */
    FILE *fp = fopen(SMOTA_PORT_FLASH_FILE, "rb");
    if (fp != NULL) {
        fread(g_flash_ctx.buffer, 1, g_flash_ctx.size, fp);
        fclose(fp);
        SMOTA_DEBUG_PRINTF("Flash loaded from file: %s\r\n", SMOTA_PORT_FLASH_FILE);
    } else {
        SMOTA_DEBUG_PRINTF("Flash initialized with zeros\r\n");
    }

    g_flash_ctx.is_open = 1;
    return 0;
}

/**
 * @brief  Flash 去初始化
 */
int flash_deinit(void)
{
    if (!g_flash_ctx.is_open) {
        return 0;
    }

    /* 保存到文件 */
    FILE *fp = fopen(SMOTA_PORT_FLASH_FILE, "wb");
    if (fp != NULL) {
        fwrite(g_flash_ctx.buffer, 1, g_flash_ctx.size, fp);
        fclose(fp);
        SMOTA_DEBUG_PRINTF("Flash saved to file: %s\r\n", SMOTA_PORT_FLASH_FILE);
    }

    if (g_flash_ctx.buffer) {
        free(g_flash_ctx.buffer);
        g_flash_ctx.buffer = NULL;
    }

    g_flash_ctx.is_open = 0;
    return 0;
}

/**
 * @brief  Flash 读取
 */
int flash_read(uint32_t addr, uint8_t *data, uint32_t size)
{
    if (!g_flash_ctx.is_open) {
        return -1;
    }

    if (addr >= g_flash_ctx.size) {
        SMOTA_DEBUG_PRINTF("Error: Flash read addr out of range: 0x%08X\r\n", addr);
        return -1;
    }

    uint32_t avail = g_flash_ctx.size - addr;
    uint32_t read_size = (size < avail) ? size : avail;

    memcpy(data, g_flash_ctx.buffer + addr, read_size);
    return (int)read_size;
}

/**
 * @brief  Flash 写入
 */
int flash_write(uint32_t addr, const uint8_t *data, uint32_t size)
{
    if (!g_flash_ctx.is_open) {
        return -1;
    }

    if (addr >= g_flash_ctx.size) {
        SMOTA_DEBUG_PRINTF("Error: Flash write addr out of range: 0x%08X\r\n", addr);
        return -1;
    }

    uint32_t avail = g_flash_ctx.size - addr;
    uint32_t write_size = (size < avail) ? size : avail;

    memcpy(g_flash_ctx.buffer + addr, data, write_size);
    return (int)write_size;
}

/**
 * @brief  Flash 擦除
 */
int flash_erase(uint32_t addr, uint32_t size)
{
    if (!g_flash_ctx.is_open) {
        return -1;
    }

    if (addr >= g_flash_ctx.size) {
        SMOTA_DEBUG_PRINTF("Error: Flash erase addr out of range: 0x%08X\r\n", addr);
        return -1;
    }

    uint32_t avail = g_flash_ctx.size - addr;
    uint32_t erase_size = (size < avail) ? size : avail;

    memset(g_flash_ctx.buffer + addr, 0xFF, erase_size);
    SMOTA_DEBUG_PRINTF("Flash erased: addr=0x%08X, size=%u\r\n", addr, erase_size);
    return 0;
}

/**
 * @brief  Flash 上锁（模拟器无需操作）
 */
int flash_lock(void)
{
    (void)g_flash_ctx;
    return 0;
}

/**
 * @brief  Flash 解锁（模拟器无需操作）
 */
int flash_unlock(void)
{
    (void)g_flash_ctx;
    return 0;
}

/*---------- 通信驱动实现 ----------*/

/**
 * @brief  通信初始化
 */
int comm_init(void)
{
    if (g_comm_ctx.is_init) {
        return 0;
    }

    g_comm_ctx.is_init = 1;
    SMOTA_DEBUG_PRINTF("Comm driver initialized (stdio simulation)\r\n");
    return 0;
}

/**
 * @brief  通信去初始化
 */
int comm_deinit(void)
{
    g_comm_ctx.is_init = 0;
    return 0;
}

/**
 * @brief  发送数据（模拟：打印到 stdout）
 */
int comm_send(const uint8_t *data, uint32_t size)
{
    if (!g_comm_ctx.is_init) {
        return -1;
    }

    fwrite(data, 1, size, stdout);
    fflush(stdout);
    return (int)size;
}

/**
 * @brief  接收数据（模拟：从 stdin 读取）
 */
int comm_receive(uint8_t *data, uint32_t size, uint32_t timeout)
{
    if (!g_comm_ctx.is_init) {
        return -1;
    }

    (void)timeout; /* 模拟器忽略超时 */

    /* 从标准输入读取 */
    size_t read_size = fread(data, 1, size, stdin);
    return (int)read_size;
}

/*---------- 系统驱动实现 ----------*/

/**
 * @brief  获取系统滴答计数（毫秒）
 */
uint64_t system_get_tick_ms(void)
{
#ifdef _WIN32
    return (uint64_t)GetTickCount64();
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
#endif
}

/**
 * @brief  系统复位（模拟器：直接退出）
 */
void system_reset(void)
{
    SMOTA_DEBUG_PRINTF("System reset requested (simulated by exit)\r\n");
    flash_deinit(); /* 保存 Flash */
    exit(0);
}

/*---------- TinyCrypt SHA-256 驱动实现 (端口封装) ----------*/

/**
 * @brief  TinyCrypt SHA256 初始化 (端口封装)
 */
void *tc_port_sha256_init(void)
{
    struct tc_sha256_ctx *ctx;

    ctx = (struct tc_sha256_ctx *)malloc(sizeof(struct tc_sha256_ctx));
    if (ctx == NULL) {
        return NULL;
    }

    if (tc_sha256_init(&ctx->state) != TC_CRYPTO_SUCCESS) {
        free(ctx);
        return NULL;
    }

    return ctx;
}

/**
 * @brief  TinyCrypt SHA256 更新 (端口封装)
 */
int tc_port_sha256_update(void *ctx, const uint8_t *data, uint32_t size)
{
    struct tc_sha256_ctx *sha_ctx = (struct tc_sha256_ctx *)ctx;

    if (ctx == NULL || data == NULL || size == 0) {
        return -1;
    }

    if (tc_sha256_update(&sha_ctx->state, data, size) != TC_CRYPTO_SUCCESS) {
        return -2;
    }

    return 0;
}

/**
 * @brief  TinyCrypt SHA256 完成 (端口封装)
 */
int tc_port_sha256_final(void *ctx, uint8_t hash[32])
{
    struct tc_sha256_ctx *sha_ctx = (struct tc_sha256_ctx *)ctx;

    if (ctx == NULL || hash == NULL) {
        return -1;
    }

    if (tc_sha256_final(hash, &sha_ctx->state) != TC_CRYPTO_SUCCESS) {
        return -2;
    }

    free(ctx);
    return 0;
}

/*---------- TinyCrypt AES-128-CTR 驱动实现 (端口封装) ----------*/

/**
 * @brief  TinyCrypt AES-128-CTR 初始化 (端口封装)
 * @note   TinyCrypt 不直接支持 CTR 模式的初始化/加密分离，
 *         需要将 key 和 iv (ctr) 保存到上下文
 */
void *tc_port_aes_init(const uint8_t *key, const uint8_t *iv)
{
    struct tc_aes_ctx *ctx;

    if (key == NULL || iv == NULL) {
        return NULL;
    }

    ctx = (struct tc_aes_ctx *)malloc(sizeof(struct tc_aes_ctx));
    if (ctx == NULL) {
        return NULL;
    }

    /* 分配密钥调度结构 */
    ctx->sched = (TCAesKeySched_t)malloc(sizeof(struct tc_aes_key_sched_struct));
    if (ctx->sched == NULL) {
        free(ctx);
        return NULL;
    }

    /* 设置加密密钥 */
    if (tc_aes128_set_encrypt_key(ctx->sched, key) != TC_CRYPTO_SUCCESS) {
        free(ctx->sched);
        free(ctx);
        return NULL;
    }

    /* 保存计数器 (IV) - 小端格式 */
    memcpy(ctx->ctr, iv, 16);

    return ctx;
}

/**
 * @brief  TinyCrypt AES-128-CTR 加密/解密 (端口封装)
 * @note   CTR 模式下加密和解密是同一个操作
 */
int tc_port_aes_crypt(void *ctx, const uint8_t *input, uint8_t *output, uint32_t size)
{
    struct tc_aes_ctx *aes_ctx = (struct tc_aes_ctx *)ctx;

    if (ctx == NULL || input == NULL || output == NULL || size == 0) {
        return -1;
    }

    /* 使用 CTR 模式进行加密/解密 */
    if (tc_ctr_mode(output, size, input, size, aes_ctx->ctr, aes_ctx->sched) != TC_CRYPTO_SUCCESS) {
        return -2;
    }

    return (int)size;
}

/*---------- TinyCrypt ECDSA-P256 驱动实现 (端口封装) ----------*/

/**
 * @brief  TinyCrypt ECDSA-P256 签名验证 (端口封装)
 * @param  hash: 消息哈希 (32字节)
 * @param  sig_r: 签名 r 分量 (32字节)
 * @param  sig_s: 签名 s 分量 (32字节)
 * @param  pub_key: 公钥 (64字节: x + y)
 * @return 0=验证成功, <0=验证失败
 */
int tc_port_ecdsa_verify(const uint8_t *hash,
                         const uint8_t *sig_r,
                         const uint8_t *sig_s,
                         const uint8_t *pub_key)
{
    uint8_t signature[64];

    if (hash == NULL || sig_r == NULL || sig_s == NULL || pub_key == NULL) {
        return -1;
    }

    /* 组装签名格式 (r || s) */
    memcpy(signature, sig_r, 32);
    memcpy(signature + 32, sig_s, 32);

    /* 使用 secp256r1 (NIST P-256) 曲线验证签名 */
    if (uECC_verify(pub_key, hash, 32, signature, uECC_secp256r1()) != TC_CRYPTO_SUCCESS) {
        return -2;
    }

    return 0;
}

/*---------- KDF 密钥派生函数实现 ----------*/

/**
 * @brief  基于 HMAC-SHA256 的密钥派生函数
 * @details 算法: HMAC-SHA256(master_key, uid || context)
 * @param  master_key: 主密钥 (32字节)
 * @param  uid: 设备唯一标识
 * @param  uid_len: UID 长度
 * @param  context: 上下文字符串 (如 "smOTA_Enc_v1")
 * @param  output: 输出的派生密钥 (32字节)
 * @return 0=成功, <0=失败
 */
int smota_kdf_derive(const uint8_t *master_key,
                     const uint8_t *uid, uint32_t uid_len,
                     const char *context,
                     uint8_t output[32])
{
    struct tc_hmac_state_struct hmac_state;
    uint8_t key_material[64];
    uint8_t context_len;
    int ret;

    if (master_key == NULL || output == NULL) {
        return -1;
    }

    /* 计算上下文字符串长度 */
    context_len = (context != NULL) ? (uint8_t)strlen(context) : 0;

    /* 组合输入数据: uid || context */
    if (uid_len > 0 && uid != NULL) {
        memcpy(key_material, uid, uid_len);
    }
    if (context_len > 0 && context != NULL) {
        memcpy(key_material + uid_len, context, context_len);
    }

    /* 设置 HMAC 密钥 */
    ret = tc_hmac_set_key(&hmac_state, master_key, 32);
    if (ret != TC_CRYPTO_SUCCESS) {
        return -2;
    }

    /* 初始化 HMAC */
    ret = tc_hmac_init(&hmac_state);
    if (ret != TC_CRYPTO_SUCCESS) {
        return -3;
    }

    /* 更新 HMAC (输入数据) */
    ret = tc_hmac_update(&hmac_state, key_material, uid_len + context_len);
    if (ret != TC_CRYPTO_SUCCESS) {
        return -4;
    }

    /* 完成 HMAC，输出 32 字节密钥 */
    ret = tc_hmac_final(output, 32, &hmac_state);
    if (ret != TC_CRYPTO_SUCCESS) {
        return -5;
    }

    return 0;
}

/*---------- TinyCrypt Windows CSPRNG 实现 ----------*/

#ifdef _WIN32

/**
 * @brief  Windows 平台默认 CSPRNG 实现
 * @details TinyCrypt ECDSA 需要此函数生成随机数
 * @param  dest: 输出缓冲区
 * @param  size: 需要的随机字节数
 * @return 1=成功, 0=失败
 */
int default_CSPRNG(uint8_t *dest, unsigned int size)
{
    HCRYPTPROV hProv = 0;

    /* 输入检查 */
    if (dest == NULL || size == 0) {
        return 0;
    }

    /* 获取加密服务提供者 */
    if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        return 0;
    }

    /* 生成随机数 */
    if (!CryptGenRandom(hProv, size, dest)) {
        CryptReleaseContext(hProv, 0);
        return 0;
    }

    CryptReleaseContext(hProv, 0);
    return 1;
}

#endif /* _WIN32 */

/*---------- end of file ----------*/
