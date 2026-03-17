# TODO1: 协议数据结构完善

## 任务概述

完善 SMOTA OTA 协议的所有数据结构定义，确保协议通信的完整性和正确性。

## 背景

当前 [smota_core/inc/smota_packet.h](smota/smota_core/inc/smota_packet.h) 已有部分结构体定义，但需要完善以确保：
1. 所有协议帧都有完整的请求/响应结构体
2. 结构体布局正确（使用 #pragma pack）
3. 字段定义与协议规范一致

## 参考资料

- [doc/4.ota-protocol.md](doc/4.ota-protocol.md) - OTA 协议规范
- [smota_core/inc/smota_packet.h](smota/smota_core/inc/smota_packet.h) - 当前协议定义
- [smota_core/src/smota_packet.c](smota/smota_core/src/smota_packet.c) - 当前实现
- [smota_core/src/smota_handler.c](smota/smota_core/src/smota_handler.c) - 协议处理函数

## 详细要求

### 1. 帧头结构 (smota_frame_header)

```c
struct smota_frame_header {
    uint8_t  sof[5];       // 起始帧: 's', 'm', 'O', 'T', 'A'
    uint8_t  ver;          // 协议版本: 0x01
    uint8_t  frag;         // 分片标志: bit7=是否分片, bit6-0=分片序号
    uint8_t  seq;          // 序列号
    uint8_t  cmd;          // 命令码
    uint16_t length;       // payload 长度 (小端)
};
```

### 2. 命令码定义

| 命令 | 命令码 | 说明 |
|:-----|:-------|:-----|
| 握手请求 | 0x01 | 客户端发起握手 |
| 握手响应 | 0x81 | 服务端响应握手 |
| 头部信息请求 | 0x02 | 发送固件头部信息 |
| 头部信息响应 | 0x82 | 确认头部信息 |
| 数据块请求 | 0x03 | 发送固件数据块 |
| 数据块响应 | 0x83 | 确认数据块 |
| 传输完成请求 | 0x04 | 通知传输完成 |
| 传输完成响应 | 0x84 | 确认传输完成 |
| 安装请求 | 0x05 | 请求安装固件 |
| 安装响应 | 0x85 | 确认安装请求 |
| 激活检查请求 | 0x06 | 检查固件激活状态 |
| 激活检查响应 | 0x86 | 返回激活状态 |

### 3. 请求/响应结构体

#### 3.1 握手 (0x01/0x81)

```c
struct smota_handshake_req {
    uint8_t  project_id[16];     // 项目 ID
    uint8_t  fw_version_major;   // 固件版本 major
    uint8_t  fw_version_minor;   // 固件版本 minor
    uint8_t  fw_version_patch;   // 固件版本 patch
    uint32_t firmware_size;      // 固件大小 (小端)
    uint16_t block_timeout;      // 数据块超时 (秒, 小端)
    uint16_t install_timeout;    // 安装超时 (秒, 小端)
    uint8_t  reserved[8];        // 保留字段
};

struct smota_handshake_resp {
    uint8_t  error_code;         // 错误码: 0=成功
    uint32_t next_offset;        // 断点续传偏移 (小端)
    uint16_t max_packet_size;    // 最大包大小 (小端)
    uint16_t mtu_size;           // MTU 大小 (小端)
    uint16_t block_timeout;      // 数据块超时 (秒, 小端)
    uint16_t install_timeout;    // 安装超时 (秒, 小端)
    uint32_t flash_free_size;    // Flash 可用空间 (小端)
    uint32_t capabilities;       // 设备能力标志 (小端)
    uint8_t  reserved[8];        // 保留字段
};
```

#### 3.2 头部信息 (0x02/0x82)

```c
struct smota_header_info_req {
    uint8_t  sha256_hash[32];    // 固件 SHA-256 哈希
    uint32_t firmware_size;      // 固件大小 (小端)
    uint8_t  fw_version_major;   // 固件版本 major
    uint8_t  fw_version_minor;   // 固件版本 minor
    uint8_t  fw_version_patch;   // 固件版本 patch
    uint8_t  flags;              // 标志位
    uint16_t header_crc;         // 头部 CRC16 (小端)
    uint8_t  reserved[8];        // 保留字段
};

struct smota_header_info_resp {
    uint8_t  error_code;         // 错误码: 0=成功
    uint8_t  reserved[15];       // 保留字段
};
```

#### 3.3 数据块 (0x03/0x83)

```c
struct smota_data_block_req {
    uint32_t offset;             // 数据偏移 (小端)
    uint16_t length;             // 数据长度 (小端)
    uint16_t block_crc;          // 数据块 CRC16 (小端)
    uint8_t  data[1];            // 变长数据区
};

struct smota_data_block_resp {
    uint8_t  error_code;         // 错误码: 0=成功
    uint32_t received_offset;    // 已接收偏移 (小端)
    uint8_t  reserved[11];       // 保留字段
};
```

#### 3.4 传输完成 (0x04/0x84)

```c
struct smota_transfer_complete_req {
    uint32_t total_size;         // 总大小 (小端)
    uint8_t  reserved[16];       // 保留字段
};

struct smota_transfer_complete_resp {
    uint8_t  error_code;         // 错误码: 0=成功
    uint8_t  verify_result;      // 验证结果: 0=通过, 1=失败
    uint8_t  reserved[14];       // 保留字段
};
```

#### 3.5 安装 (0x05/0x85)

```c
struct smota_install_req {
    uint8_t  force_install;      // 强制安装标志: 0=否, 1=是
    uint8_t  reserved[15];       // 保留字段
};

struct smota_install_resp {
    uint8_t  error_code;         // 错误码: 0=成功
    uint16_t estimated_time_s;   // 预估安装时间 (秒, 小端)
    uint8_t  reserved[13];       // 保留字段
};
```

#### 3.6 激活检查 (0x06/0x86)

```c
struct smota_activate_check_req {
    uint8_t  reserved[16];       // 保留字段
};

struct smota_activate_check_resp {
    uint8_t  error_code;         // 错误码: 0=成功
    uint8_t  fw_version_major;   // 当前固件版本 major
    uint8_t  fw_version_minor;   // 当前固件版本 minor
    uint8_t  fw_version_patch;   // 当前固件版本 patch
    uint8_t  activate_status;    // 激活状态: 0=旧固件, 1=新固件
    uint8_t  reserved[11];       // 保留字段
};
```

### 4. 错误码定义

```c
#define SMOTA_ERR_OK                          0
#define SMOTA_ERR_INVALID_STATE               1
#define SMOTA_ERR_INVALID_PARAM               2
#define SMOTA_ERR_TIMEOUT                     3
#define SMOTA_ERR_CRC                         4
#define SMOTA_ERR_VERSION                     5
#define SMOTA_ERR_VERSION_ROLLBACK            6
#define SMOTA_ERR_SPACE                       7
#define SMOTA_ERR_FLASH_WRITE                 8
#define SMOTA_ERR_FLASH_ERASE                 9
#define SMOTA_ERR_FLASH_INSUFFICIENT          10
#define SMOTA_ERR_SIGNATURE                   11
#define SMOTA_ERR_NOT_SUPPORTED               12
#define SMOTA_ERR_BUSY                        13
#define SMOTA_ERR_VERIFY_SHA256_FAILED        14
```

### 5. 固件包头结构

```c
struct smota_firmware_header {
    // Magic Word
    uint8_t  magic[4];            // 0xAA, 0x55, 0xAA, 0x55

    // 版本信息
    uint8_t  version_major;
    uint8_t  version_minor;
    uint8_t  version_patch;

    // 固件信息
    uint32_t firmware_size;       // Payload 实际大小
    uint32_t firmware_crc;        // CRC-32 校验

    // 安全信息
    uint8_t  sha256_hash[32];     // 固件 SHA-256 摘要

    // 标志位
    uint16_t flags;               // bit0: 是否加密, bit1: 是否启用防回滚
    uint16_t header_crc;          // Header 自身的 CRC-16

    // 保留字段（填充至 256 Bytes）
    uint8_t  reserved[200];
};
```

### 6. 版本结构体

```c
struct smota_version {
    uint8_t major;
    uint8_t minor;
    uint8_t patch;
    uint8_t reserved;
};

// 版本比较宏
#define SMOTA_VERSION_COMPARE(v1, v2) \
    (((v1).major > (v2).major) ? 1 : \
     ((v1).major < (v2).major) ? -1 : \
     ((v1).minor > (v2).minor) ? 1 : \
     ((v1).minor < (v2).minor) ? -1 : \
     ((v1).patch > (v2).patch) ? 1 : \
     ((v1).patch < (v2).patch) ? -1 : 0)
```

## 设计要求

1. **字节序**: 所有多字节字段使用小端序 (Little-Endian)
2. **对齐**: 所有结构体使用 `#pragma pack(push, 1)` 确保字节对齐
3. **预留字段**: 所有结构体应预留足够的保留字段用于未来扩展
4. **命名规范**:
   - 请求结构体: `smota_xxx_req`
   - 响应结构体: `smota_xxx_resp`
   - 命令码宏: `SMOTA_CMD_XXX`
   - 错误码宏: `SMOTA_ERR_XXX`

## 验证标准

1. 所有结构体都有完整的定义
2. 结构体大小明确，可以使用 `sizeof()` 获取
3. 所有结构体都有对应的注释说明
4. 与现有代码 ([smota_handler.c](smota/smota_core/src/smota_handler.c)) 中的使用保持一致

## 输出文件

1. 更新 [smota_core/inc/smota_packet.h](smota/smota_core/inc/smota_packet.h)
2. 如有需要，创建 [smota_core/inc/smota_protocol.h](smota/smota_core/inc/smota_protocol.h) 存放协议相关常量定义

---

**创建日期**: 2026-03-03
**分配者**: Manager
**执行者**: Architect
