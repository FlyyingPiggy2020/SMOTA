from __future__ import annotations

import queue
import threading
import tkinter as tk
from pathlib import Path
from tkinter import filedialog, messagebox, ttk

from .protocol import SerialPortInfo, format_version, list_serial_ports, parse_version, read_ota_package
from .session import UpgradeConfig, UpgradeSession


class HostApp:
    def __init__(self, root: tk.Tk) -> None:
        self.root = root
        self.root.title("smOTA 上位机")
        self.root.geometry("980x720")

        self.firmware_path = tk.StringVar(value="")
        self.version = tk.StringVar(value="")
        self.project_id = tk.StringVar(value="")
        self.transport = tk.StringVar(value="tcp")
        self.host = tk.StringVar(value="127.0.0.1")
        self.port = tk.StringVar(value="8888")
        self.serial_port = tk.StringVar(value="")
        self.serial_baudrate = tk.StringVar(value="115200")
        self.chunk_size = tk.StringVar(value="128")
        self.timeout_s = tk.StringVar(value="3.0")
        self.connect_timeout_s = tk.StringVar(value="8.0")
        self.block_timeout_ms = tk.StringVar(value="5000")
        self.check_timeout_ms = tk.StringVar(value="30000")
        self.install_timeout_ms = tk.StringVar(value="15000")
        self.total_timeout_ms = tk.StringVar(value="60000")
        self.force_install = tk.BooleanVar(value=False)
        self.activate_check = tk.BooleanVar(value=True)
        self.status = tk.StringVar(value="空闲")

        self.progress = tk.IntVar(value=0)
        self.event_queue: queue.Queue[tuple[str, str, object]] = queue.Queue()
        self.worker: threading.Thread | None = None
        self.session: UpgradeSession | None = None
        self.serial_ports: list[SerialPortInfo] = []
        self.serial_port_labels: dict[str, str] = {}

        self._load_serial_ports()
        if self.serial_ports:
            self.serial_port.set(self.serial_ports[0].device)
            self.transport.set("serial")

        self._build_ui()
        self.root.after(100, self._process_events)

    def _build_ui(self) -> None:
        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(3, weight=1)

        firmware_frame = ttk.LabelFrame(self.root, text="固件")
        firmware_frame.grid(row=0, column=0, sticky="ew", padx=12, pady=(12, 6))
        firmware_frame.columnconfigure(1, weight=1)

        ttk.Label(firmware_frame, text="文件").grid(row=0, column=0, padx=8, pady=8, sticky="w")
        ttk.Entry(firmware_frame, textvariable=self.firmware_path).grid(
            row=0, column=1, padx=8, pady=8, sticky="ew"
        )
        ttk.Button(firmware_frame, text="浏览", command=self._browse_firmware).grid(
            row=0, column=2, padx=8, pady=8
        )

        config_frame = ttk.LabelFrame(self.root, text="连接与升级")
        config_frame.grid(row=1, column=0, sticky="ew", padx=12, pady=6)
        config_frame.columnconfigure(0, weight=1)

        common_frame = ttk.Frame(config_frame)
        common_frame.grid(row=0, column=0, sticky="ew", padx=8, pady=8)
        for index in range(6):
            common_frame.columnconfigure(index, weight=1)

        self._add_entry(common_frame, "目标版本", self.version, 0, 0, state="readonly")
        self._add_entry(common_frame, "项目名称ID", self.project_id, 0, 2, state="readonly")
        self._add_combo(common_frame, "传输方式", self.transport, ["tcp", "serial"], 0, 4)
        self._add_entry(common_frame, "分块大小", self.chunk_size, 1, 0)
        self._add_entry(common_frame, "通信超时(s)", self.timeout_s, 1, 2)
        self._add_entry(common_frame, "连接超时(s)", self.connect_timeout_s, 1, 4)
        self._add_entry(common_frame, "分块超时(ms)", self.block_timeout_ms, 2, 0)
        self._add_entry(common_frame, "校验超时(ms)", self.check_timeout_ms, 2, 2)
        self._add_entry(common_frame, "安装超时(ms)", self.install_timeout_ms, 2, 4)
        self._add_entry(common_frame, "总超时(ms)", self.total_timeout_ms, 3, 0)

        ttk.Checkbutton(common_frame, text="强制安装", variable=self.force_install).grid(
            row=3, column=2, padx=8, pady=8, sticky="w"
        )
        ttk.Checkbutton(common_frame, text="激活校验", variable=self.activate_check).grid(
            row=3, column=4, padx=8, pady=8, sticky="w"
        )

        self.transport_frame = ttk.Frame(config_frame)
        self.transport_frame.grid(row=1, column=0, sticky="ew", padx=8, pady=(0, 8))
        self.transport_frame.columnconfigure(0, weight=1)

        self.tcp_frame = ttk.LabelFrame(self.transport_frame, text="TCP 配置")
        for index in range(4):
            self.tcp_frame.columnconfigure(index, weight=1)
        self._add_entry(self.tcp_frame, "主机地址", self.host, 0, 0)
        self._add_entry(self.tcp_frame, "端口", self.port, 0, 2)

        self.serial_frame = ttk.LabelFrame(self.transport_frame, text="串口配置")
        for index in range(6):
            self.serial_frame.columnconfigure(index, weight=1)
        self._add_serial_combo(self.serial_frame, "串口", 0, 0)
        self._add_entry(self.serial_frame, "串口波特率", self.serial_baudrate, 0, 2)
        ttk.Button(self.serial_frame, text="刷新串口", command=self._refresh_serial_ports).grid(
            row=0, column=4, padx=8, pady=8, sticky="ew"
        )

        self.transport.trace_add("write", self._on_transport_changed)
        self._update_transport_frame()

        action_frame = ttk.Frame(self.root)
        action_frame.grid(row=2, column=0, sticky="ew", padx=12, pady=6)
        action_frame.columnconfigure(3, weight=1)

        self.start_button = ttk.Button(action_frame, text="连接并升级", command=self._start_upgrade)
        self.start_button.grid(row=0, column=0, padx=(0, 8), pady=4)
        self.stop_button = ttk.Button(action_frame, text="停止", command=self._stop_upgrade, state="disabled")
        self.stop_button.grid(row=0, column=1, padx=8, pady=4)
        ttk.Button(action_frame, text="清空日志", command=self._clear_log).grid(row=0, column=2, padx=8, pady=4)
        ttk.Label(action_frame, textvariable=self.status).grid(row=0, column=3, sticky="e")

        progress_frame = ttk.Frame(self.root)
        progress_frame.grid(row=3, column=0, sticky="nsew", padx=12, pady=(0, 12))
        progress_frame.columnconfigure(0, weight=1)
        progress_frame.rowconfigure(1, weight=1)

        ttk.Progressbar(progress_frame, maximum=100, variable=self.progress).grid(
            row=0, column=0, sticky="ew", pady=(0, 8)
        )

        notebook = ttk.Notebook(progress_frame)
        notebook.grid(row=1, column=0, sticky="nsew")

        log_tab = ttk.Frame(notebook)
        log_tab.columnconfigure(0, weight=1)
        log_tab.rowconfigure(0, weight=1)
        notebook.add(log_tab, text="日志")

        raw_tab = ttk.Frame(notebook)
        raw_tab.columnconfigure(0, weight=1)
        raw_tab.rowconfigure(0, weight=1)
        notebook.add(raw_tab, text="原始报文")

        self.log_text = tk.Text(log_tab, wrap="word", font=("Consolas", 10))
        self.log_text.grid(row=0, column=0, sticky="nsew")
        log_scrollbar = ttk.Scrollbar(log_tab, orient="vertical", command=self.log_text.yview)
        log_scrollbar.grid(row=0, column=1, sticky="ns")
        self.log_text.configure(yscrollcommand=log_scrollbar.set)

        self.raw_text = tk.Text(raw_tab, wrap="none", font=("Consolas", 10))
        self.raw_text.grid(row=0, column=0, sticky="nsew")
        raw_scrollbar = ttk.Scrollbar(raw_tab, orient="vertical", command=self.raw_text.yview)
        raw_scrollbar.grid(row=0, column=1, sticky="ns")
        self.raw_text.configure(yscrollcommand=raw_scrollbar.set)

        self.log_text.tag_configure("INFO", foreground="#243b53")
        self.log_text.tag_configure("ERROR", foreground="#b42318")
        self.raw_text.tag_configure("TX", foreground="#0b6e4f")
        self.raw_text.tag_configure("RX", foreground="#8a4b08")
        self.raw_text.tag_configure("ERROR", foreground="#b42318")

    def _add_entry(
        self,
        parent: ttk.LabelFrame,
        label: str,
        variable: tk.StringVar,
        row: int,
        column: int,
        state: str = "normal",
    ) -> None:
        ttk.Label(parent, text=label).grid(row=row, column=column, padx=8, pady=8, sticky="w")
        ttk.Entry(parent, textvariable=variable, state=state).grid(
            row=row, column=column + 1, padx=8, pady=8, sticky="ew"
        )

    def _add_combo(
        self,
        parent: ttk.LabelFrame,
        label: str,
        variable: tk.StringVar,
        values: list[str],
        row: int,
        column: int,
    ) -> None:
        ttk.Label(parent, text=label).grid(row=row, column=column, padx=8, pady=8, sticky="w")
        ttk.Combobox(parent, textvariable=variable, values=values, state="readonly").grid(
            row=row, column=column + 1, padx=8, pady=8, sticky="ew"
        )

    def _add_serial_combo(self, parent: ttk.LabelFrame, label: str, row: int, column: int) -> None:
        ttk.Label(parent, text=label).grid(row=row, column=column, padx=8, pady=8, sticky="w")
        self.serial_combo = ttk.Combobox(parent, state="readonly")
        self.serial_combo.grid(row=row, column=column + 1, padx=8, pady=8, sticky="ew")
        self.serial_combo.bind("<<ComboboxSelected>>", self._on_serial_selected)
        self._update_serial_combo_values()

    def _load_serial_ports(self) -> None:
        self.serial_ports = list_serial_ports()
        self.serial_port_labels = {port.device: port.label for port in self.serial_ports}

    def _update_serial_combo_values(self) -> None:
        if not hasattr(self, "serial_combo"):
            return

        labels = [port.label for port in self.serial_ports]
        self.serial_combo.configure(values=labels)

        current_device = self.serial_port.get().strip()
        if current_device in self.serial_port_labels:
            self.serial_combo.set(self.serial_port_labels[current_device])
        elif labels:
            self.serial_combo.set(labels[0])
            self.serial_port.set(self.serial_ports[0].device)
        else:
            self.serial_combo.set("")

    def _on_serial_selected(self, _event: object) -> None:
        selected_label = self.serial_combo.get().strip()
        for port in self.serial_ports:
            if port.label == selected_label:
                self.serial_port.set(port.device)
                return

    def _browse_firmware(self) -> None:
        filename = filedialog.askopenfilename(
            title="选择 OTA 文件",
            filetypes=[("OTA 文件", "*.ota")],
        )
        if filename:
            try:
                package = read_ota_package(filename)
            except Exception as exc:
                messagebox.showerror("OTA 文件无效", str(exc))
                return

            self.firmware_path.set(filename)
            self.version.set(format_version(package.manifest.version))
            self.project_id.set(package.manifest.project_id)
            self._append_log(
                "INFO",
                "已加载 OTA 文件 "
                f"{package.path.name}，"
                f"目标版本 {format_version(package.manifest.version)}，"
                f"项目名称ID {package.manifest.project_id}",
            )

    def _on_transport_changed(self, *_args: object) -> None:
        self._update_transport_frame()

    def _update_transport_frame(self) -> None:
        transport = self.transport.get().strip().lower() or "tcp"

        self.tcp_frame.grid_forget()
        self.serial_frame.grid_forget()

        if transport == "serial":
            self.serial_frame.grid(row=0, column=0, sticky="ew")
        else:
            self.tcp_frame.grid(row=0, column=0, sticky="ew")

    def _start_upgrade(self) -> None:
        if self.worker is not None and self.worker.is_alive():
            return

        try:
            transport = self.transport.get().strip().lower() or "tcp"
            if transport not in ("tcp", "serial"):
                raise ValueError("传输方式只能是 tcp 或 serial")

            config = UpgradeConfig(
                firmware_path=Path(self.firmware_path.get()).expanduser().resolve(),
                version=parse_version(self.version.get()),
                transport=transport,
                host=self.host.get().strip(),
                port=int(self.port.get()),
                serial_port=self.serial_port.get().strip(),
                serial_baudrate=int(self.serial_baudrate.get()),
                timeout_s=float(self.timeout_s.get()),
                connect_timeout_s=float(self.connect_timeout_s.get()),
                chunk_size=int(self.chunk_size.get()),
                block_timeout_ms=int(self.block_timeout_ms.get()),
                check_timeout_ms=int(self.check_timeout_ms.get()),
                install_timeout_ms=int(self.install_timeout_ms.get()),
                total_timeout_ms=int(self.total_timeout_ms.get()),
                force_install=self.force_install.get(),
                activate_check=self.activate_check.get(),
            )
        except Exception as exc:
            messagebox.showerror("输入无效", str(exc))
            return

        if config.transport == "serial" and not config.serial_port:
            messagebox.showerror("串口未填写", "串口号不能为空")
            return

        if not config.firmware_path.exists():
            messagebox.showerror("固件不存在", f"未找到文件：{config.firmware_path}")
            return

        if config.firmware_path.suffix.lower() != ".ota":
            messagebox.showerror("OTA 类型错误", "只允许选择 .ota OTA 文件")
            return

        try:
            package = read_ota_package(config.firmware_path)
        except Exception as exc:
            messagebox.showerror("OTA 文件无效", str(exc))
            return

        self.version.set(format_version(package.manifest.version))
        self.project_id.set(package.manifest.project_id)

        self.progress.set(0)
        self._append_log("INFO", "开始连接、版本检查和升级流程")
        self.start_button.configure(state="disabled")
        self.stop_button.configure(state="normal")

        self.session = UpgradeSession(
            config=config,
            logger=lambda level, msg: self.event_queue.put(("log", level, msg)),
            progress=lambda value: self.event_queue.put(("progress", "", value)),
            status=lambda msg: self.event_queue.put(("status", "", msg)),
        )
        self.worker = threading.Thread(target=self._run_worker, daemon=True)
        self.worker.start()

    def _refresh_serial_ports(self) -> None:
        current_device = self.serial_port.get().strip()
        self._load_serial_ports()
        self._update_serial_combo_values()

        if not self.serial_ports:
            messagebox.showwarning("串口列表", "未找到可用串口")
            return

        if current_device and current_device in self.serial_port_labels:
            self.serial_port.set(current_device)
            self.serial_combo.set(self.serial_port_labels[current_device])
        elif self.serial_ports:
            self.serial_port.set(self.serial_ports[0].device)
            self.serial_combo.set(self.serial_ports[0].label)

        messagebox.showinfo("串口列表", "\n".join(port.label for port in self.serial_ports))

    def _run_worker(self) -> None:
        assert self.session is not None
        try:
            self.session.run()
            self.event_queue.put(("done", "INFO", "升级完成"))
        except Exception as exc:
            self.event_queue.put(("done", "ERROR", str(exc)))

    def _stop_upgrade(self) -> None:
        if self.session is not None:
            self.session.request_stop()
            self._append_log("INFO", "已请求停止升级")

    def _clear_log(self) -> None:
        self.log_text.delete("1.0", tk.END)
        self.raw_text.delete("1.0", tk.END)

    def _process_events(self) -> None:
        while True:
            try:
                event, level, payload = self.event_queue.get_nowait()
            except queue.Empty:
                break

            if event == "log":
                self._append_log(level, str(payload))
            elif event == "progress":
                self.progress.set(int(payload))
            elif event == "status":
                self.status.set(str(payload))
            elif event == "done":
                self._append_log(level, str(payload))
                self.status.set("空闲" if level == "ERROR" else "已完成")
                self.start_button.configure(state="normal")
                self.stop_button.configure(state="disabled")

        self.root.after(100, self._process_events)

    def _append_log(self, level: str, message: str) -> None:
        if level in ("TX", "RX"):
            self.raw_text.insert(tk.END, f"[{level}] {message}\n", level)
            self.raw_text.see(tk.END)
            return

        self.log_text.insert(tk.END, f"[{level}] {message}\n", level)
        self.log_text.see(tk.END)


def main() -> int:
    root = tk.Tk()
    style = ttk.Style(root)
    if "vista" in style.theme_names():
        style.theme_use("vista")
    HostApp(root)
    root.mainloop()
    return 0
