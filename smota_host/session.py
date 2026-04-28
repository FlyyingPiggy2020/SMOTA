from __future__ import annotations

import hashlib
import threading
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Callable

from .protocol import (
    QUERY_VERSION_REQ,
    CMD_DATA_BLOCK,
    CMD_DATA_BLOCK_RESP,
    CMD_DATA_COMPLETE,
    CMD_DATA_COMPLETE_RESP,
    CMD_HANDSHAKE,
    CMD_HANDSHAKE_RESP,
    CMD_HEADER_INFO,
    CMD_HEADER_INFO_RESP,
    CMD_INSTALL,
    CMD_INSTALL_RESP,
    CMD_QUERY_VERSION,
    CMD_QUERY_VERSION_RESP,
    DATA_BLOCK_REQ,
    DATA_BLOCK_RESP,
    HANDSHAKE_REQ,
    HEADER_INFO_REQ,
    INSTALL_REQ,
    INSTALL_RESP,
    SmotaSerialClient,
    SmotaTcpClient,
    TRANSFER_COMPLETE_REQ,
    TRANSFER_COMPLETE_RESP,
    compare_version,
    decode_handshake,
    decode_query_version,
    format_version,
    read_ota_package,
)


LogFn = Callable[[str, str], None]
ProgressFn = Callable[[int], None]
StatusFn = Callable[[str], None]


@dataclass(frozen=True)
class UpgradeConfig:
    firmware_path: Path
    version: tuple[int, int, int]
    transport: str
    host: str
    port: int
    serial_port: str
    serial_baudrate: int
    timeout_s: float
    connect_timeout_s: float
    chunk_size: int
    block_timeout_ms: int
    check_timeout_ms: int
    install_timeout_ms: int
    total_timeout_ms: int
    force_install: bool
    activate_check: bool


class UpgradeSession:
    def __init__(
        self,
        config: UpgradeConfig,
        logger: LogFn,
        progress: ProgressFn,
        status: StatusFn,
    ) -> None:
        self.config = config
        self.logger = logger
        self.progress = progress
        self.status = status
        self._stop_event = threading.Event()

    def request_stop(self) -> None:
        self._stop_event.set()

    def run(self) -> None:
        package = read_ota_package(self.config.firmware_path)
        target_version = package.manifest.version
        firmware = package.firmware
        project_id = package.manifest.project_id.encode("utf-8")[:16].ljust(16, b"\x00")
        if self.config.transport == "serial":
            client = SmotaSerialClient(
                port=self.config.serial_port,
                baudrate=self.config.serial_baudrate,
                timeout=self.config.timeout_s,
                logger=self.logger,
            )
            self.logger("INFO", f"传输方式=串口 串口={self.config.serial_port} 波特率={self.config.serial_baudrate}")
        else:
            client = SmotaTcpClient(
                host=self.config.host,
                port=self.config.port,
                timeout=self.config.timeout_s,
                logger=self.logger,
            )
            self.logger("INFO", f"传输方式=TCP 地址={self.config.host}:{self.config.port}")

        self.logger("INFO", f"目标版本={format_version(target_version)}")
        self.logger("INFO", f"项目名称ID={package.manifest.project_id}")
        self.logger("INFO", f"OTA文件={package.path}")
        self.logger("INFO", f"容器固件={package.manifest.firmware_name}")
        self.progress(0)

        try:
            self.status("连接中")
            self._connect_with_retry(client, self.config.connect_timeout_s)
            self._ensure_not_stopped()

            self.status("读取版本")
            running_version = self._query_running_version(client)
            self.logger("INFO", f"当前运行版本={format_version(running_version)}")

            compare_result = compare_version(running_version, target_version)
            if not self.config.force_install:
                if compare_result == 0:
                    self.logger("INFO", "当前版本与目标版本一致，跳过下载")
                    self.status("已跳过")
                    self.progress(100)
                    return
                if compare_result > 0:
                    raise RuntimeError(
                        "当前运行版本高于目标版本："
                        f"{format_version(running_version)} > {format_version(target_version)}; "
                        "如需继续请启用强制安装"
                    )
            else:
                self.logger("INFO", "已启用强制安装，继续执行升级流程")

            firmware_hash = hashlib.sha256(firmware).digest()
            self.logger("INFO", f"固件文件={self.config.firmware_path}")
            self.logger("INFO", f"固件大小={len(firmware)} 字节")
            self.logger("INFO", f"sha256={firmware_hash.hex()}")

            self.status("握手中")
            handshake_frame = client.exchange(
                CMD_HANDSHAKE,
                CMD_HANDSHAKE_RESP,
                HANDSHAKE_REQ.pack(
                    target_version[0],
                    target_version[1],
                    target_version[2],
                    len(firmware),
                    project_id,
                    self.config.block_timeout_ms,
                    self.config.check_timeout_ms,
                    self.config.install_timeout_ms,
                    self.config.total_timeout_ms,
                ),
            )
            handshake = decode_handshake(handshake_frame.payload)
            if handshake.error_code != 0:
                raise RuntimeError(f"握手失败：0x{handshake.error_code:08X}")

            self.logger(
                "INFO",
                "握手成功 "
                f"最大包长={handshake.max_packet_size} "
                f"可用 Flash={handshake.flash_free_size} "
                f"起始偏移={handshake.next_offset}",
            )

            self.status("发送头信息")
            header_frame = client.exchange(
                CMD_HEADER_INFO,
                CMD_HEADER_INFO_RESP,
                HEADER_INFO_REQ.pack(firmware_hash, bytes(32), bytes(32)),
            )
            header_error = int.from_bytes(header_frame.payload[:4], "little")
            if header_error != 0:
                raise RuntimeError(f"头信息发送失败：0x{header_error:08X}")

            self.status("传输中")
            payload_size = max(1, min(self.config.chunk_size, handshake.max_packet_size - DATA_BLOCK_REQ.size))
            offset = handshake.next_offset
            while offset < len(firmware):
                self._ensure_not_stopped()
                chunk = firmware[offset:offset + payload_size]
                frame = client.exchange(
                    CMD_DATA_BLOCK,
                    CMD_DATA_BLOCK_RESP,
                    DATA_BLOCK_REQ.pack(offset, len(chunk)) + chunk,
                )
                block_error, received_offset = DATA_BLOCK_RESP.unpack(frame.payload)
                if block_error != 0:
                    raise RuntimeError(f"数据块发送失败，偏移 {offset}：0x{block_error:08X}")
                if received_offset != offset + len(chunk):
                    raise RuntimeError(
                        f"设备确认偏移为 {received_offset}，期望值为 {offset + len(chunk)}"
                    )
                offset = received_offset
                self.progress(int(offset * 100 / len(firmware)))
                self.logger("INFO", f"进度 {offset}/{len(firmware)} 字节")

            self.status("校验中")
            complete_frame = client.exchange(
                CMD_DATA_COMPLETE,
                CMD_DATA_COMPLETE_RESP,
                TRANSFER_COMPLETE_REQ.pack(len(firmware)),
            )
            complete_error = TRANSFER_COMPLETE_RESP.unpack(complete_frame.payload)[0]
            if complete_error != 0:
                raise RuntimeError(f"传输校验失败：0x{complete_error:08X}")
            self.logger("INFO", "传输校验成功")

            self.status("安装中")
            install_frame = client.exchange(
                CMD_INSTALL,
                CMD_INSTALL_RESP,
                INSTALL_REQ.pack(1 if self.config.force_install else 0, bytes(15)),
            )
            install_error, estimated_time_s = INSTALL_RESP.unpack(install_frame.payload)
            if install_error != 0:
                raise RuntimeError(f"安装请求失败：0x{install_error:08X}")
            self.logger("INFO", f"设备已接受安装，预计重启时间 {estimated_time_s} 秒")
            client.close()

            if not self.config.activate_check:
                self.status("已完成")
                self.progress(100)
                return

            self.status("等待重连")
            self.logger(
                "INFO",
                "设备即将断开；请重启 win_sim，并保持当前窗口开启以完成激活校验",
            )
            self._connect_with_retry(client, self.config.install_timeout_ms / 1000.0 + 5.0)
            self._ensure_not_stopped()

            self.status("激活校验")
            activate = self._query_running_version(client)
            if activate != target_version:
                raise RuntimeError(
                    "激活校验版本不一致："
                    f"当前运行 {format_version(activate)}，"
                    f"期望版本 {format_version(target_version)}"
                )

            self.logger("INFO", f"激活校验成功，当前运行版本 {format_version(activate)}")
            self.status("已完成")
            self.progress(100)
        finally:
            client.close()

    def _connect_with_retry(self, client: SmotaTcpClient | SmotaSerialClient, timeout_s: float) -> None:
        deadline = time.time() + timeout_s
        last_error: Exception | None = None

        while time.time() < deadline:
            self._ensure_not_stopped()
            try:
                client.connect()
                return
            except Exception as exc:
                last_error = exc
                self.logger("INFO", f"连接重试中: {exc}")
                time.sleep(0.2)

        if last_error is None:
            raise TimeoutError("连接超时")
        raise TimeoutError(f"连接超时：{last_error}") from last_error

    def _query_running_version(self, client: SmotaTcpClient | SmotaSerialClient) -> tuple[int, int, int]:
        self.logger("INFO", "准备查询设备当前运行版本")
        frame = client.exchange(
            CMD_QUERY_VERSION,
            CMD_QUERY_VERSION_RESP,
            QUERY_VERSION_REQ.pack(0),
        )
        response = decode_query_version(frame.payload)
        if response.error_code != 0:
            raise RuntimeError(f"读取版本失败：0x{response.error_code:08X}")
        return response.version

    def _ensure_not_stopped(self) -> None:
        if self._stop_event.is_set():
            raise RuntimeError("升级已取消")
