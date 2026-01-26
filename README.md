# smOTA - 通用 MCU 安全升级库

> **Secure MCU OTA** - 轻量级、高度可靠且安全的 MCU 固件升级库

## 名称含义

**smOTA** = **Secure MCU OTA**

- **S** - **Secure**（安全）- 强调加密、签名验证等安全特性
- **M** - **MCU**（微控制器）- 专为嵌入式/MCU 设计
- **OTA** - **Over-The-Air**（空中升级）

> **开发中** - 项目正在积极开发中，目前提供 Windows 模拟环境用于快速验证。

> **AI 辅助开发** - 本项目起源于测试 AI 辅助编程能力，由人类开发者与 AI 协作完成。AI 负责代码生成、文档编写和方案建议，人类开发者负责需求定义、代码审查和决策把控。

## 特性

- **跨平台** - 支持 ARM Cortex-M、RISC-V 等 MCU 架构
- **三种升级模式** - 双 Bank 交换、双槽位搬运、单分区覆盖
- **五大可靠性** - 运行、内容、来源、过程、版本可靠性
- **高安全性** - ECDSA-P256 签名验证、AES-128 加密传输
- **易集成** - 统一头文件，HAL 接口隔离硬件差异

## 快速开始

### 1. 复制 smota 文件夹

将 `smota/` 文件夹复制到你的项目：

```
YourProject/
├── src/
│   └── main.c
└── smota/                    # 复制整个 smota 文件夹
    ├── smota.h               # 统一头文件
    ├── smota_core/
    ├── smota_hal/
    └── ...
```

### 2. 配置编译器

添加头文件路径：
- `smota`
- `smota/smota_core/inc`
- `smota/smota_hal/inc`

添加源文件：
- `smota/smota_core/src/*.c`
- 对应平台的 HAL 实现文件

### 3. 调用 API

```c
#include "smota.h"

int main(void) {
    // 初始化
    OTA_Init(&my_hal, work_buf, sizeof(work_buf));

    // 使用 OTA 功能...
}
```

## 文档

| 文档 | 说明 |
|:-----|:-----|
| [doc/0.requirements.md](doc/0.requirements.md) | 需求规格书 |
| [doc/1.project-structure.md](doc/1.project-structure.md) | 项目结构与集成指南 |
| [doc/2.config-reference.md](doc/2.config-reference.md) | 配置宏参考 |
| [doc/3.key-management.md](doc/3.key-management.md) | 密钥管理方案 |
| [doc/4.ota-protocol.md](doc/4.ota-protocol.md) | OTA 协议规范 |

## 示例

- **[examples/win_sim/](examples/win_sim/)** - Windows 模拟示例

  在 Windows 平台通过文件模拟 Flash 存储，无需开发板即可验证 OTA 功能。

## 开发进度

项目目前处于开发阶段，详细进度请参考 [examples/win_sim/README.md#待完成功能](examples/win_sim/README.md#待完成功能)。

主要待完成功能：

- [ ] 完整的 OTA 状态机实现
- [ ] Flash 模拟读写逻辑
- [ ] 固件包加载和解析
- [ ] ECDSA 签名验证集成
- [ ] AES 解密传输模拟
- [ ] 版本比较和防回滚
- [ ] 错误处理和日志输出
- [ ] 测试固件生成工具

## 工具

- **[scripts/keygen.py](scripts/keygen.py)** - 密钥生成工具

  ```bash
  # 生成 ECDSA-P256 密钥对（用于固件签名）
  python scripts/keygen.py --ecdsa
  
  # 生成 AES-128 主密钥（用于固件加密）
  python scripts/keygen.py --aes
  ```

## 许可证

本项目采用 [LICENSE](LICENSE) 开源许可证。
