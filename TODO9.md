# TODO9: 编译错误修复记录

> **创建日期**: 2026-01-31
> **完成内容**: 修复 win_sim 编译错误

---

## 问题描述

运行 `build.sh` 编译 win_sim 示例时出现以下错误：

```
error: implicit declaration of function 'smota_sha256_compute'
error: implicit declaration of function 'Sleep'
```

---

## 错误分析

### 错误 1: `smota_sha256_compute` 函数未声明

- **位置**: [main.c:128](examples/win_sim/main.c#L128)
- **原因**: `smota_sha256_compute` 函数在 `smota_verify.c` 中已实现，但 `smota_verify.h` 头文件中缺少函数声明
- **影响**: 调用该函数的代码无法编译通过

### 错误 2: `Sleep` 函数未声明

- **位置**: [main.c:207](examples/win_sim/main.c#L207)
- **原因**: `main.c` 缺少 Windows 平台的头文件包含
- **影响**: Windows 平台下 `Sleep()` 函数未定义

---

## 修复方案

### 修复 1: 添加 `smota_sha256_compute` 函数声明

**文件**: [smota_verify.h](smota/smota_core/inc/smota_verify.h)

```c
/**
 * @brief       快速计算数据的 SHA-256 哈希
 * @param[in]   data: 待计算数据
 * @param[in]   size: 数据长度
 * @param[out]  hash: 输出哈希值（32字节）
 * @return      0=成功, <0=失败
 */
int smota_sha256_compute(const uint8_t *data, uint32_t size, uint8_t hash[32]);
```

### 修复 2: 添加平台条件编译的头文件

**文件**: [main.c](examples/win_sim/main.c)

```c
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#include <unistd.h>
#endif
```

---

## 编译验证

```bash
cd examples/win_sim
bash build.sh
```

**结果**: 编译成功，仅有警告（未使用的参数和变量）

---

## 修改文件清单

| 文件 | 修改内容 |
|:-----|:---------|
| `smota/smota_core/inc/smota_verify.h` | 添加 `smota_sha256_compute` 函数声明 |
| `examples/win_sim/main.c` | 添加平台条件编译的头文件 |

---

**完成时间**: 2026-01-31
