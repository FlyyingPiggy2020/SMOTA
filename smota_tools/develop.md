这是一个为你整理的**《现代极简、专业级串口工具开发路线图》**。你可以将其保存为 `ROADMAP.md`，作为项目的指导文档。

---

# 🚀 极简 & 专业串口工具开发路线图 (Tauri + Rust + Vue/React)

本文档旨在指导如何使用 **Tauri 2.0** 技术栈开发一款高颜值、高性能、支持 Python 自定义脚本校验的专业串口调试工具。

---

## 🏗️ 第一阶段：开发环境配置
在开始之前，必须确保 Windows 开发环境具备底层编译能力。

1.  **安装 Rust 工具链**
    *   前往 [rustup.rs](https://rustup.rs/) 下载并运行 `rustup-init.exe`。
2.  **安装 Node.js**
    *   建议版本 `v18+`，推荐使用 [pnpm](https://pnpm.io/) 作为包管理器。
3.  **安装 Visual Studio C++ 生成工具 (关键)**
    *   下载 [Visual Studio Installer](https://visualstudio.microsoft.com/zh-hans/downloads/)。
    *   选择 **“使用 C++ 的桌面开发”** 工作负载。
    *   确认勾选：`MSVC v143` 和 `Windows 10/11 SDK`。
    *   *作用：Rust 需要它来链接 Windows 系统 API 并生成 `.exe`。*

---

## 📂 第二阶段：项目初始化

使用 Tauri 脚手架快速创建项目骨架：

```bash
# 创建项目
pnpm create tauri-app

# 推荐配置选择：
# Project Name: serial-pro
# Frontend Language: TypeScript
# Package Manager: pnpm
# UI Template: Vue (或 React)
# UI Flavor: Vite
```

---

## ⚙️ 第三阶段：Rust 后端逻辑 (串口 & Python)

### 1. 添加依赖项
在 `src-tauri/Cargo.toml` 中加入核心库：
```toml
[dependencies]
serialport = "4.3.0"  # 串口操作
pyo3 = { version = "0.20", features = ["auto-initialize"] } # 嵌入 Python
serde = { version = "1.0", features = ["derive"] } # 序列化
```

### 2. 核心功能模块实现
*   **串口管理**：实现获取串口列表、打开/关闭串口、设置比特率等。
*   **异步读取**：利用 Rust 的线程或 `tokio` 任务持续监听串口数据，并通过 `emit` 事件发送至前端。
*   **Python 桥接**：实现一个 `run_checksum_script` 的 Tauri Command，将待发数据传给用户的 `.py` 脚本，并取回带校验位的结果。

---

## 🎨 第四阶段：前端 UI 设计 (高颜值的关键)

为了让工具看起来“专业且高级”，建议采用以下技术栈：

| 组件类型       | 推荐方案                  | 说明                                    |
| :------------- | :------------------------ | :-------------------------------------- |
| **基础 UI 库** | **Shadcn UI** (React/Vue) | 极简、现代、高度可定制。                |
| **编辑器**     | **Monaco Editor**         | 与 VS Code 同款，用于编写 Python 脚本。 |
| **图标**       | **Lucide Icons**          | 线条感强的矢量图标库。                  |
| **布局**       | **Tailwind CSS**          | 快速实现响应式和深色模式适配。          |
| **可视化**     | **ECharts**               | 实时展示数据波形（如电压、温度等）。    |

---

## 🐍 第五阶段：Python 脚本集成设计

### 用户脚本示例 (`checksum.py`)
定义一个标准接口供用户实现：
```python
def calculate(data_hex_list):
    # data_hex_list: 传入的字节数组
    # 返回: 经过计算后的字节数组或校验位
    checksum = sum(data_hex_list) & 0xFF
    return [checksum]
```

### 工具交互流程
1.  **用户输入**：在前端 UI 的 Monaco Editor 中编写 Python 逻辑。
2.  **发送触发**：点击“发送”时，前端将数据和脚本内容传给 Rust。
3.  **Rust 执行**：Rust 调用内嵌 Python 解释器运行脚本，获取返回数据。
4.  **串口输出**：Rust 将最终的字节流写入串口。

---

## 📦 第六阶段：打包与发布

Tauri 的优势在于生成的安装包极小（通常 < 10MB）。

```bash
# 构建生产环境安装包
pnpm tauri build
```
*生成的安装包位于：`src-tauri/target/release/bundle/msi/`*

---

## 💡 开发建议 (Best Practices)

1.  **分治原则**：UI 只负责展示和用户输入，复杂的二进制计算和串口 IO 放在 Rust 侧。
2.  **异常处理**：串口插入/拔出是高频操作，在 Rust 中需处理好 `Panics`，并通过 Tauri 事件通知用户“设备已断开”。
3.  **性能优化**：对于高频（如 1ms 间隔）接收的串口数据，前端应采用缓存机制，避免由于频繁更新 DOM 导致界面卡顿。
4.  **脚本安全**：虽然是本地工具，但如果支持加载外部脚本，应在文档中提醒用户注意脚本来源。

---

## 🔗 参考资源
*   [Tauri 官方文档](https://tauri.app/)
*   [Rust Serialport 库文档](https://docs.rs/serialport/)
*   [PyO3 (Rust & Python 交互) 指南](https://pyo3.rs/)
*   [Lucide 图标库](https://lucide.dev/)