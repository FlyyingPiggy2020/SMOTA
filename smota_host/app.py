from __future__ import annotations

import queue
import threading
import tkinter as tk
from pathlib import Path
from tkinter import filedialog, messagebox, ttk

from .protocol import parse_version
from .session import UpgradeConfig, UpgradeSession


class HostApp:
    def __init__(self, root: tk.Tk) -> None:
        self.root = root
        self.root.title("smOTA Host")
        self.root.geometry("980x720")

        self.firmware_path = tk.StringVar(value=str(Path("examples/win_sim/README.md").resolve()))
        self.version = tk.StringVar(value="1.2.3")
        self.project_id = tk.StringVar(value="SMOTA_WIN_SIM")
        self.host = tk.StringVar(value="127.0.0.1")
        self.port = tk.StringVar(value="8888")
        self.chunk_size = tk.StringVar(value="128")
        self.timeout_s = tk.StringVar(value="3.0")
        self.connect_timeout_s = tk.StringVar(value="8.0")
        self.block_timeout_ms = tk.StringVar(value="5000")
        self.check_timeout_ms = tk.StringVar(value="30000")
        self.install_timeout_ms = tk.StringVar(value="15000")
        self.total_timeout_ms = tk.StringVar(value="60000")
        self.force_install = tk.BooleanVar(value=False)
        self.activate_check = tk.BooleanVar(value=True)
        self.status = tk.StringVar(value="Idle")

        self.progress = tk.IntVar(value=0)
        self.event_queue: queue.Queue[tuple[str, str, object]] = queue.Queue()
        self.worker: threading.Thread | None = None
        self.session: UpgradeSession | None = None

        self._build_ui()
        self.root.after(100, self._process_events)

    def _build_ui(self) -> None:
        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(3, weight=1)

        firmware_frame = ttk.LabelFrame(self.root, text="Firmware")
        firmware_frame.grid(row=0, column=0, sticky="ew", padx=12, pady=(12, 6))
        firmware_frame.columnconfigure(1, weight=1)

        ttk.Label(firmware_frame, text="File").grid(row=0, column=0, padx=8, pady=8, sticky="w")
        ttk.Entry(firmware_frame, textvariable=self.firmware_path).grid(
            row=0, column=1, padx=8, pady=8, sticky="ew"
        )
        ttk.Button(firmware_frame, text="Browse", command=self._browse_firmware).grid(
            row=0, column=2, padx=8, pady=8
        )

        config_frame = ttk.LabelFrame(self.root, text="Connection And Upgrade")
        config_frame.grid(row=1, column=0, sticky="ew", padx=12, pady=6)
        for index in range(6):
            config_frame.columnconfigure(index, weight=1)

        self._add_entry(config_frame, "Version", self.version, 0, 0)
        self._add_entry(config_frame, "Project ID", self.project_id, 0, 2)
        self._add_entry(config_frame, "Host", self.host, 0, 4)
        self._add_entry(config_frame, "Port", self.port, 1, 0)
        self._add_entry(config_frame, "Chunk Size", self.chunk_size, 1, 2)
        self._add_entry(config_frame, "Socket Timeout(s)", self.timeout_s, 1, 4)
        self._add_entry(config_frame, "Connect Timeout(s)", self.connect_timeout_s, 2, 0)
        self._add_entry(config_frame, "Block Timeout(ms)", self.block_timeout_ms, 2, 2)
        self._add_entry(config_frame, "Check Timeout(ms)", self.check_timeout_ms, 2, 4)
        self._add_entry(config_frame, "Install Timeout(ms)", self.install_timeout_ms, 3, 0)
        self._add_entry(config_frame, "Total Timeout(ms)", self.total_timeout_ms, 3, 2)

        ttk.Checkbutton(config_frame, text="Force Install", variable=self.force_install).grid(
            row=3, column=4, padx=8, pady=8, sticky="w"
        )
        ttk.Checkbutton(config_frame, text="Activate Check", variable=self.activate_check).grid(
            row=4, column=4, padx=8, pady=8, sticky="w"
        )

        action_frame = ttk.Frame(self.root)
        action_frame.grid(row=2, column=0, sticky="ew", padx=12, pady=6)
        action_frame.columnconfigure(3, weight=1)

        self.start_button = ttk.Button(action_frame, text="Start Upgrade", command=self._start_upgrade)
        self.start_button.grid(row=0, column=0, padx=(0, 8), pady=4)
        self.stop_button = ttk.Button(action_frame, text="Stop", command=self._stop_upgrade, state="disabled")
        self.stop_button.grid(row=0, column=1, padx=8, pady=4)
        ttk.Button(action_frame, text="Clear Log", command=self._clear_log).grid(row=0, column=2, padx=8, pady=4)
        ttk.Label(action_frame, textvariable=self.status).grid(row=0, column=3, sticky="e")

        progress_frame = ttk.Frame(self.root)
        progress_frame.grid(row=3, column=0, sticky="nsew", padx=12, pady=(0, 12))
        progress_frame.columnconfigure(0, weight=1)
        progress_frame.rowconfigure(1, weight=1)

        ttk.Progressbar(progress_frame, maximum=100, variable=self.progress).grid(
            row=0, column=0, sticky="ew", pady=(0, 8)
        )

        self.log_text = tk.Text(progress_frame, wrap="word", font=("Consolas", 10))
        self.log_text.grid(row=1, column=0, sticky="nsew")
        scrollbar = ttk.Scrollbar(progress_frame, orient="vertical", command=self.log_text.yview)
        scrollbar.grid(row=1, column=1, sticky="ns")
        self.log_text.configure(yscrollcommand=scrollbar.set)
        self.log_text.tag_configure("INFO", foreground="#243b53")
        self.log_text.tag_configure("TX", foreground="#0b6e4f")
        self.log_text.tag_configure("RX", foreground="#8a4b08")
        self.log_text.tag_configure("ERROR", foreground="#b42318")

    def _add_entry(self, parent: ttk.LabelFrame, label: str, variable: tk.StringVar, row: int, column: int) -> None:
        ttk.Label(parent, text=label).grid(row=row, column=column, padx=8, pady=8, sticky="w")
        ttk.Entry(parent, textvariable=variable).grid(row=row, column=column + 1, padx=8, pady=8, sticky="ew")

    def _browse_firmware(self) -> None:
        filename = filedialog.askopenfilename(
            title="Select firmware file",
            filetypes=[("All Files", "*.*"), ("Binary", "*.bin"), ("Hex", "*.hex"), ("Text", "*.txt *.md")],
        )
        if filename:
            self.firmware_path.set(filename)

    def _start_upgrade(self) -> None:
        if self.worker is not None and self.worker.is_alive():
            return

        try:
            config = UpgradeConfig(
                firmware_path=Path(self.firmware_path.get()).expanduser().resolve(),
                version=parse_version(self.version.get()),
                project_id=self.project_id.get().strip() or "SMOTA_WIN_SIM",
                host=self.host.get().strip(),
                port=int(self.port.get()),
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
            messagebox.showerror("Invalid Input", str(exc))
            return

        if not config.firmware_path.exists():
            messagebox.showerror("Firmware Missing", f"file not found: {config.firmware_path}")
            return

        self.progress.set(0)
        self._append_log("INFO", "starting upgrade session")
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

    def _run_worker(self) -> None:
        assert self.session is not None
        try:
            self.session.run()
            self.event_queue.put(("done", "INFO", "upgrade completed"))
        except Exception as exc:
            self.event_queue.put(("done", "ERROR", str(exc)))

    def _stop_upgrade(self) -> None:
        if self.session is not None:
            self.session.request_stop()
            self._append_log("INFO", "stop requested")

    def _clear_log(self) -> None:
        self.log_text.delete("1.0", tk.END)

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
                self.status.set("Idle" if level == "ERROR" else "Completed")
                self.start_button.configure(state="normal")
                self.stop_button.configure(state="disabled")

        self.root.after(100, self._process_events)

    def _append_log(self, level: str, message: str) -> None:
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
