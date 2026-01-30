/**
 * @file    smota_config.h
 * @brief   smOTA 库配置文件
 * @version v1.0
 * @date    2026-01-26
 *
 * @details 配置说明：
 *          1. 用户可通过编译选项 -DSMOTA_USER_CONFIG_FILE=\"xxx.h\" 引入自定义配置
 *          2. 用户配置会在默认配置之前加载，实现覆盖效果
 *          3. 未定义的配置项将使用默认值
 *
 * @example 使用自定义配置文件：
 *          // 编译选项中添加
 *          -DSMOTA_USER_CONFIG_FILE=\"my_smota_config.h\"
 *
 */

#ifndef SMOTA_CONFIG_H
#define SMOTA_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/*==============================================================================
 * 1. 用户配置文件引入（可选）
 *============================================================================*/

// 如果定义了用户配置文件路径，则引入
#ifdef SMOTA_USER_CONFIG_FILE
#include SMOTA_USER_CONFIG_FILE
#endif

/*==============================================================================
 * 2. 升级模式配置（三选一）
 *============================================================================*/

/**
 * @brief 升级模式选择
 * @details 0 = 双 Bank 硬件交换模式
 *          1 = 双槽位软件搬运模式
 *          2 = 单分区覆盖模式
 *
 *          模式说明：
 *          [0] 双 Bank 硬件交换模式
 *              适用于支持硬件 Bank 交换的 MCU（如 STM32G0/L4）
 *              特点：硬件直接对调地址映射，无需物理搬运，升级速度最快
 *
 *          [1] 双槽位软件搬运模式
 *              适用于 Flash 较大但不支持硬件交换的 MCU
 *              特点：App 存在 Active 区，备份存在 Secondary 区，由 Bootloader 执行物理拷贝
 *
 *          [2] 单分区覆盖模式
 *              适用于 Flash 空间紧缺（无法存放两份 App）的 MCU
 *              特点：App 触发重启，Bootloader 接管通讯并直接覆盖写入
 */
#ifndef SMOTA_MODE
#define SMOTA_MODE 2
#endif

/*==============================================================================
 * 3. 可靠性配置
 *============================================================================*/

// 运行可靠性和内容可靠性为默认开启，无需配置

/**
 * @brief 来源可靠性（Source Reliability）
 * @details 确保固件由唯一合法的私钥持有者签发
 *          技术：ECDSA-P256 (secp256r1) 签名验证
 *          状态：【可选】
 */
#ifndef SMOTA_RELIABILITY_SOURCE
#define SMOTA_RELIABILITY_SOURCE 0
#endif

/**
 * @brief 过程可靠性（Transmission Reliability）
 * @details 防止固件被黑客通过总线监听进行逆向工程
 *          技术：AES-128-CTR 对称加密 + HMAC-SHA256 密钥派生
 *          状态：【可选】
 */
#ifndef SMOTA_RELIABILITY_TRANSMISSION
#define SMOTA_RELIABILITY_TRANSMISSION 0
#endif

/**
 * @brief 版本可靠性（Version Reliability）
 * @details 防止黑客通过重放（Replay）带有已知漏洞的旧版合法固件攻击系统
 *          技术：防回滚机制（Anti-Rollback）
 *          状态：【可选】
 */
#ifndef SMOTA_RELIABILITY_VERSION
#define SMOTA_RELIABILITY_VERSION 0
#endif

/*==============================================================================
 * 4. Flash 布局配置
 *============================================================================*/

/**
 * @brief Flash 基地址
 * @note   通常为 0x08000000（STM32）或 0x00000000（某些 RISC-V）
 */
#ifndef SMOTA_FLASH_BASE_ADDR
#define SMOTA_FLASH_BASE_ADDR 0x08000000
#endif

/**
 * @brief Bootloader 占用空间
 * @note  根据实际功能需求调整
 */
#ifndef SMOTA_BOOTLOADER_SIZE
#define SMOTA_BOOTLOADER_SIZE 0x4000 // 16KB
#endif

/**
 * @brief 应用程序区大小
 * @note   根据 Flash 总容量和升级模式调整
 */
#ifndef SMOTA_APP_SIZE
#define SMOTA_APP_SIZE 0x40000 // 256KB
#endif

/**
 * @brief Flash 页大小
 * @note   不同 MCU 页大小不同：
 *         - STM32F1: 1KB (小页) / 2KB (大页)
 *         - STM32F4: 16KB/64KB/128KB (扇区)
 *         - STM32L4: 2KB
 */
#ifndef SMOTA_FLASH_PAGE_SIZE
#define SMOTA_FLASH_PAGE_SIZE 0x800 // 2KB
#endif

/**
 * @brief Flash 总容量
 * @note   必须根据实际 MCU 型号设置
 *         常见容量：64KB(0x10000), 128KB(0x20000), 256KB(0x40000),
 *                   512KB(0x80000), 1024KB(0x100000)
 */
#ifndef SMOTA_FLASH_SIZE
#define SMOTA_FLASH_SIZE 0x80000 // 512KB
#endif

/*==============================================================================
 * 5. 固件包配置
 *============================================================================*/
/**
 * @brief 物理层最大MTU
 * @note   设备物理支持的传输单元最大值
 */
#ifndef SMOTA_MAX_MTU_SIZE
#define SMOTA_MAX_MTU_SIZE 2048 // 字节
#endif

/*==============================================================================
 * 6. 缓冲区配置
 *============================================================================*/

/**
 * @brief 工作缓冲区大小
 * @note   用于解密、Hash 计算等操作
 *         最小 512 字节，建议 2048 字节
 */
#ifndef SMOTA_WORK_BUF_SIZE
#define SMOTA_WORK_BUF_SIZE 2048 // 字节
#endif

/**
 * @brief 解密缓冲区大小
 * @note   用于流式解密，不能超过工作缓冲区大小
 */
#ifndef SMOTA_DECRYPT_BUF_SIZE
#define SMOTA_DECRYPT_BUF_SIZE 1024 // 字节
#endif

/*==============================================================================
 * 7. 加密算法配置
 *============================================================================*/

/**
 * @brief 使用 TinyCrypt 库
 * @note   设为 0 时需要自行实现加密算法接口
 */
#ifndef SMOTA_USE_TINYCRYPT
#define SMOTA_USE_TINYCRYPT 1
#endif

// AES 加密模式固定为 CTR（唯一支持流式处理的模式）

/**
 * @brief 密钥派生上下文字符串
 * @note   用于 HMAC-SHA256 密钥派生，不同用途应使用不同字符串
 */
#ifndef SMOTA_KDF_CONTEXT
#define SMOTA_KDF_CONTEXT "smOTA_Enc_v1"
#endif

/*==============================================================================
 * 8. 调试配置
 *============================================================================*/

/**
 * @brief 启用调试输出
 * @note   开启后会输出详细的调试信息，增加代码体积
 */
#ifndef SMOTA_ENABLE_DEBUG
#define SMOTA_ENABLE_DEBUG 0
#endif

/**
 * @brief 调试输出函数
 * @note   用户可自定义输出函数（如 UART 输出）
 */
#if SMOTA_ENABLE_DEBUG
#ifndef SMOTA_DEBUG_PRINTF
#include <stdio.h>
#define SMOTA_DEBUG_PRINTF(...) printf("[smOTA] " __VA_ARGS__)
#endif
#else
#define SMOTA_DEBUG_PRINTF(...)
#endif

/*==============================================================================
 * 9. 超时配置
 *============================================================================*/

/**
 * @brief 数据包接收超时时间
 * @note   单位：毫秒
 */
#ifndef SMOTA_PACKET_TIMEOUT_MS
#define SMOTA_PACKET_TIMEOUT_MS 5000
#endif

/**
 * @brief 固件验签超时时间
 * @note   单位：毫秒
 */
#ifndef SMOTA_VERIFY_TIMEOUT_MS
#define SMOTA_VERIFY_TIMEOUT_MS 30000
#endif

/*==============================================================================
 * 10. 编译时校验
 *============================================================================*/

/* --- 模式配置校验 --- */

// 模式取值校验：必须在 0-2 范围内
#if (SMOTA_MODE < 0) || (SMOTA_MODE > 2)
#error "Error: Invalid SMOTA_MODE! Must be 0 (Dual Bank), 1 (Dual Slot), or 2 (Single Slot)."
#endif

/* --- 可靠性配置校验 --- */

// （内容可靠性和运行可靠性默认开启，无需校验）

/* --- 缓冲区配置校验 --- */

// 工作缓冲区最小值检查
#if SMOTA_WORK_BUF_SIZE < 512
#error "Error: Work buffer too small! Minimum 512 bytes required."
#endif

// 解密缓冲区不能超过工作缓冲区
#if SMOTA_DECRYPT_BUF_SIZE > SMOTA_WORK_BUF_SIZE
#error "Error: Decrypt buffer cannot exceed work buffer size!"
#endif

/* --- Flash 容量配置校验 --- */

// 单分区模式：App 区结束地址不能超过 Flash 容量
#if SMOTA_MODE == 2
#if (SMOTA_FLASH_BASE_ADDR + SMOTA_BOOTLOADER_SIZE + SMOTA_APP_SIZE) > (SMOTA_FLASH_BASE_ADDR + SMOTA_FLASH_SIZE)
#error "Error: App region exceeds Flash size! Please reduce SMOTA_APP_SIZE or check SMOTA_FLASH_SIZE."
#endif
#endif

// 双槽位模式：备份区结束地址不能超过 Flash 容量
#if SMOTA_MODE == 1
#if (SMOTA_FLASH_BASE_ADDR + SMOTA_BOOTLOADER_SIZE + SMOTA_APP_SIZE * 2) > (SMOTA_FLASH_BASE_ADDR + SMOTA_FLASH_SIZE)
#error "Error: Dual-slot mode: Backup region exceeds Flash size! Please reduce SMOTA_APP_SIZE or increase SMOTA_FLASH_SIZE."
#endif
#endif

// 双 Bank 模式：Bank1 + Bootloader + Bank2 不能超过 Flash 容量
#if SMOTA_MODE == 0
#if (SMOTA_FLASH_BASE_ADDR + SMOTA_APP_SIZE + SMOTA_BOOTLOADER_SIZE + SMOTA_APP_SIZE) > (SMOTA_FLASH_BASE_ADDR + SMOTA_FLASH_SIZE)
#error "Error: Dual-bank mode: Total size exceeds Flash capacity! Please reduce SMOTA_APP_SIZE or SMOTA_BOOTLOADER_SIZE."
#endif
#endif

/*==============================================================================
 * 11. 辅助宏定义
 *============================================================================*/

/**
 * @brief 字节数对齐宏
 */
#define SMOTA_ALIGN_UP(size, align)   (((size) + ((align) - 1)) & ~((align) - 1))
#define SMOTA_ALIGN_DOWN(size, align) ((size) & ~((align) - 1))

/**
 * @brief 获取各模式的地址偏移; App 和备份区地址计算
 */
#if SMOTA_MODE == 0
// 双 Bank 模式：App 固定在基地址，硬件自动映射
#define SMOTA_APP_OFFSET    0
#define SMOTA_BACKUP_OFFSET SMOTA_BOOTLOADER_SIZE
#define SMOTA_APP_ADDR      (SMOTA_FLASH_BASE_ADDR + SMOTA_APP_OFFSET)
#define SMOTA_BACKUP_ADDR   (SMOTA_FLASH_BASE_ADDR + SMOTA_BACKUP_OFFSET)

#elif SMOTA_MODE == 1
// 双槽位模式：需要偏移 Bootloader 空间
#define SMOTA_APP_OFFSET    SMOTA_BOOTLOADER_SIZE
#define SMOTA_BACKUP_OFFSET (SMOTA_BOOTLOADER_SIZE + SMOTA_APP_SIZE)
#define SMOTA_APP_ADDR      (SMOTA_FLASH_BASE_ADDR + SMOTA_APP_OFFSET)
#define SMOTA_BACKUP_ADDR   (SMOTA_FLASH_BASE_ADDR + SMOTA_BACKUP_OFFSET)
#elif SMOTA_MODE == 2
// 单分区模式：只有一个 App 区
#define SMOTA_APP_OFFSET SMOTA_BOOTLOADER_SIZE
#define SMOTA_APP_ADDR   (SMOTA_FLASH_BASE_ADDR + SMOTA_APP_OFFSET)
#endif

#ifdef __cplusplus
}
#endif

#endif // SMOTA_CONFIG_H
