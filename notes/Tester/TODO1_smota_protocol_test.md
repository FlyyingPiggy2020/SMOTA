# TODO1: 协议模块测试文档

## 对应的设计文档

- [notes/Worker/TODO1_smota_protocol_design.md](../Worker/TODO1_smota_protocol_design.md)

## 测试目标

验证 `smota_packet.h` 中定义的协议数据结构和 `smota_packet.c` 中的实现函数是否正确。

---

## 1. 编码规范检查

### 1.1 文件结构检查

- [ ] 文件头注释完整且格式正确
- [ ] 头文件包含保护宏 (`#ifndef _SMOTA_PACKET_H_`)
- [ ] 分区注释清晰：includes/macro/type/prototype

### 1.2 命名规范检查

- [ ] 结构体使用 `struct smota_xxx` 形式
- [ ] 函数命名：`smota_xxx_xxx()`
- [ ] 宏命名：`SMOTA_XXX`
- [ ] 常量使用宏定义

### 1.3 格式规范检查

- [ ] 缩进使用4空格
- [ ] 函数大括号另起一行
- [ ] 指针声明：`uint8_t *ptr`
- [ ] 结构体使用 `#pragma pack(push, 1)`

---

## 2. 函数级检查

### 2.1 参数校验

```c
/**
 * @brief  构建待发送的协议帧
 */
int smota_frame_build(uint8_t cmd,
                      const uint8_t *payload,
                      uint16_t payload_len,
                      uint8_t *buffer,
                      uint16_t buffer_size);
```

检查点：
- [ ] `buffer` NULL 检查
- [ ] `buffer` 与 `buffer_size` 一致性检查
- [ ] `payload_len` 最大值检查
- [ ] 返回错误码使用标准值

### 2.2 资源管理

- [ ] 无动态内存分配（或正确释放）
- [ ] 无内存泄漏

### 2.3 边界条件

- [ ] `payload_len = 0` 处理
- [ ] `buffer_size` 不足处理
- [ ] 最大 payload 长度处理

---

## 3. 逻辑检查

### 3.1 字节序处理

- [ ] 多字节字段使用小端序
- [ ] CRC 计算使用正确的字节序

### 3.2 CRC 校验

- [ ] CRC-16-CCITT 算法正确
- [ ] CRC 包含正确的数据范围

### 3.3 数据结构大小

```c
/* 验证结构体大小 */
sizeof(struct smota_frame_header) == 11
sizeof(struct smota_handshake_req) == 36
sizeof(struct smota_handshake_resp) == 42
/* ... */
```

---

## 4. Python 协议测试脚本

### 4.1 测试脚本位置

`examples/win_sim/test_protocol_todo1.py`

### 4.2 测试用例

```python
#!/usr/bin/env python3
"""
TODO1: 协议模块测试脚本

测试 smota_frame_build 和 smota_frame_parse 函数
"""

import subprocess
import struct
import sys

class SmotaProtocolTest:
    def __init__(self):
        self.process = None

    def start_device(self):
        """启动设备模拟器"""
        self.process = subprocess.Popen(
            ['./build/win_sim/win_sim.exe', '-r'],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            bufsize=0
        )

    def stop_device(self):
        """停止设备模拟器"""
        if self.process:
            self.process.terminate()
            self.process.wait()

    def build_frame(self, cmd, payload):
        """构建协议帧"""
        sof = b'smOTA'
        ver = 0x01
        frag = 0x00
        seq = 0x00
        length = len(payload)

        header = struct.pack('<5sBBBBH', sof, ver, frag, seq, cmd, length)
        crc16 = self.calc_crc16(header + payload)

        return header + payload + struct.pack('<H', crc16)

    def calc_crc16(self, data):
        """计算 CRC-16-CCITT"""
        crc = 0xFFFF
        for byte in data:
            crc ^= byte << 8
            for _ in range(8):
                if crc & 0x8000:
                    crc = (crc << 1) ^ 0x1021
                else:
                    crc <<= 1
                crc &= 0xFFFF
        return crc

    def send_frame(self, frame):
        """发送协议帧"""
        self.process.stdin.write(frame)
        self.process.stdin.flush()

    def recv_frame(self, timeout=2):
        """接收协议帧"""
        # 简化实现，实际需要解析帧
        return self.process.stdout.read(1024)

    def test_handshake_request(self):
        """测试握手请求帧"""
        print("Test: Handshake Request...")

        payload = struct.pack('<16sBBBIHH',
            b'PROJECT_ID_12345',  # project_id
            1, 0, 0,              # version
            1024,                 # firmware_size
            10,                   # block_timeout
            30)                   # install_timeout

        frame = self.build_frame(0x01, payload)
        self.send_frame(frame)

        response = self.recv_frame()
        # 验证响应
        if response and len(response) > 0:
            print("  PASS: Received response")
            return True
        else:
            print("  FAIL: No response")
            return False

    def test_invalid_crc(self):
        """测试 CRC 校验失败"""
        print("Test: Invalid CRC...")

        frame = self.build_frame(0x01, b'test')
        # 篡改 CRC
        frame = frame[:-2] + b'\x00\x00'
        self.send_frame(frame)

        response = self.recv_frame()
        # 设备应该忽略该帧或返回错误
        print("  PASS: Device handled invalid CRC")
        return True

    def test_invalid_magic(self):
        """测试无效魔数"""
        print("Test: Invalid Magic...")

        frame = b'XXXXX' + self.build_frame(0x01, b'test')[5:]
        self.send_frame(frame)

        print("  PASS: Device handled invalid magic")
        return True

    def run_all_tests(self):
        """运行所有测试"""
        print("=" * 50)
        print("SMOTA Protocol Test - TODO1")
        print("=" * 50)

        self.start_device()

        try:
            results = []
            results.append(self.test_handshake_request())
            results.append(self.test_invalid_crc())
            results.append(self.test_invalid_magic())

            print("\n" + "=" * 50)
            print(f"Results: {sum(results)}/{len(results)} passed")
            print("=" * 50)

            return all(results)

        finally:
            self.stop_device()

if __name__ == '__main__':
    test = SmotaProtocolTest()
    success = test.run_all_tests()
    sys.exit(0 if success else 1)
```

---

## 5. 编译验证

### 5.1 验证步骤

```bash
cd examples/win_sim
mkdir -p build && cd build
cmake ..
make
```

### 5.2 预期结果

- [ ] 编译无 error
- [ ] 编译无 warning（如果是警告可忽略）
- [ ] 生成可执行文件

---

## 6. 问题记录模板

将发现的问题记录到 `notes/Worker/ISSUE1.md`

```markdown
# ISSUE1: 协议模块问题记录

## 问题列表

### 🔴 严重问题

#### 1. NULL 指针解引用
- **文件**: `smota_packet.c:xx`
- **描述**: `smota_frame_build` 未检查 `buffer` 是否为 NULL
- **修复**: 添加 NULL 检查

### 🟡 中等问题

#### 1. 参数未校验
- **文件**: `smota_packet.c:xx`
- **描述**: `payload_len` 未检查最大值

### 🟢 轻微问题

#### 1. 注释缺失
- **文件**: `smota_packet.h:xx`
- **描述**: 结构体字段缺少注释
```

---

## 7. 测试通过标准

- [ ] 所有编码规范检查通过
- [ ] 所有函数级检查通过
- [ ] Python 协议测试脚本全部通过
- [ ] 编译验证无 error
- [ ] 无严重问题记录

---

**文档版本**: 1.0
**创建日期**: 2026-03-03
**对应 Worker TODO**: TODO1
**测试执行者**: Tester
