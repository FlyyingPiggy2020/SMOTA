# TODO3: 单分区模式 Flash 布局设计

## 任务概述

设计单分区覆盖模式 (SMOTA_MODE=2) 下的 Flash 布局，包括分区划分、状态标志存储和版本信息管理。

## 背景

单分区覆盖模式适用于 Flash 空间紧缺的设备，无法存储两份固件。因此需要：
1. 合理规划 Flash 分区布局
2. 设计可靠的状态标志机制
3. 实现断电保护

## 参考资料

- [doc/0.requirements.md](doc/0.requirements.md) 第 736-765 行 - 单分区模式流程
- [smota_core/inc/smota_config.h](smota/smota_core/inc/smota_config.h) - Flash 配置
- [examples/win_sim/port/smota_port.c](examples/win_sim/port/smota_port.c) - Flash 模拟

## 详细要求

### 1. Flash 分区布局

```
+------------------------+ 0x08000000
|   Bootloader (16KB)    |  Bootloader 区
+------------------------+ 0x08004000
|                        |
|   App (256KB)          |  应用程序区
|                        |
+------------------------+ 0x08044000
|   OTA State (1KB)      |  状态标志区
+------------------------+ 0x08044400
|   Config (1KB)         |  配置区
+------------------------+ 0x08044800
|   Reserved (14KB)      |  保留区
+------------------------+ 0x08008000 (Flash 结束)
```

### 2. 地址定义

```c
/* Flash 基地址 */
#define SMOTA_FLASH_BASE            0x08000000

/* Bootloader 区 */
#define SMOTA_BOOTLOADER_OFFSET     0x00000000
#define SMOTA_BOOTLOADER_ADDR       (SMOTA_FLASH_BASE + SMOTA_BOOTLOADER_OFFSET)
#define SMOTA_BOOTLOADER_SIZE       0x00004000  /* 16KB */

/* App 区 */
#define SMOTA_APP_OFFSET            0x00004000
#define SMOTA_APP_ADDR              (SMOTA_FLASH_BASE + SMOTA_APP_OFFSET)
#define SMOTA_APP_SIZE              0x00040000  /* 256KB */

/* OTA 状态区 */
#define SMOTA_STATE_OFFSET          0x00044000
#define SMOTA_STATE_ADDR            (SMOTA_FLASH_BASE + SMOTA_STATE_OFFSET)
#define SMOTA_STATE_SIZE            0x00000400  /* 1KB */

/* 配置区 */
#define SMOTA_CONFIG_OFFSET         0x00044400
#define SMOTA_CONFIG_ADDR           (SMOTA_FLASH_BASE + SMOTA_CONFIG_OFFSET)
#define SMOTA_CONFIG_SIZE           0x00000400  /* 1KB */
```

### 3. OTA 状态标志结构

```c
#pragma pack(push, 1)

/**
 * @brief  OTA 状态标志
 * @details 存储在 Flash 的 OTA 状态区，用于断电保护
 */
struct smota_ota_state {
    uint8_t  magic[4];            // 魔数: 'O', 'T', 'A', 'S'
    uint8_t  state;               // 当前状态
    uint8_t  flags;               // 标志位
    uint16_t crc;                 // CRC-16 校验
    uint32_t firmware_size;       // 新固件大小
    uint32_t received_size;       // 已接收大小
    uint8_t  new_version[3];      // 新固件版本
    uint8_t  current_version[3];  // 当前固件版本
    uint32_t timestamp;           // 时间戳
    uint8_t  reserved[16];        // 保留字段
};

/* 状态值定义 */
#define SMOTA_OTA_STATE_IDLE            0x00
#define SMOTA_OTA_STATE_DOWNLOADING     0x01
#define SMOTA_OTA_STATE_DOWNLOADED      0x02
#define SMOTA_OTA_STATE_VERIFYING       0x03
#define SMOTA_OTA_STATE_VERIFIED        0x04
#define SMOTA_OTA_STATE_INSTALLING      0x05
#define SMOTA_OTA_STATE_INSTALLED       0x06
#define SMOTA_OTA_STATE_ERROR           0xFF

/* 标志位定义 */
#define SMOTA_OTA_FLAG_VALID            0x01  // 状态有效
#define SMOTA_OTA_FLAG_NEED_REBOOT      0x02  // 需要重启
#define SMOTA_OTA_FLAG_UPDATE_SUCCESS   0x04  // 更新成功

#pragma pack(pop)
```

### 4. 版本信息结构

```c
#pragma pack(push, 1)

/**
 * @brief  固件版本信息
 * @details 存储在 Flash 的配置区
 */
struct smota_version_info {
    uint8_t  magic[4];            // 魔数: 'V', 'E', 'R', 'S'
    uint8_t  major;               // 主版本号
    uint8_t  minor;               // 次版本号
    uint8_t  patch;               // 补丁版本号
    uint8_t  flags;               // 标志位
    uint16_t crc;                 // CRC-16 校验
    uint32_t build_time;          // 构建时间戳
    uint32_t firmware_size;       // 固件大小
    uint8_t  sha256_hash[32];     // 固件 SHA-256
    uint8_t  reserved[16];        // 保留字段
};

#pragma pack(pop)
```

### 5. 状态标志操作 API

```c
/**
 * @brief  初始化 OTA 状态
 * @return 0=成功, <0=失败
 */
int smota_state_init(void);

/**
 * @brief  读取 OTA 状态
 * @param  state: 输出状态结构
 * @return 0=成功, <0=失败
 */
int smota_state_read(struct smota_ota_state *state);

/**
 * @brief  写入 OTA 状态
 * @param  state: 状态结构
 * @return 0=成功, <0=失败
 */
int smota_state_write(const struct smota_ota_state *state);

/**
 * @brief  清除 OTA 状态
 * @return 0=成功, <0=失败
 */
int smota_state_clear(void);

/**
 * @brief  验证 OTA 状态有效性
 * @param  state: 状态结构
 * @return 1=有效, 0=无效
 */
int smota_state_is_valid(const struct smota_ota_state *state);
```

### 6. 版本信息操作 API

```c
/**
 * @brief  读取当前版本信息
 * @param  version: 输出版本结构
 * @return 0=成功, <0=失败
 */
int smota_version_read(struct smota_version_info *version);

/**
 * @brief  写入版本信息
 * @param  version: 版本结构
 * @return 0=成功, <0=失败
 */
int smota_version_write(const struct smota_version_info *version);

/**
 * @brief  版本比较
 * @param  v1: 版本 1
 * @param  v2: 版本 2
 * @return 1=v1>v2, 0=v1=v2, -1=v1<v2
 */
int smota_version_compare(const struct smota_version_info *v1,
                          const struct smota_version_info *v2);
```

### 7. 断电保护流程

```
┌─────────────────────────────────────────────────────────────┐
│                    断电保护流程                              │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  正常升级流程:                                              │
│  ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌────────┐│
│  │ 下载固件 │───▶│ 验证固件 │───▶│ 写入状态 │───▶│ 重启   ││
│  └──────────┘    └──────────┘    └──────────┘    └────────┘│
│       │                                  │                  │
│       ▼                                  ▼                  │
│  ┌──────────┐                    ┌──────────┐              │
│  │状态=下载中│                    │状态=已安装│              │
│  └──────────┘                    └──────────┘              │
│                                                             │
│  断电后恢复流程 (Bootloader 启动时):                        │
│  ┌──────────┐    ┌──────────┐    ┌──────────┐             │
│  │读取状态  │───▶│状态判断  │───▶│处理恢复  │             │
│  └──────────┘    └──────────┘    └──────────┘             │
│       │                                  │                 │
│       ▼                                  ▼                 │
│  ┌──────────┐                    ┌──────────┐             │
│  │下载中    │                    │已安装    │             │
│  │→ 继续下载│                    │→ 激活新固件│            │
│  └──────────┘                    └──────────┘             │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 8. 状态转换流程

```
                    ┌─────────────┐
                    │    IDLE     │
                    └──────┬──────┘
                           │ 收到升级请求
                           ▼
                    ┌─────────────┐
                    │ DOWNLOADING │  ← 状态=下载中
                    └──────┬──────┘
                           │ 下载完成
                           ▼
                    ┌─────────────┐
                    │ DOWNLOADED  │  ← 状态=已下载
                    └──────┬──────┘
                           │ 验证通过
                           ▼
                    ┌─────────────┐
                    │ VERIFIED    │  ← 状态=已验证
                    └──────┬──────┘
                           │ 开始安装
                           ▼
                    ┌─────────────┐
                    │ INSTALLING  │  ← 状态=安装中
                    └──────┬──────┘
                           │ 安装完成
                           ▼
                    ┌─────────────┐
                    │ INSTALLED   │  ← 状态=已安装
                    └──────┬──────┘
                           │ 激活新固件
                           ▼
                    ┌─────────────┐
                    │    IDLE     │  ← 清除状态
                    └─────────────┘
```

## 设计要求

1. **断电保护**: 任何时刻断电，系统重启后都能恢复到一致状态
2. **原子性**: 状态写入应具有原子性（使用双备份或 CRC 校验）
3. **可靠性**: 关键信息使用 CRC 校验确保数据完整性
4. **兼容性**: 状态结构应预留扩展字段

## 验证标准

1. 能够成功读写 OTA 状态
2. 断电后能够正确恢复状态
3. 版本比较逻辑正确
4. CRC 校验能够检测数据损坏

## 输出文件

1. 创建 [smota_core/inc/smota_storage.h](smota/smota_core/inc/smota_storage.h) - 存储相关定义
2. 创建 [smota_core/src/smota_storage.c](smota/smota_core/src/smota_storage.c) - 存储操作实现
3. 更新 [smota_core/inc/smota_config.h](smota/smota_core/inc/smota_config.h) - 添加地址定义

---

**创建日期**: 2026-03-03
**分配者**: Manager
**执行者**: Architect
