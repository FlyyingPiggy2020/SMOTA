# TODO5: 集成测试文档

## 对应的设计文档

- 综合测试，对应所有 Worker TODO (TODO1-4)

## 测试目标

验证所有模块协同工作的正确性，进行端到端的完整 OTA 流程测试。

---

## 1. 测试范围

### 1.1 模块集成

- [ ] 协议模块 + 固件包模块
- [ ] 协议模块 + Flash 存储模块
- [ ] 协议模块 + 错误处理模块
- [ ] 所有模块集成

### 1.2 完整流程

- [ ] 握手流程
- [ ] 头部信息流程
- [ ] 数据传输流程
- [ ] 传输完成流程
- [ ] 安装流程
- [ ] 激活检查流程

---

## 2. Python 集成测试脚本

### 2.1 测试脚本位置

`examples/win_sim/test_integration_todo5.py`

### 2.2 完整 OTA 流程测试

```python
#!/usr/bin/env python3
"""
TODO5: 集成测试脚本

完整的 OTA 升级流程测试
"""

import subprocess
import struct
import sys
import time
import hashlib
import binascii

class SmotaIntegrationTest:
    def __init__(self):
        self.process = None
        self.seq = 0

    def start_device(self):
        """启动设备模拟器"""
        self.process = subprocess.Popen(
            ['./build/win_sim/win_sim.exe', '-r'],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            bufsize=0
        )
        time.sleep(0.5)  # 等待设备启动

    def stop_device(self):
        """停止设备模拟器"""
        if self.process:
            self.process.terminate()
            self.process.wait()

    def build_frame(self, cmd, payload=b''):
        """构建协议帧"""
        sof = b'smOTA'
        ver = 0x01
        frag = 0x00
        seq = self.seq % 256
        self.seq += 1
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

    def send_and_recv(self, cmd, payload=b'', timeout=2):
        """发送命令并接收响应"""
        frame = self.build_frame(cmd, payload)
        self.process.stdin.write(frame)
        self.process.stdin.flush()

        start_time = time.time()
        response = b''

        while time.time() - start_time < timeout:
            try:
                char = self.process.stdout.read(1)
                if not char:
                    break
                response += char

                # 检查是否接收到完整帧
                if len(response) >= 11:  # 帧头大小
                    # 解析长度
                    length = struct.unpack('<H', response[9:11])[0]
                    total_len = 11 + length + 2
                    if len(response) >= total_len:
                        break
            except:
                break

        return response

    def parse_response(self, response):
        """解析响应帧"""
        if len(response) < 11:
            return None, None

        header = response[:11]
        sof, ver, frag, seq, cmd, length = struct.unpack('<5sBBBBH', header)

        if len(response) < 11 + length + 2:
            return None, None

        payload = response[11:11+length]
        crc16 = struct.unpack('<H', response[11+length:11+length+2])[0]

        # 验证 CRC
        calc_crc = self.calc_crc16(header + payload)
        if calc_crc != crc16:
            return None, None

        return cmd, payload

    def test_handshake(self):
        """测试握手流程"""
        print("Test: Handshake...")

        # 构建握手请求
        payload = struct.pack('<16sBBBIHHH8x',
            b'TEST_PROJECT_123',  # project_id
            1, 0, 1,              # version: 1.0.1
            2048,                 # firmware_size
            10,                   # block_timeout
            30,                   # install_timeout
            300)                  # total_timeout

        response = self.send_and_recv(0x01, payload)
        cmd, resp_payload = self.parse_response(response)

        if cmd == 0x81:  # 握手响应
            error_code = resp_payload[0] if resp_payload else 0xFF
            if error_code == 0:
                print("  PASS: Handshake successful")
                return True
            else:
                print(f"  FAIL: Handshake failed with error {error_code}")
                return False
        else:
            print("  FAIL: Invalid response")
            return False

    def test_header_info(self):
        """测试头部信息流程"""
        print("Test: Header Info...")

        # 准备测试固件信息
        test_data = b'A' * 2048
        sha256_hash = hashlib.sha256(test_data).digest()

        # 构建头部信息请求
        payload = struct.pack('<32sIBBBxBH8x',
            sha256_hash,
            len(test_data),
            1, 0, 1,              # version
            0x04,                 # flags: signed
            0x1234)               # header_crc

        response = self.send_and_recv(0x02, payload)
        cmd, resp_payload = self.parse_response(response)

        if cmd == 0x82:  # 头部信息响应
            error_code = resp_payload[0] if resp_payload else 0xFF
            if error_code == 0:
                print("  PASS: Header info successful")
                return True
            else:
                print(f"  FAIL: Header info failed with error {error_code}")
                return False
        else:
            print("  FAIL: Invalid response")
            return False

    def test_data_transfer(self):
        """测试数据传输流程"""
        print("Test: Data Transfer...")

        # 发送多个数据块
        block_size = 256
        total_size = 1024
        offset = 0

        while offset < total_size:
            data = b'D' * block_size
            block_crc = self.calc_crc16(data)

            payload = struct.pack('<IHH',
                offset,
                block_size,
                block_crc) + data

            response = self.send_and_recv(0x03, payload)
            cmd, resp_payload = self.parse_response(response)

            if cmd != 0x83:
                print(f"  FAIL: Invalid response at offset {offset}")
                return False

            error_code = resp_payload[0] if resp_payload else 0xFF
            if error_code != 0:
                print(f"  FAIL: Data block failed at offset {offset}")
                return False

            offset += block_size

        print("  PASS: Data transfer successful")
        return True

    def test_transfer_complete(self):
        """测试传输完成流程"""
        print("Test: Transfer Complete...")

        payload = struct.pack('<I16x', 1024)  # total_size

        response = self.send_and_recv(0x04, payload)
        cmd, resp_payload = self.parse_response(response)

        if cmd == 0x84:
            error_code = resp_payload[0] if resp_payload else 0xFF
            verify_result = resp_payload[1] if len(resp_payload) > 1 else 0xFF

            if error_code == 0 and verify_result == 0:
                print("  PASS: Transfer complete successful")
                return True
            else:
                print(f"  FAIL: Transfer complete failed (err={error_code}, verify={verify_result})")
                return False
        else:
            print("  FAIL: Invalid response")
            return False

    def test_install(self):
        """测试安装流程"""
        print("Test: Install...")

        payload = struct.pack('<B15x', 0)  # force_install = 0

        response = self.send_and_recv(0x05, payload)
        cmd, resp_payload = self.parse_response(response)

        if cmd == 0x85:
            error_code = resp_payload[0] if resp_payload else 0xFF

            if error_code == 0:
                print("  PASS: Install successful")
                return True
            else:
                print(f"  FAIL: Install failed with error {error_code}")
                return False
        else:
            print("  FAIL: Invalid response")
            return False

    def test_activate_check(self):
        """测试激活检查流程"""
        print("Test: Activate Check...")

        payload = struct.pack('<16x', 0)

        response = self.send_and_recv(0x06, payload)
        cmd, resp_payload = self.parse_response(response)

        if cmd == 0x86:
            error_code = resp_payload[0] if resp_payload else 0xFF

            if error_code == 0:
                # 读取版本信息
                if len(resp_payload) >= 4:
                    major = resp_payload[1]
                    minor = resp_payload[2]
                    patch = resp_payload[3]
                    status = resp_payload[4] if len(resp_payload) > 4 else 0

                    print(f"  PASS: Activate check successful (version: {major}.{minor}.{patch}, status: {status})")
                    return True

            print(f"  FAIL: Activate check failed with error {error_code}")
            return False
        else:
            print("  FAIL: Invalid response")
            return False

    def test_full_ota_flow(self):
        """测试完整 OTA 流程"""
        print("\n" + "=" * 50)
        print("Full OTA Flow Test")
        print("=" * 50)

        results = []

        # 按顺序执行所有步骤
        results.append(self.test_handshake())
        results.append(self.test_header_info())
        results.append(self.test_data_transfer())
        results.append(self.test_transfer_complete())
        results.append(self.test_install())
        results.append(self.test_activate_check())

        print("\n" + "=" * 50)
        if all(results):
            print("Full OTA Flow: PASSED")
        else:
            print(f"Full OTA Flow: FAILED ({sum(results)}/{len(results)} passed)")
        print("=" * 50)

        return all(results)

    def run_all_tests(self):
        """运行所有测试"""
        print("=" * 50)
        print("SMOTA Integration Test - TODO5")
        print("=" * 50)

        self.start_device()

        try:
            return self.test_full_ota_flow()

        finally:
            self.stop_device()

if __name__ == '__main__':
    test = SmotaIntegrationTest()
    success = test.run_all_tests()
    sys.exit(0 if success else 1)
```

---

## 3. 测试场景

### 3.1 正常流程

- [ ] 完整升级流程
- [ ] 断点续传
- [ ] 版本升级

### 3.2 异常流程

- [ ] 传输中断后恢复
- [ ] CRC 错误处理
- [ ] 超时处理
- [ ] 版本回滚拒绝

### 3.3 边界条件

- [ ] 最小固件大小
- [ ] 最大固件大小
- [ ] 空固件处理

---

## 4. 编译验证

### 4.1 验证步骤

```bash
cd examples/win_sim
./build.sh
```

### 4.2 预期结果

- [ ] 编译无 error
- [ ] 所有模块链接成功
- [ ] 可执行文件生成

---

## 5. 问题记录模板

将发现的问题记录到 `notes/Worker/ISSUE5.md`

```markdown
# ISSUE5: 集成测试问题记录

## 问题列表

### 🔴 严重问题

#### 1. 状态机不一致
- **模块**: 协议 + 状态机
- **描述**: 握手成功后状态未更新

### 🟡 中等问题

#### 1. 超时时间不合理
- **模块**: 协议处理
- **描述**: 数据块超时时间过短

### 🟢 轻微问题

#### 1. 日志输出过多
- **模块**: 错误处理
```

---

## 6. 测试通过标准

- [ ] 完整 OTA 流程测试通过
- [ ] 所有异常场景测试通过
- [ ] 所有边界条件测试通过
- [ ] 无严重问题记录
- [ ] 中等问题数量 < 3

---

## 7. 测试执行命令

```bash
# 编译
cd examples/win_sim
./build.sh

# 运行集成测试
python test_integration_todo5.py
```

---

**文档版本**: 1.0
**创建日期**: 2026-03-03
**对应 Worker TODO**: 综合测试
**测试执行者**: Tester
