# FBR2101_XJ 编码规范

## 1. 文件结构

### 1.1 头文件
```c
/*---------- includes ----------*/
/*---------- macro ----------*/
/*---------- type define ----------*/
/*---------- variable prototype ----------*/
/*---------- function prototype ----------*/
/*---------- end of file ----------*/
```

### 1.2 源文件
```c
/*---------- includes ----------*/
/*---------- macro ----------*/
/*---------- type define ----------*/
/*---------- variable prototype ----------*/
/*---------- function prototype ----------*/
/*---------- variable ----------*/
/*---------- function ----------*/
/*---------- end of file ----------*/
```

### 1.3 文件头 (必须)
```c
/*
 * Copyright (c) 2025 by Lu Xianfan.
 * @FilePath     : filename.c
 * @Author       : lxf
 * @Date         : 2025-12-24 10:00:00
 * @LastEditors  : lxf_zjnb@qq.com
 * @LastEditTime : 2025-12-24 10:00:00
 * @Brief        : 模块功能说明
 */
```

### 1.4 驱动类文件扩展标签 (可选)
| 标签 | 用途 |
|------|------|
| `@hardware` | 硬件接口说明 |
| `@usage` | 使用示例 (@code ... @endcode) |
| `@features` | 功能特性列表 |
| `@warning` | 重要警告 |
| `@note` | 使用注意事项 |
| `@register_map` | 寄存器映射表 |
| `@performance` | 性能对比表 |
| `@transmit_flow` | 发送流程 |
| `@receive_flow` | 接收流程 |

注释的排版参考 dev_uart.c

## 2. 命名规范

### 2.1 文件
- 小写下划线: `app_motor.c`, `device.h`

### 2.2 函数
- 公开API: `device_open()`, `app_motor_init()`
- 私有函数: `static void motor_update(void)`

### 2.3 变量
- 局部/全局: `uint32_t timeout_ms`
- 常量: `#define MAX_BUFFER_SIZE 256`

### 2.4 结构体 (重要: 按照POSIX要求，一律用struct torque_sensor形式，除非该结构体成员不打算暴露给用户，可以使用typedef，比如device_t)
```c
// ✅ 正确
struct torque_sensor {
    float value;
};

// ❌ 错误
typedef struct { ... } torque_sensor_t;
```

## 3. 注释规范

### 3.1 函数注释
```c
/**
 * @brief  功能说明
 * @param  xxx: 参数说明
 * @return 0=成功, <0=失败
 */
```

### 3.2 中文注释
- 项目必须使用UTF-8中文注释
- 关键接口用英文@brief

## 4. 格式规范

- **缩进**: 4空格 (不用Tab)
- **大括号**: 函数另起一行, if/while不换行
- **空行**: 函数/段落间空1行, 最多1连续空行
- **指针**: `uint8_t *ptr` (*靠近类型)

## 5. 协议模块独立化规范

### 5.1 适用场景
当应用层模块中包含特定硬件的通信协议时，应将协议层独立到 `components/fp-sdk/packages/` 目录。

### 5.2 判断标准
满足以下任一条件时应独立协议层:
- 协议具有通用性，可用于其他项目
- 协议代码超过50行
- 包含复杂的数据解析或状态机逻辑

### 5.3 文件组织
```
components/fp-sdk/packages/<模块名>/
├── <模块名>_sensor.h          # 协议层头文件
├── <模块名>_sensor.c          # 协议层实现
└── readme.md                  # 可选: 协议说明文档
```

### 5.4 命名规范
- 模块名: 小写下划线 (如 `mgt_abs`, `sentong_torque`)
- 结构体: `struct <模块名>_sensor`
- 函数: `<模块名>_sensor_<功能>`

### 5.5 API 设计原则
- 协议层只负责通信和数据解析，不包含业务逻辑
- 协议层不直接调用 `device_open`，由应用层传入设备句柄
- 协议层不使用静态全局变量，支持多实例
- 保持简单，不添加超时检测、错误重试等复杂功能（除非用户要求）

### 5.6 应用层封装
- 应用层保留管理封装功能（设备管理、轮询调度）
- 应用层结构体包含协议层对象和接收缓冲区
- 应用层调用协议层 API 完成通信

### 5.7 实施步骤
1. 在 `packages/` 下创建协议模块目录
2. 实现协议层头文件和源文件
3. 修改应用层，保留管理封装功能
4. 配置编译器 include 路径（如需要）
5. 更新 `CLAUDE.md` 文档

### 5.8 示例：MGT-ABS 位置传感器
- 协议层: `packages/mgt_abs/mgt_abs_sensor.h/c`
- 应用层: `app/app_position_sensor.h/c`
- 协议命令: 读位置 `0x02`
- 响应格式: `[头部][长度][位置低][位置高]...` (小端)
