# TODO4: 错误处理机制设计

## 任务概述

设计完整的错误处理和恢复机制，包括错误码定义、错误日志、错误恢复策略和调试输出接口。

## 背景

OTA 升级过程涉及多个环节，任何环节出错都需要能够：
1. 正确报告错误原因
2. 记录错误日志用于调试
3. 提供错误恢复机制
4. 向上层应用反馈错误状态

## 参考资料

- [smota_core/inc/smota_types.h](smota/smota_core/inc/smota_types.h) - 当前错误码定义
- [smota_core/src/smota_types.c](smota/smota_core/src/smota_types.c) - 类型实现
- [smota_core/inc/smota_config.h](smota/smota_core/inc/smota_config.h) - 调试配置

## 详细要求

### 1. 错误码定义

```c
/**
 * @brief  OTA 错误码枚举
 */
typedef enum {
    /* 成功 (0) */
    SMOTA_ERR_OK                        = 0,

    /* 通用错误 (1-10) */
    SMOTA_ERR_INVALID_STATE             = 1,    // 无效状态
    SMOTA_ERR_INVALID_PARAM             = 2,    // 无效参数
    SMOTA_ERR_NOT_SUPPORTED             = 3,    // 功能不支持
    SMOTA_ERR_BUSY                      = 4,    // 设备忙
    SMOTA_ERR_NO_MEMORY                 = 5,    // 内存不足

    /* 通信错误 (11-20) */
    SMOTA_ERR_TIMEOUT                   = 11,   // 操作超时
    SMOTA_ERR_COMM_FAILED               = 12,   // 通信失败
    SMOTA_ERR_PACKET_INVALID            = 13,   // 无效数据包
    SMOTA_ERR_CRC_FAILED                = 14,   // CRC 校验失败

    /* 版本错误 (21-30) */
    SMOTA_ERR_VERSION                   = 21,   // 版本错误
    SMOTA_ERR_VERSION_ROLLBACK          = 22,   // 版本回滚
    SMOTA_ERR_VERSION_MISMATCH          = 23,   // 版本不匹配

    /* Flash 错误 (31-40) */
    SMOTA_ERR_FLASH                     = 31,   // Flash 操作失败
    SMOTA_ERR_FLASH_WRITE               = 32,   // Flash 写入失败
    SMOTA_ERR_FLASH_ERASE               = 33,   // Flash 擦除失败
    SMOTA_ERR_FLASH_READ                = 34,   // Flash 读取失败
    SMOTA_ERR_FLASH_INSUFFICIENT        = 35,   // Flash 空间不足
    SMOTA_ERR_FLASH_LOCKED              = 36,   // Flash 已锁定

    /* 验证错误 (41-50) */
    SMOTA_ERR_VERIFY                    = 41,   // 验证失败
    SMOTA_ERR_VERIFY_SHA256_FAILED      = 42,   // SHA-256 验证失败
    SMOTA_ERR_VERIFY_CRC32_FAILED       = 43,   // CRC-32 验证失败
    SMOTA_ERR_VERIFY_HEADER_FAILED      = 44,   // Header 验证失败

    /* 安全错误 (51-60) */
    SMOTA_ERR_SIGNATURE                 = 51,   // 签名验证失败
    SMOTA_ERR_DECRYPT                   = 52,   // 解密失败
    SMOTA_ERR_KEY_INVALID               = 53,   // 密钥无效

    /* 系统错误 (61-70) */
    SMOTA_ERR_SYSTEM                    = 61,   // 系统错误
    SMOTA_ERR_HARDWARE                  = 62,   // 硬件错误
    SMOTA_ERR_WATCHDOG                  = 63,   // 看门狗超时
    SMOTA_ERR_POWER                     = 64,   // 电源异常

    /* 未知错误 */
    SMOTA_ERR_UNKNOWN                   = 255,

} smota_err_t;
```

### 2. 错误上下文结构

```c
/**
 * @brief  错误上下文
 * @details 记录错误发生时的详细信息
 */
struct smota_error_context {
    smota_err_t    error_code;       // 错误码
    smota_state_t  state;            // 发生错误时的状态
    uint32_t       timestamp;        // 错误发生时间戳
    uint16_t       line;             // 发生错误的代码行号
    const char    *file;             // 发生错误的文件名
    const char    *function;         // 发生错误的函数名
    char           message[64];      // 错误描述信息
    uint8_t        reserved[32];     // 保留字段
};
```

### 3. 错误日志系统

```c
/**
 * @brief  错误日志条目
 */
struct smota_error_log {
    uint32_t    timestamp;           // 时间戳
    smota_err_t error_code;          // 错误码
    uint8_t     state;               // 当时状态
    uint16_t    data;                // 附加数据
    uint8_t     reserved[8];         // 保留字段
};

/**
 * @brief  初始化错误日志系统
 * @param  buffer: 日志缓冲区
 * @param  size: 缓冲区大小（条目数）
 * @return 0=成功, <0=失败
 */
int smota_log_init(struct smota_error_log *buffer, uint16_t size);

/**
 * @brief  记录错误
 * @param  error_code: 错误码
 * @param  data: 附加数据
 * @return 0=成功, <0=失败
 */
int smota_log_error(smota_err_t error_code, uint16_t data);

/**
 * @brief  获取错误日志
 * @param  index: 日志索引
 * @param  log: 输出日志条目
 * @return 0=成功, <0=失败
 */
int smota_log_get(uint16_t index, struct smota_error_log *log);

/**
 * @brief  清除错误日志
 * @return 0=成功, <0=失败
 */
int smota_log_clear(void);
```

### 4. 错误恢复策略

```c
/**
 * @brief  错误恢复策略
 */
typedef enum {
    SMOTA_RECOVER_NONE,              // 不自动恢复
    SMOTA_RECOVER_RETRY,             // 重试
    SMOTA_RECOVER_ROLLBACK,          // 回滚
    SMOTA_RECOVER_RESET,             // 复位
    SMOTA_RECOVER_ABORT,             // 中止
} smota_recovery_t;

/**
 * @brief  错误处理配置
 */
struct smota_error_config {
    smota_err_t      error_code;     // 错误码
    smota_recovery_t recovery;       // 恢复策略
    uint8_t         max_retry;       // 最大重试次数
    uint16_t        retry_delay_ms;  // 重试延迟
};

/**
 * @brief  处理错误
 * @param  error: 错误码
 * @return 处理后的错误码
 */
smota_err_t smota_error_handle(smota_err_t error);

/**
 * @brief  设置错误恢复策略
 * @param  config: 配置数组
 * @param  count: 配置数量
 * @return 0=成功, <0=失败
 */
int smota_error_set_recovery(const struct smota_error_config *config,
                              uint16_t count);
```

### 5. 调试输出接口

```c
/**
 * @brief  日志级别
 */
typedef enum {
    SMOTA_LOG_LEVEL_NONE = 0,        // 关闭
    SMOTA_LOG_LEVEL_ERROR,           // 错误
    SMOTA_LOG_LEVEL_WARN,            // 警告
    SMOTA_LOG_LEVEL_INFO,            // 信息
    SMOTA_LOG_LEVEL_DEBUG,           // 调试
    SMOTA_LOG_LEVEL_VERBOSE,         // 详细
} smota_log_level_t;

/**
 * @brief  设置日志级别
 * @param  level: 日志级别
 */
void smota_log_set_level(smota_log_level_t level);

/**
 * @brief  获取日志级别
 * @return 当前日志级别
 */
smota_log_level_t smota_log_get_level(void);

/**
 * @brief  设置日志输出函数
 * @param  output: 输出函数指针
 */
typedef void (*smota_log_output_fn)(const char *message);
void smota_log_set_output(smota_log_output_fn output);

/**
 * @brief  调试输出宏
 */
#define SMOTA_LOGE(fmt, ...)    smota_log_output(SMOTA_LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define SMOTA_LOGW(fmt, ...)    smota_log_output(SMOTA_LOG_LEVEL_WARN, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define SMOTA_LOGI(fmt, ...)    smota_log_output(SMOTA_LOG_LEVEL_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define SMOTA_LOGD(fmt, ...)    smota_log_output(SMOTA_LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define SMOTA_LOGV(fmt, ...)    smota_log_output(SMOTA_LOG_LEVEL_VERBOSE, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
```

### 6. 错误信息字符串表

```c
/**
 * @brief  获取错误码对应的字符串
 * @param  error: 错误码
 * @return 错误描述字符串
 */
const char *smota_err_to_string(smota_err_t error);

/**
 * @brief  获取状态对应的字符串
 * @param  state: 状态
 * @return 状态描述字符串
 */
const char *smota_state_to_string(smota_state_t state);

/**
 * @brief  获取详细的错误描述
 * @param  error: 错误码
 * @param  buffer: 输出缓冲区
 * @param  size: 缓冲区大小
 * @return 实际写入字节数
 */
int smota_err_get_detail(smota_err_t error, char *buffer, uint16_t size);
```

### 7. 断言宏

```c
/**
 * @brief  断言宏（仅在调试模式生效）
 */
#if SMOTA_ENABLE_DEBUG
    #define SMOTA_ASSERT(expr) \
        do { \
            if (!(expr)) { \
                SMOTA_LOGE("Assert failed: %s at %s:%d", #expr, __FILE__, __LINE__); \
                smota_error_handle(SMOTA_ERR_UNKNOWN); \
            } \
        } while(0)

    #define SMOTA_ASSERT_ERROR(expr, error) \
        do { \
            if (!(expr)) { \
                SMOTA_LOGE("Assert failed: %s (code=%d) at %s:%d", #expr, error, __FILE__, __LINE__); \
                smota_error_handle(error); \
            } \
        } while(0)
#else
    #define SMOTA_ASSERT(expr) ((void)0)
    #define SMOTA_ASSERT_ERROR(expr, error) ((void)0)
#endif
```

## 设计要求

1. **层次化**: 错误处理分层次，底层错误向上传递
2. **可追踪**: 所有错误都记录日志，包含上下文信息
3. **可恢复**: 部分错误可自动恢复或重试
4. **低开销**: 日志系统在 Release 模式下可完全移除

## 验证标准

1. 所有错误码都有对应的字符串描述
2. 错误日志能够正确记录和读取
3. 错误恢复策略能够正确执行
4. 调试输出能够正确显示

## 输出文件

1. 更新 [smota_core/inc/smota_types.h](smota/smota_core/inc/smota_types.h) - 添加错误相关定义
2. 更新 [smota_core/src/smota_types.c](smota/smota_core/src/smota_types.c) - 实现错误处理函数
3. 创建 [smota_core/inc/smota_log.h](smota/smota_core/inc/smota_log.h) - 日志系统定义
4. 创建 [smota_core/src/smota_log.c](smota/smota_core/src/smota_log.c) - 日志系统实现

---

**创建日期**: 2026-03-03
**分配者**: Manager
**执行者**: Architect
