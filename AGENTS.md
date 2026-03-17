# SMOTA 编码规范

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
