# smOTA 上位机

这个目录是新的独立上位机实现，和 `examples/win_sim/test_host.py` 不再绑在一起。

当前版本特性：

- 桌面 GUI，技术栈为 Python `tkinter`
- 选择固件文件
- 配置版本号、项目 ID、TCP 地址和超时参数
- 显示升级进度
- 显示主机侧详细日志
- 显示 `TX` / `RX` 协议帧摘要
- 可直接连接手动启动的 `win_sim`

## 启动

在仓库根目录执行：

```powershell
python -m smota_host
```

## 和 win_sim 联调

先在一个终端里手动启动模拟器，这样你能直接看到设备侧日志：

```powershell
cd C:\Users\w1545\Desktop\Project\SMOTA
.\examples\win_sim\build\win_sim.exe -i
.\examples\win_sim\build\win_sim.exe -r
```

然后启动 GUI：

```powershell
python -m smota_host
```

推荐参数：

- Host: `127.0.0.1`
- Port: `8888`
- Version: `1.2.3`
- Project ID: `SMOTA_WIN_SIM`
- Chunk Size: `128`

固件文件第一版不强制要求是真实 MCU 镜像，`win_sim` 当前验证的是 OTA 传输流程，所以任意文件都可用于联调，例如：

- `examples/win_sim/README.md`
- 你的 `app.bin`
- 你的 `firmware.hex`

## 当前范围

这版先只做了 `TCP` 传输，适配 `win_sim` 调试场景。后续如果要接真实设备，建议下一步再补：

- Serial 传输层
- 固件包元数据解析
- 升级历史和结果导出
- 更完整的十六进制报文查看器
