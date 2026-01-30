# TODO6: 模块 H 实现完成记录

> **创建日期**: 2026-01-30
> **完成内容**: SHA-256 验签模块、版本验证

---

## 完成项

### H.1 SHA-256 校验 ✅
- [x] `smota_sha256_start()` - 开始 SHA-256 计算
- [x] `smota_sha256_update()` - 增量更新
- [x] `smota_sha256_final()` - 完成并验证
- [x] `smota_sha256_compute()` - 快速计算（一次性）

### H.2 辅助验证函数 ✅
- [x] `smota_verify_hash_equal()` - 比较两个哈希值
- [x] `smota_verify_version()` - 版本比较（防回滚）

---

## 架构说明

```
smota_verify (上层API)
       ↓
   HAL crypto driver (底层实现)
       ↓
  sha256_init/update/final
```

**设计原则**：
- SHA-256 由 HAL crypto 驱动实现
- smota_verify 提供 HAL 抽象封装
- 支持分块增量计算

---

## 修改文件清单

| 文件 | 状态 | 说明 |
|:-----|:-----|:-----|
| `smota/smota_core/inc/smota_verify.h` | 已修改 | 添加函数声明、结构体定义 |
| `smota/smota_core/src/smota_verify.c` | 已修改 | 添加验签实现 |

---

## API 接口

### smota_sha256_compute（一次性计算）
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

### smota_verify_version（防回滚）
```c
/**
 * @brief       验证版本号（防回滚）
 * @param[in]   current_version: 当前版本号[major, minor, patch]
 * @param[in]   new_version: 新版本号[major, minor, patch]
 * @return      true=允许升级, false=拒绝
 */
bool smota_verify_version(const uint8_t current_version[3], const uint8_t new_version[3]);
```

---

## 编译验证

```bash
# Windows (MinGW/Clang)
mkdir build && cd build
cmake ..
cmake --build .
```

---

## 当前进度

| 模块 | 状态 |
|:-----|:-----|
| A (数据类型) | ✅ 完成 |
| B (CRC16) | ✅ 完成 |
| C (包解析) | ✅ 完成 |
| D (状态机) | ✅ 完成 |
| E (握手) | ✅ 完成 |
| F (传输) | ✅ 完成 |
| G (安装) | ✅ 完成 |
| H (验签) | ✅ 完成 |
| I (核心API) | ✅ 完成 |
| J (Flash) | ⏳ 待实现 |
| L (Win32示例) | ⏳ 待实现 |

---

**完成时间**: 2026-01-30
