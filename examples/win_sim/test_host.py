#!/usr/bin/env python3
"""
smOTA 简单上位机验证示例
通过 stdin/stdout 与 win_sim.exe 通信
"""

import subprocess
import threading
import queue
import struct
import time


class SmotaHost:
    """smOTA 上位机简单实现"""

    # 帧起始符
    SOF = b"smOTA"

    # 命令码
    CMD_HANDSHAKE = 0x01
    CMD_HANDSHAKE_RESP = 0x81

    def __init__(self, exe_path="./build/win_sim.exe"):
        self.proc = None
        self.exe_path = exe_path
        self.rx_queue = queue.Queue()
        self.running = False
        self.seq = 0

    def start(self):
        """启动模拟器"""
        self.proc = subprocess.Popen(
            [self.exe_path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            bufsize=0,
        )
        self.running = True

        # 启动接收线程
        self.rx_thread = threading.Thread(target=self._rx_loop, daemon=True)
        self.rx_thread.start()
        print(f"[HOST] Simulator started: {self.exe_path}")

    def stop(self):
        """停止模拟器"""
        self.running = False
        if self.proc:
            self.proc.stdin.close()
            self.proc.terminate()
            self.proc.wait()
        print("[HOST] Simulator stopped")

    def send_frame(self, cmd, payload=b""):
        """发送 smOTA 帧"""
        # 构造帧头: sof(5) + ver(1) + frag(1) + seq(2) + cmd(1) + length(2)
        header = struct.pack(
            "<5sBBHBH",
            self.SOF,           # sof[5]
            0x00,              # ver
            0x00,              # frag
            self.seq,          # seq
            cmd,               # cmd
            len(payload)       # length
        )

        frame = header + payload
        # TODO: 添加 CRC16 (简单验证可跳过)

        self.proc.stdin.write(frame)
        self.proc.stdin.flush()

        print(f"[TX] CMD=0x{cmd:02X}, SEQ={self.seq}, LEN={len(payload)}")
        self.seq = (self.seq + 1) % 65536

    def receive_frame(self, timeout=2.0):
        """接收 smOTA 帧"""
        try:
            data = self.rx_queue.get(timeout=timeout)
            return self._parse_frame(data)
        except queue.Empty:
            return None

    def _parse_frame(self, data):
        """解析接收到的帧"""
        if len(data) < 11:  # 最小帧头长度
            print(f"[RX] Frame too short: {len(data)}")
            return None

        sof, ver, frag, seq, cmd, length = struct.unpack("<5sBBHBH", data[:11])

        if sof != self.SOF:
            print(f"[RX] Invalid SOF: {sof}")
            return None

        payload = data[11:11+length] if length > 0 else b""

        print(f"[RX] CMD=0x{cmd:02X}, SEQ={seq}, LEN={length}")

        return {
            "cmd": cmd,
            "seq": seq,
            "payload": payload
        }

    def _rx_loop(self):
        """接收循环"""
        while self.running and self.proc and not self.proc.stdout.closed:
            try:
                # 读取帧头
                header = self.proc.stdout.read(11)
                if not header or len(header) < 11:
                    break

                # 解析长度
                length = struct.unpack("<H", header[9:11])[0]

                # 读取 payload
                payload = b""
                if length > 0:
                    payload = self.proc.stdout.read(length)

                self.rx_queue.put(header + payload)

            except Exception as e:
                print(f"[RX] Error: {e}")
                break

    def send_handshake(self):
        """发送握手请求"""
        # 握手请求 payload
        payload = struct.pack(
            "<BBB16sHHHHI",
            1, 0, 0,           # version: 1.0.0
            b"TEST_PROJECT_123",  # project_id (16 bytes)
            5000,              # block_timeout
            30000,             # check_timeout
            60000,             # install_timeout
            300000             # total_timeout
        )

        self.send_frame(self.CMD_HANDSHAKE, payload)


def main():
    """主函数"""
    host = SmotaHost()

    try:
        host.start()
        time.sleep(0.5)  # 等待模拟器初始化

        print("\n=== Test 1: Send Handshake ===")
        host.send_handshake()

        print("\n=== Waiting for response ===")
        resp = host.receive_frame(timeout=5.0)

        if resp:
            print(f"Received response: CMD=0x{resp['cmd']:02X}")
            if resp['cmd'] == host.CMD_HANDSHAKE_RESP:
                print("Handshake success!")
                # 解析 payload
                if len(resp['payload']) >= 4:
                    error_code = struct.unpack("<I", resp['payload'][:4])[0]
                    print(f"Error code: 0x{error_code:08X}")
        else:
            print("No response received")

        print("\n=== Test completed, keeping running ===")
        print("Press Ctrl+C to exit...")

        while True:
            time.sleep(1)

    except KeyboardInterrupt:
        host.stop()


if __name__ == "__main__":
    main()
