# smOTA 开发任务清单

> **项目**: Universal Secure OTA Library (smOTA)
> **创建日期**: 2026-01-30
> **目标**: 实现 OTA 协议核心功能，用户仅需注册 HAL 接口、调用初始化和 poll 循环即可完成移植

---

## 项目概述

本项目的目标是为 smOTA 实现完整的 OTA 升级协议，包括三个阶段：
1. **阶段一：握手阶段** (我守) - 协商参数、验证设备能力
2. **阶段二：传输阶段** - 分包传输固件数据、重传控制
3. **阶段三：升级完成阶段** - 验签、激活、重启

用户使用 API：
```c
// 1. 注册 HAL 接口
smota_hal_register(&smota_hal);

// 2. 初始化（可选）
smota_init();

// 3. Poll 循环（在主循环中每1ms调用一次）
smota_poll();
```

---

## 模块 A: 核心数据结构定义 (smota_types.h/c)

### A.1 状态枚举定义
- [ ] 定义 OTA 状态枚举 `enum smota_state`
  - `SMOTA_STATE_IDLE` - 空闲状态
  - `SMOTA_STATE_HANDSHAKE` - 握手阶段
  - `SMOTA_STATE_TRANSFER` - 传输阶段
  - `SMOTA_STATE_VERIFY` - 验签阶段
  - `SMOTA_STATE_INSTALL` - 安装阶段
  - `SMOTA_STATE_ACTIVATE` - 激活阶段
  - `SMOTA_STATE_ERROR` - 错误状态

### A.2 OTA 上下文结构体
- [ ] 定义 `struct smota_ctx` - OTA 核心上下文
  - 当前状态
  - 固件版本信息
  - Flash 地址信息
  - 超时配置
  - 接收缓冲区
  - SHA-256 上下文
  - AES 上下文（如需加密）

### A.3 设备信息结构体
- [ ] 定义 `struct smota_device_info` - 设备信息
  - 当前固件版本
  - 项目 ID
  - 设备能力标志
  - Flash 容量信息

---

## 模块 B: CRC16 校验实现 (smota_packet.c)

### B.1 CRC16 计算函数
- [ ] 实现 `smota_crc16_compute()` - CRC-16 计算
  - 多项式：0x1021
  - 初始值：0xFFFF
  - 输入：数据指针 + 长度
  - 输出：16位 CRC 值

### B.2 CRC16 验证函数
- [ ] 实现 `smota_crc16_verify()` - 验证帧 CRC
  - 从帧中提取 CRC
  - 计算并比较

---

## 模块 C: 包解析与序列化 (smota_packet.c)

### C.1 帧接收解析
- [ ] 实现 `smota_frame_parse()` - 解析接收到的帧
  - 验证 SOF (smOTA)
  - 验证协议版本
  - 提取命令码
  - 提取 payload
  - 验证 CRC16

### C.2 帧发送构建
- [ ] 实现 `smota_frame_build()` - 构建待发送的帧
  - 填充帧头
  - 填充 payload
  - 计算 CRC16

### C.3 Payload 解析函数
- [ ] 实现 `smota_parse_handshake_req()` - 解析握手请求
- [ ] 实现 `smota_parse_header_info_req()` - 解析头部信息请求
- [ ] 实现 `smota_parse_data_block_req()` - 解析数据块请求
- [ ] 实现 `smota_parse_transfer_complete_req()` - 解析传输完成请求
- [ ] 实现 `smota_parse_install_req()` - 解析安装请求
- [ ] 实现 `smota_parse_activate_check_req()` - 解析激活检查请求

### C.4 响应构建函数
- [ ] 实现 `smota_build_handshake_resp()` - 构建握手响应
- [ ] 实现 `smota_build_header_info_resp()` - 构建头部信息响应
- [ ] 实现 `smota_build_data_block_resp()` - 构建数据块响应
- [ ] 实现 `smota_build_transfer_complete_resp()` - 构建传输完成响应
- [ ] 实现 `smota_build_install_resp()` - 构建安装响应
- [ ] 实现 `smota_build_activate_check_resp()` - 构建激活检查响应

---

## 模块 D: 状态机实现 (smota_state.c)

### D.1 状态机核心
- [ ] 实现 `smota_state_init()` - 初始化状态机
- [ ] 实现 `smota_state_reset()` - 重置状态机
- [ ] 实现 `smota_state_get()` - 获取当前状态
- [ ] 实现 `smota_state_set()` - 设置状态（带状态转换验证）

### D.2 状态处理函数
- [ ] 实现 `smota_state_idle()` - 空闲状态处理
- [ ] 实现 `smota_state_handshake()` - 握手状态处理
- [ ] 实现 `smota_state_transfer()` - 传输状态处理
- [ ] 实现 `smota_state_verify()` - 验签状态处理
- [ ] 实现 `smota_state_install()` - 安装状态处理
- [ ] 实现 `smota_state_activate()` - 激活状态处理
- [ ] 实现 `smota_state_error()` - 错误状态处理

---

## 模块 E: 阶段一 - 握手阶段实现

### E.1 握手请求处理 (0x01)
- [ ] 实现 `smota_handle_handshake_req()` - 处理握手请求
  - 验证协议版本
  - 验证项目 ID
  - 检查版本号（防回滚）
  - 检查 Flash 空间是否足够
  - 确定超时参数
  - 构建握手响应 (0x81)

### E.2 头部信息处理 (0x02)
- [ ] 实现 `smota_handle_header_info_req()` - 处理头部信息请求
  - 记录 SHA-256 哈希值
  - 记录 ECDSA 签名（如启用）
  - 擦除/准备 Flash 扇区
  - 初始化解密引擎（如需）
  - 构建头部信息响应 (0x82)

---

## 模块 F: 阶段二 - 传输阶段实现

### F.1 数据块接收 (0x03)
- [ ] 实现 `smota_handle_data_block_req()` - 处理数据块请求
  - 验证偏移量是否连续
  - 解密数据（如需）
  - 计算增量 SHA-256
  - 写入 Flash
  - 更新接收进度
  - 构建数据块响应 (0x83)

### F.2 传输完成处理 (0x04)
- [ ] 实现 `smota_handle_transfer_complete_req()` - 处理传输完成请求
  - 验证总大小
  - 完成最终 SHA-256 计算
  - 验证哈希值
  - ECDSA 签名验证（如启用）
  - 构建传输完成响应 (0x84)

---

## 模块 G: 阶段三 - 升级完成阶段实现

### G.1 安装触发 (0x05)
- [ ] 实现 `smota_handle_install_req()` - 处理安装请求
  - 检查电池电量（如需）
  - 检查业务状态（是否可重启）
  - 设置安装标志位（根据模式）
    - 双 Bank 模式：设置交换位
    - 双槽位模式：设置拷贝标志
    - 单分区模式：设置恢复模式
  - 构建安装响应 (0x85)
  - 执行系统复位

### G.2 激活检查 (0x06)
- [ ] 实现 `smota_handle_activate_check_req()` - 处理激活检查请求
  - 读取当前运行的固件版本
  - 构建激活检查响应 (0x86)

---

## 模块 H: 验签模块实现 (smota_verify.c)

### H.1 SHA-256 校验
- [ ] 实现 `smota_verify_sha256_start()` - 开始 SHA-256 计算
- [ ] 实现 `smota_verify_sha256_update()` - 增量更新
- [ ] 实现 `smota_verify_sha256_final()` - 完成并验证

### H.2 ECDSA 签名验证（可选）
- [ ] 实现 `smota_verify_ecdsa()` - ECDSA-P256 签名验证
  - 使用 HAL crypto 驱动
  - 验证签名有效性

### H.3 版本验证（防回滚）
- [ ] 实现 `smota_verify_version()` - 版本比较
  - 比较新旧版本号
  - 防止回滚

---

## 模块 I: 核心控制 API (smota_core.c)

### I.1 初始化与清理
- [ ] 实现 `smota_init()` - 初始化 OTA 模块
  - 检查 HAL 是否注册
  - 初始化状态机
  - 初始化接收缓冲区
  - 读取当前固件版本

- [ ] 实现 `smota_deinit()` - 去初始化

### I.2 Poll 函数
- [ ] 实现 `smota_poll()` - 主轮询函数
  - 检查超时
  - 尝试接收数据
  - 解析帧
  - 根据命令码调用相应处理函数
  - 发送响应

### I.3 控制接口
- [ ] 实现 `smota_start()` - 启动 OTA（主动触发）
- [ ] 实现 `smota_abort()` - 中止 OTA
- [ ] 实现 `smota_get_progress()` - 获取进度
- [ ] 实现 `smota_get_state()` - 获取状态
- [ ] 实现 `smota_get_error()` - 获取错误信息

---

## 模块 J: Flash 操作封装

### J.1 Flash 写入管理
- [ ] 实现 `smota_flash_write_backup()` - 写入备份区
  - 处理跨页写入
  - 处理对齐

### J.2 Flash 擦除管理
- [ ] 实现 `smota_flash_erase_backup()` - 擦除备份区
  - 按页擦除
  - 进度回调

### J.3 固件拷贝（双槽位模式）
- [ ] 实现 `smota_flash_copy_firmware()` - 固件拷贝
  - 从备份区拷贝到应用区
  - 边拷贝边校验

---

## 模块 K: 加密支持（可选）

### K.1 AES-CTR 解密
- [ ] 实现 `smota_decrypt_init()` - 初始化解密
- [ ] 实现 `smota_decrypt_update()` - 解密数据块
- [ ] 实现 `smota_decrypt_final()` - 完成解密

### K.2 密钥派生
- [ ] 实现 `smota_kdf_derive()` - 密钥派生函数
  - HMAC-SHA256
  - 基于设备 UID

---

## 模块 L: Win32 模拟示例完善

### L.1 加密驱动实现
- [ ] 实现 OpenSSL 加密驱动
  - SHA-256
  - AES-128-CTR
  - ECDSA-P256

### L.2 测试脚本
- [ ] 完善 `test_host.py` 测试脚本
  - 完整的 OTA 流程测试
  - 错误场景模拟

### L.3 Flash 模拟
- [ ] 完善 Flash 文件模拟
  - 持久化存储
  - 模拟擦写延迟

---

## 模块 M: 头文件完善

### M.1 smota_types.h 完善
- [ ] 添加所有类型定义
- [ ] 添加枚举定义
- [ ] 添加结构体定义

### M.2 smota_state.h 完善
- [ ] 添加状态机 API 声明
- [ ] 添加状态转换宏

### M.3 smota.h 统一头文件完善
- [ ] 添加所有公共 API 声明
- [ ] 整理 include 顺序

---

## 模块 N: 错误处理与日志

### N.1 错误码定义
- [ ] 完善错误码定义
- [ ] 实现错误码转字符串函数

### N.2 调试输出
- [ ] 完善调试宏
- [ ] 添加状态日志
- [ ] 添加数据包日志

---

## 模块 O: 文档

### O.1 API 文档
- [ ] 为每个公共 API 添加注释
- [ ] 生成 Doxygen 风格文档

### O.2 移植指南
- [ ] 完善 [6.porting-guide.md](doc/6.porting-guide.md)
- [ ] 添加更多示例代码

---

## 优先级说明

### P0 - 核心功能（必须）
- 模块 B: CRC16 校验
- 模块 C: 包解析与序列化
- 模块 D: 状态机核心
- 模块 E: 握手阶段
- 模块 F: 传输阶段（不含加密）
- 模块 G: 升级完成阶段
- 模块 I: 核心 API (init/poll)

### P1 - 基础功能（重要）
- 模块 A: 数据结构定义
- 模块 H: SHA-256 校验
- 模块 J: Flash 操作封装
- 模块 L: Win32 示例
- 模块 N: 错误处理

### P2 - 可选功能
- 模块 K: 加密支持
- 模块 H: ECDSA 签名验证
- 模块 M: 头文件完善
- 模块 O: 文档

---

## 开发顺序建议

1. **第一阶段**: 数据结构 + CRC16 + 包解析框架
   - 完成 A, B, C（基础部分）

2. **第二阶段**: 状态机 + 握手阶段
   - 完成 D, E

3. **第三阶段**: 传输阶段
   - 完成 F, H(SHA-256), J

4. **第四阶段**: 升级完成阶段 + 核心API
   - 完成 G, I

5. **第五阶段**: 测试与完善
   - 完成 L, N

6. **第六阶段**: 可选功能
   - 完成 K, O

---

## 文件修改清单

| 文件 | 状态 | 说明 |
|:-----|:-----|:-----|
| `smota/smota_core/inc/smota_types.h` | 需实现 | 类型定义 |
| `smota/smota_core/inc/smota_state.h` | 需实现 | 状态机定义 |
| `smota/smota_core/inc/smota_verify.h` | 需实现 | 验签模块定义 |
| `smota/smota_core/src/smota_types.c` | 需创建 | 类型实现（如有） |
| `smota/smota_core/src/smota_state.c` | 需实现 | 状态机实现 |
| `smota/smota_core/src/smota_packet.c` | 需实现 | 包解析实现 |
| `smota/smota_core/src/smota_verify.c` | 需实现 | 验签实现 |
| `smota/smota_core/src/smota_core.c` | 需实现 | 核心API实现 |
| `smota/smota.h` | 需完善 | 统一头文件 |
| `examples/win_sim/port/smota_port.c` | 需完善 | 加密驱动实现 |
| `examples/win_sim/main.c` | 需完善 | 示例完善 |

---

**最后更新**: 2026-01-30
