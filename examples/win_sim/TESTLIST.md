# TESTLIST: win_sim OTA 测试用例设计

> **创建日期**: 2026-01-31
> **目标**: 验证 win_sim 中 OTA 逻辑的正确性

---

## 1. 测试架构

```
┌─────────────────────────────────────────────────────────────┐
│                    测试框架架构                              │
├─────────────────────────────────────────────────────────────┤
│  test_runner.c (测试套件入口)                                │
│       ↓                                                     │
│  test_cases/                                                │
│  ├── test_unit.c        # 单元测试                           │
│  ├── test_protocol.c    # 协议测试                           │
│  ├── test_flow.c        # 流程测试                           │
│  └── test_error.c       # 错误处理测试                       │
│       ↓                                                     │
│  mock/                                                           │
│  ├── mock_comm.c       # 通信模拟                            │
│  └── mock_flash.c      # Flash 模拟                          │
└─────────────────────────────────────────────────────────────┘
```

---

## 2. 测试分类

### 2.1 单元测试 (Unit Tests)

| 用例编号 | 测试目标 | 输入 | 预期结果 |
|:---------|:---------|:-----|:---------|
| U-CRC-01 | CRC16 计算 | `{0x01, 0x02, 0x03, 0x04}` | 正确 CRC 值 |
| U-CRC-02 | CRC16 验证 | 完整帧（含 CRC） | 验证通过 |
| U-CRC-03 | CRC16 错误检测 | 篡改后的帧 | 验证失败 |
| U-SHA-01 | SHA256 计算 | `"Hello smOTA"` | 正确哈希 |
| U-SHA-02 | SHA256 分块计算 | 大数据（>1KB） | 与整体计算结果一致 |
| U-VER-01 | 版本号比较 | `1.0.0` vs `1.0.1` | 允许升级 |
| U-VER-02 | 版本号回滚检测 | `1.0.1` vs `1.0.0` | 拒绝升级 |
| U-VER-03 | 版本号相等 | `1.0.0` vs `1.0.0` | 允许 |
| U-STA-01 | 状态机初始化 | 复位 | `IDLE` |
| U-STA-02 | 状态转换 HANDSHAKE | IDLE→HANDSHAKE | 成功 |
| U-STA-03 | 状态转换 HEADER_INFO | HANDSHAKE→HEADER_INFO | 成功 |
| U-STA-04 | 状态转换 TRANSFER | HEADER_INFO→TRANSFER | 成功 |
| U-STA-05 | 状态转换 COMPLETE | TRANSFER→COMPLETE | 成功 |
| U-STA-06 | 无效状态转换 | ERROR→HANDSHAKE | 失败 |
| U-FRA-01 | 帧构建 | 有效 payload | 正确帧结构 |
| U-FRA-02 | 帧解析 | 有效帧 | 正确解析 |
| U-FRA-03 | 帧解析 SOF 错误 | 错误 SOF | 解析失败 |

### 2.2 协议测试 (Protocol Tests)

| 用例编号 | 测试目标 | 测试步骤 | 预期结果 |
|:---------|:---------|:---------|:---------|
| P-HSK-01 | 握手请求处理 | 发送 0x01 帧 | 返回 0x81 响应，状态变为 HANDSHAKE |
| P-HSK-02 | 握手版本验证 | 低版本固件 | 返回错误码 |
| P-HSK-03 | 握手项目 ID 验证 | 错误项目 ID | 返回错误码 |
| P-HSK-04 | 握手空间检查 | 固件大小 > 可用空间 | 返回空间不足错误 |
| P-HDR-01 | 头部信息处理 | 发送 0x02 帧（含 SHA256） | 擦除 Flash，状态变为 HEADER_INFO |
| P-HDR-02 | 头部信息 SHA256 保存 | 32 字节哈希 | 正确保存 |
| P-DAT-01 | 单数据块处理 | 发送 0x03 帧（256 字节） | 写入 Flash，进度更新 |
| P-DAT-02 | 多数据块处理 | 分 4 次发送 0x03 帧 | 每次正确写入，状态变为 TRANSFER |
| P-DAT-03 | 数据块偏移校验 | 错误偏移 | 返回错误码，不写入 |
| P-DAT-04 | 数据块连续性 | 非连续偏移 | 拒绝写入 |
| P-CMP-01 | 传输完成处理 | 发送 0x04 帧 | 验证 SHA256，状态变为 COMPLETE |
| P-CMP-02 | 大小校验 | 错误总大小 | 返回错误码 |
| P-CMP-03 | SHA256 校验 | 正确的固件哈希 | 校验通过 |
| P-CMP-04 | SHA256 校验失败 | 篡改数据 | 校验失败 |
| P-INS-01 | 安装请求处理 | 发送 0x05 帧 | 设置安装标志，状态变为 INSTALL |
| P-ACT-01 | 激活检查请求 | 发送 0x06 帧 | 返回当前版本，状态变为 ACTIVATE |

### 2.3 流程测试 (Integration Tests)

| 用例编号 | 测试目标 | 测试步骤 | 预期结果 |
|:---------|:---------|:---------|:---------|
| F-FULL-01 | 完整 OTA 流程 | 1. 握手 2. 头部 3. 数据 4. 完成 5. 安装 | 所有状态正确转换 |
| F-FULL-02 | 完整 OTA + 小固件 | 固件大小 = 100 字节 | 一次数据传输完成 |
| F-FULL-03 | 完整 OTA + 大固件 | 固件大小 = 10KB | 分多次传输 |
| F-RES-01 | 断点续传 | 传一半后断开重连 | 从断点继续传输 |
| F-TIM-01 | 超时检测 | 发送后不继续 | 超时进入 ERROR 状态 |
| F-ABT-01 | 中止 OTA | 传输中调用 smota_abort() | 状态回到 IDLE |

### 2.4 错误处理测试 (Error Tests)

| 用例编号 | 测试目标 | 输入 | 预期结果 |
|:---------|:---------|:-----|:---------|
| E-INV-01 | 无效命令码 | 未知命令码 | 无响应，状态不变 |
| E-INV-02 | 空 payload | 空数据 | 解析失败 |
| E-INV-03 | 帧长度不匹配 | 长度字段与实际不符 | 解析失败 |
| E-STA-01 | 状态外请求 | 在 IDLE 状态发送数据块 | 返回无效状态错误 |
| E-SEQ-01 | 重复帧 | 同一偏移重发 | 拒绝（偏移不连续） |
| E-FLA-01 | Flash 写入失败 | 模拟写入错误 | 返回 Flash 错误 |

---

## 3. 测试实现方案

### 3.1 自定义轻量级测试框架

```c
/* test_framework.h */
#define TEST_ASSERT_EQUAL(expected, actual) \
    do { if ((expected) != (actual)) { \
        printf("FAIL: %s:%d\n", __FILE__, __LINE__); \
        return -1; \
    }} while(0)

#define TEST_ASSERT_TRUE(cond) \
    do { if (!(cond)) { \
        printf("FAIL: %s:%d\n", __FILE__, __LINE__); \
        return -1; \
    }} while(0)

#define RUN_TEST(name) \
    do { \
        printf("Running %s... ", #name); \
        if (name() == 0) { \
            printf("PASS\n"); \
        } else { \
            printf("FAIL\n"); \
            return -1; \
        } \
    } while(0)
```

### 3.2 Mock 实现

```c
/* mock_comm.c - 通信 Mock */
static uint8_t g_mock_rx_buffer[2048];
static uint32_t g_mock_rx_len = 0;
static uint8_t g_mock_tx_buffer[2048];
static uint32_t g_mock_tx_len = 0;

void mock_comm_set_rx_data(const uint8_t *data, uint32_t len)
{
    memcpy(g_mock_rx_buffer, data, len);
    g_mock_rx_len = len;
}

int mock_comm_get_tx_data(uint8_t *data, uint32_t *len)
{
    memcpy(data, g_mock_tx_buffer, g_mock_tx_len);
    *len = g_mock_tx_len;
    return 0;
}
```

### 3.3 测试命令

```bash
# 编译测试
cd examples/win_sim
gcc -I. -I../../smota -I../../smota/smota_core/inc \
    test_runner.c test_cases/*.c \
    ../../smota/smota_core/src/*.c \
    ../../smota/smota_hal/smota_hal.c \
    port/smota_port.c \
    ../../smota/third_party/tinycrypt/lib/source/*.c \
    -o test_runner.exe

# 运行测试
./test_runner.exe
```

---

## 4. test_host.py 增强方案

### 4.1 增强功能

```python
class SmotaHostEnhanced(SmotaHost):
    """增强版 smOTA 上位机"""

    def calculate_crc16(self, data):
        """计算 CRC16 (多项式 0x1021)"""
        crc = 0xFFFF
        for byte in data:
            crc ^= byte
            for _ in range(8):
                if crc & 0x0001:
                    crc = (crc >> 1) ^ 0x1021
                else:
                    crc >>= 1
        return crc & 0xFFFF

    def send_frame_with_crc(self, cmd, payload):
        """发送带 CRC 的帧"""
        frame = self._build_frame(cmd, payload)
        crc = self.calculate_crc16(frame)
        return frame + struct.pack('<H', crc)

    def run_full_ota_test(self, firmware_data):
        """完整 OTA 流程测试"""
        results = {}
        results['handshake'] = self._test_handshake()
        results['header'] = self._test_header_info(firmware_data)
        results['transfer'] = self._test_transfer(firmware_data)
        results['complete'] = self._test_complete()
        results['install'] = self._test_install()
        return results
```

---

## 5. 测试文件结构

```
examples/win_sim/
├── test/
│   ├── test_runner.c          # 测试入口
│   ├── test_framework.h       # 测试框架宏
│   ├── test_cases/
│   │   ├── test_crc.c         # CRC 测试
│   │   ├── test_sha256.c      # SHA256 测试
│   │   ├── test_version.c     # 版本验证测试
│   │   ├── test_state.c       # 状态机测试
│   │   ├── test_frame.c       # 帧解析测试
│   │   ├── test_handler.c     # 协议处理测试
│   │   └── test_flow.c        # 完整流程测试
│   └── mock/
│       ├── mock_comm.c        # 通信 Mock
│       └── mock_flash.c       # Flash Mock
└── TESTLIST.md                # 本文档
```

---

## 6. 测试执行顺序

```
1. 编译检查
   └─> 单元测试 (U-*) - CRC/SHA256/版本/状态机/帧解析

2. 协议测试 (P-*)
   ├─ 握手测试
   ├─ 头部信息测试
   ├─ 数据块测试
   ├─ 传输完成测试
   └─ 安装/激活测试

3. 流程测试 (F-*)
   ├─ 完整 OTA 流程
   ├─ 断点续传
   └─ 超时检测

4. 错误测试 (E-*)
   ├─ 无效输入
   ├─ 状态错误
   └─ Flash 错误
```

---

## 7. 覆盖率要求

| 模块 | 覆盖率目标 |
|:-----|:-----------|
| smota_packet.c | > 90% |
| smota_state.c | > 95% |
| smota_handler.c | > 85% |
| smota_core.c | > 80% |

---

**最后更新**: 2026-01-31
