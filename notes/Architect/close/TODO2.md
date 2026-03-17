# TODO2: 固件包结构设计

## 任务概述

设计完整的固件包格式，定义固件包的文件结构、加载方式和解析逻辑。

## 背景

当前需要明确固件包的格式，以便：
1. 上位机/服务器能够生成正确的固件包
2. MCU 端能够正确解析和验证固件包
3. 支持固件的完整性和安全性验证

## 参考资料

- [doc/0.requirements.md](doc/0.requirements.md) 第 376-424 行 - 固件包结构定义
- [scripts/keygen.py](scripts/keygen.py) - 密钥生成工具
- [examples/win_sim/keys/](examples/win_sim/keys/) - 测试用密钥

## 详细要求

### 1. 固件包整体格式

```
+------------------+
|   Header         |  固定 256 Bytes
+------------------+
|   Payload        |  变长 (原始 .bin 固件数据)
+------------------+
```

### 2. Header 结构 (256 Bytes)

```c
#pragma pack(push, 1)

struct smota_firmware_header {
    /* ========== Magic Word (4 Bytes) ========== */
    uint8_t  magic[4];            // 0xAA, 0x55, 0xAA, 0x55

    /* ========== 版本信息 (3 Bytes) ========== */
    uint8_t  version_major;       // 主版本号
    uint8_t  version_minor;       // 次版本号
    uint8_t  version_patch;       // 补丁版本号

    /* ========== 固件信息 (8 Bytes) ========== */
    uint32_t firmware_size;       // Payload 实际大小 (小端)
    uint32_t firmware_crc;        // Payload CRC-32 (小端)

    /* ========== 安全信息 (32 Bytes) ========== */
    uint8_t  sha256_hash[32];     // Payload SHA-256 摘要

    /* ========== 签名信息 (64 Bytes) - 可选 ========== */
    uint8_t  signature_r[32];     // ECDSA 签名 r 分量
    uint8_t  signature_s[32];     // ECDSA 签名 s 分量

    /* ========== 标志位 (4 Bytes) ========== */
    uint32_t flags;               // bit0: 加密标志
                                  // bit1: 防回滚标志
                                  // bit2: 签名标志
                                  // bit3-31: 保留

    /* ========== 时间戳 (4 Bytes) ========== */
    uint32_t timestamp;           // 构建时间戳 (Unix 时间, 小端)

    /* ========== UUID (16 Bytes) ========== */
    uint8_t  uuid[16];            // 唯一标识符

    /* ========== 硬件信息 (8 Bytes) ========== */
    uint32_t project_id;          // 项目 ID (小端)
    uint32_t hardware_ver;        // 硬件版本 (小端)

    /* ========== 保留字段 (113 Bytes) ========== */
    uint16_t header_crc;          // Header CRC-16 (小端)
    uint8_t  reserved[111];       // 保留字段

} smota_firmware_header_t;

#pragma pack(pop)
```

### 3. 标志位定义

```c
#define SMOTA_FLAG_ENCRYPTED       0x01  // bit0: 固件已加密
#define SMOTA_FLAG_ANTI_ROLLBACK   0x02  // bit1: 启用防回滚
#define SMOTA_FLAG_SIGNED          0x04  // bit2: 固件已签名
```

### 4. 固件包文件格式

#### 文件扩展名
- 完整固件包: `.smota` 或 `.ota`
- 仅固件数据: `.bin`

#### 文件结构示例
```
my_firmware_v1.0.1.smota
├── Header (256 Bytes)
│   ├── Magic: 0xAA55AA55
│   ├── Version: 1.0.1
│   ├── Size: 65536 Bytes
│   ├── SHA256: [32 bytes hash]
│   └── ...
└── Payload (65536 Bytes)
    └── [原始固件二进制数据]
```

### 5. 固件包加载 API

```c
/**
 * @brief  加载固件包
 * @param  path: 固件包文件路径
 * @param  header: 输出 Header 结构
 * @param  payload: 输出 Payload 数据指针 (需调用者释放)
 * @param  payload_size: 输出 Payload 大小
 * @return 0=成功, <0=失败
 */
int smota_firmware_load(const char *path,
                        smota_firmware_header_t *header,
                        uint8_t **payload,
                        uint32_t *payload_size);

/**
 * @brief  释放固件包资源
 * @param  payload: Payload 数据指针
 */
void smota_firmware_free(uint8_t *payload);

/**
 * @brief  验证固件包
 * @param  header: Header 结构
 * @param  payload: Payload 数据
 * @return 0=验证通过, <0=验证失败
 */
int smota_firmware_verify(const smota_firmware_header_t *header,
                          const uint8_t *payload);
```

### 6. 固件包生成工具设计

#### Python 工具 (scripts/pack_firmware.py)

```python
#!/usr/bin/env python3
"""
smOTA 固件打包工具

用法:
    python pack_firmware.py input.bin output.smota \\
        --version 1.0.1 \\
        --project-id 0x12345678 \\
        --sign \\
        --encrypt
"""

import struct
import hashlib
import argparse

def pack_firmware(input_bin, output_file, version, project_id, sign=False, encrypt=False):
    # 1. 读取原始固件
    with open(input_bin, 'rb') as f:
        payload = f.read()

    # 2. 计算 SHA-256
    sha256_hash = hashlib.sha256(payload).digest()

    # 3. 计算 CRC-32
    firmware_crc = binascii.crc32(payload) & 0xFFFFFFFF

    # 4. 构建 Header
    header = bytearray(256)
    struct.pack_into('<4s', header, 0, b'\xAA\x55\xAA\x55')
    struct.pack_into('<3B', header, 4, version[0], version[1], version[2])
    struct.pack_into('<I', header, 7, len(payload))
    struct.pack_into('<I', header, 11, firmware_crc)
    struct.pack_into('<32s', header, 15, sha256_hash)

    # 5. 可选: 签名
    if sign:
        signature = sign_firmware(hashlib.sha256(payload).digest())
        struct.pack_into('<64s', header, 47, signature)

    # 6. 可选: 加密
    if encrypt:
        payload = encrypt_firmware(payload)

    # 7. 计算 Header CRC-16
    header_crc = calc_crc16(header[:254])
    struct.pack_into('<H', header, 254, header_crc)

    # 8. 写入文件
    with open(output_file, 'wb') as f:
        f.write(header)
        f.write(payload)

    print(f"固件包已生成: {output_file}")
    print(f"大小: {len(payload)} Bytes")
```

## 设计要求

1. **兼容性**: 固件包格式应向后兼容，旧版本 Bootloader 应能识别新格式
2. **扩展性**: Header 中的保留字段允许未来添加新特性
3. **安全性**: SHA-256 和 ECDSA 签名确保固件完整性和来源可靠性
4. **可验证性**: 固件包应能被独立验证（不依赖 Bootloader）

## 验证标准

1. 能够成功生成固件包文件
2. 能够成功加载和解析固件包
3. SHA-256 验证通过
4. Header CRC-16 验证通过
5. Payload CRC-32 验证通过

## 输出文件

1. 创建 [smota_core/inc/smota_firmware.h](smota/smota_core/inc/smota_firmware.h) - 固件包结构定义
2. 创建 [smota_core/src/smota_firmware.c](smota/smota_core/src/smota_firmware.c) - 固件包加载/验证实现
3. 创建 [scripts/pack_firmware.py](scripts/pack_firmware.py) - 固件打包工具

---

**创建日期**: 2026-03-03
**分配者**: Manager
**执行者**: Architect
