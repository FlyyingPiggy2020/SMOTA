# Windows 开发环境安装指南

> 本项目基于 Tauri 框架构建，需要安装 Rust 和 Node.js 环境。

## 1. 安装 Rust

### 1.1 下载安装

访问 [Rust 官网](https://www.rust-lang.org/tools/install) 下载 `rustup-init.exe`，或使用以下命令：

```powershell
winget install Rustlang.Rustup
```

### 1.2 验证安装

```bash
rustc --version
cargo --version
```

**预期输出：**
```
rustc 1.XX.X (XXXXXX 2025-XX-XX)
cargo 1.XX.X (XXXXXX 2025-XX-XX)
```

---

## 2. 安装 Node.js

### 2.1 下载安装

访问 [Node.js 官网](https://nodejs.org/) 下载 LTS 版本，或使用 winget：

```powershell
winget install OpenJS.NodeJS.LTS
```

### 2.2 验证安装

```bash
node --version
npm --version
```

**预期输出：**
```
v20.X.X
10.X.X
```

---

## 3. 安装 pnpm

### 3.1 使用 npm 安装（推荐）

```bash
npm install -g pnpm
```

### 3.2 使用 corepack 安装

```bash
corepack enable
corepack prepare pnpm@latest --activate
```

### 3.3 验证安装

```bash
pnpm --version
```

**预期输出：**
```
9.X.X
```

---

## 4. 安装 C++ 构建工具

Tauri 需要 C++ 编译环境。

### 4.1 使用 winget 安装

```powershell
winget install Microsoft.VisualStudio.2022.BuildTools
```

### 4.2 手动安装

1. 下载 [Visual Studio Build Tools](https://visualstudio.microsoft.com/visual-cpp-build-tools/)
2. 安装时选择 **"使用 C++ 的桌面开发"** 工作负载
3. 确保勾选 **"Windows 11 SDK"**（或 Windows 10 SDK）

### 4.3 验证安装

```bash
cl /?
```

**预期输出：**
```
Microsoft (R) C/C++ Optimizing Compiler Version XXXXXX
```

---

## 5. 安装 WebView2

Tauri 需要 WebView2 运行时（Windows 11 已内置）。

### 5.1 使用 winget 安装

```powershell
winget install Microsoft.WebView2
```

### 5.2 手动安装

下载 [WebView2 Evergreen Runtime](https://developer.microsoft.com/en-us/microsoft-edge/webview2/) 并安装。

---

## 6. 配置国内镜像（可选）

### 6.1 pnpm 镜像

```bash
pnpm config set registry https://registry.npmmirror.com
```

### 6.2 Rust 镜像（编辑 `~/.cargo/config.toml`）

```toml
[source.crates-io]
replace-with = 'ustc'

[source.ustc]
registry = "sparse+https://mirrors.ustc.edu.cn/crates.io-index/"
```

---

## 7. 克隆项目并安装依赖

```bash
# 克隆项目
git clone <your-repo-url>
cd SMOTA/smota_tools/serial-tool

# 安装依赖
pnpm install
```

---

## 8. 启动开发环境

```bash
pnpm tauri dev
```

---

## 快速验证清单

| 工具 | 验证命令 | 成功表现 |
|------|----------|----------|
| Rust | `rustc --version` | 显示版本号 |
| Cargo | `cargo --version` | 显示版本号 |
| Node.js | `node --version` | 显示版本号 |
| pnpm | `pnpm --version` | 显示版本号 |
| C++ 编译器 | `cl /?` | 显示编译器信息 |

---

## 常见问题

### Q: 编译时报错 "could not find VC++ toolchain"
A: 确保已安装 Visual Studio Build Tools，并重启终端。

### Q: pnpm install 速度慢
A: 配置国内镜像（见第 6 节）。

### Q: WebView2 找不到
A: 安装 WebView2 Evergreen Runtime。

### Q: 权限错误
A: 以管理员身份运行终端，或配置 npm/pnpm 使用自定义目录。
