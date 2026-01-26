# smOTA Windows 模拟环境

> **smOTA = Secure MCU OTA** - 安全的微控制器 OTA 升级方案

## 状态

> **开发中** - 本环境用于配置 Windows 编译环境，作为未来开发 smOTA 库时的快速验证平台。

> **AI 辅助开发** - 本项目起源于测试 AI 辅助编程能力，由人类开发者与 AI 协作完成。AI 负责代码生成、文档编写和方案建议，人类开发者负责需求定义、代码审查和决策把控。

## 说明

本示例旨在 Windows 平台上搭建一个模拟开发环境，通过文件模拟 Flash 存储，便于在没有嵌入式开发板的情况下开发和验证 smOTA 库功能。

## 规划特性

- **文件模拟 Flash** - 使用 `.bin` 文件模拟 Flash 分区
- **支持三种模式** - 双 Bank、双槽位、单分区模式切换
- **完整 OTA 流程** - 下载、验证、激活全流程演示
- **加密算法** - 使用 OpenSSL 实现 SHA-256、AES、HMAC

## 目录结构

```
win_sim/
├── CMakeLists.txt      # CMake 构建配置
├── build.sh            # 编译脚本
├── README.md           # 本文档
├── main.c              # 主程序（待完善）
├── smota_user_config.h # 用户配置头文件
├── keys/               # 测试用密钥文件
│   ├── aes_master_key.c
│   ├── ecdsa_private_key.pem
│   └── ecdsa_public_key.c
├── flash/              # Flash 模拟文件目录（运行时生成）
└── build/              # 构建输出目录
```

## 环境要求

### 必需

- CMake 3.15+
- C 编译器（推荐 MinGW-w64）
- Bash（用于执行 build.sh，Git Bash / MSYS2 / WSL 均可）

### 推荐（使用 Scoop 安装）

```bash
# 安装 CMake
scoop install cmake

# 安装 MinGW-w64
scoop install mingw-w64

# 安装 Ninja（可选，构建更快）
scoop install ninja
```

## 快速开始

### 方法一：使用 build.sh（推荐）

```bash
cd examples/win_sim

# 默认编译
./build.sh
```



## 运行

编译完成后，可执行文件位于 `build/` 目录：

```bash
# 运行程序
./build/win_sim.exe

# 指定固件文件
./build/win_sim.exe my_firmware.bin
```

## 密钥管理

### 生成生产密钥

使用项目根目录的 `scripts/keygen.py` 生成独立的密钥：

```bash
# 当前目录为examples\win_sim
# 安装依赖
pip install cryptography

# 生成所有密钥（默认输出到 keys/ 目录）
python scripts/keygen.py
```

### 输出文件

运行 `keygen.py` 后会生成以下文件：

| 文件 | 说明 |
|:-----|:-----|
| `ecdsa_private_key.pem` | ECDSA 私钥（PEM 格式，用于固件签名） |
| `ecdsa_public_key.bin` | ECDSA 公钥（二进制格式） |
| `ecdsa_public_key.c` | ECDSA 公钥（C 数组格式，嵌入固件） |
| `aes_master_key.bin` | AES 主密钥（二进制格式） |
| `aes_master_key.c` | AES 主密钥（C 数组格式，嵌入固件） |
| `smota_keys.c` | 统一密钥接口文件（需 `--c-file` 参数） |

### 安全提醒

1. **私钥保护** - `ecdsa_private_key.pem` 必须妥善保管，不可泄露
2. **不要提交** - 密钥文件不应提交到 Git 仓库，已在 `.gitignore` 中排除
3. **MCU 保护** - 生产环境建议启用 MCU 的读保护（RDP）

## 待完成功能

- [ ] 完整的 OTA 状态机实现
- [ ] Flash 模拟读写逻辑
- [ ] 固件包加载和解析
- [ ] ECDSA 签名验证集成
- [ ] AES 解密传输模拟
- [ ] 版本比较和防回滚
- [ ] 错误处理和日志输出
- [ ] 测试固件生成工具

## 开发说明

本环境主要用于：

1. **快速验证** - 在没有硬件的情况下测试代码逻辑
2. **调试方便** - 利用 VSCode / Visual Studio 调试器进行单步调试
3. **单元测试** - 验证各个模块的功能正确性
4. **性能分析** - 测量加密、Hash 计算的耗时

## 相关文档

- [项目需求](../../doc/0.requirements.md)
- [项目结构](../../doc/1.project-structure.md)
- [配置参考](../../doc/2.config-reference.md)
- [密钥管理](../../doc/3.key-management.md)
- [OTA 协议](../../doc/4.ota-protocol.md)
