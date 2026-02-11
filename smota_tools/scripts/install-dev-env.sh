#!/bin/bash
#=====================================================================
# Windows 开发环境安装脚本
#=====================================================================
# 功能: 安装 Rust、Node.js、pnpm、C++ 构建工具
# 支持: Windows 10/11 (在 Git Bash 或 WSL 中运行)
#=====================================================================

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 日志函数
log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 检查是否在 Windows 环境下运行
check_windows() {
    if [[ "$(uname)" != "MINGW"* ]] && [[ "$(uname)" != "MSYS"* ]]; then
        log_warn "检测到非 MINGW 环境，某些命令可能需要手动执行"
        log_info "请确保已安装 winget，并在 PowerShell 中以管理员身份运行以下命令："
        echo ""
        echo "winget install Rustlang.Rustup"
        echo "winget install OpenJS.NodeJS.LTS"
        echo "winget install Microsoft.VisualStudio.2022.BuildTools"
        echo "winget install Microsoft.WebView2"
        echo ""
        read -p "是否继续尝试安装其他组件? (y/n): " confirm
        if [[ "$confirm" != "y" && "$confirm" != "Y" ]]; then
            exit 0
        fi
    fi
}

# 检查命令是否存在
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# 安装 Rust
install_rust() {
    if command_exists rustc; then
        log_info "Rust 已安装: $(rustc --version)"
        return 0
    fi

    log_info "正在安装 Rust..."

    if command_exists winget; then
        winget install Rustlang.Rustup -e
    else
        log_warn "请手动安装 Rust: https://rustup.rs/"
        log_info "下载 rustup-init.exe 并运行"
    fi

    # 激活 Rust 环境
    if [ -f "$HOME/.cargo/env" ]; then
        source "$HOME/.cargo/env"
    fi

    # 验证安装
    if command_exists rustc; then
        log_info "Rust 安装成功: $(rustc --version)"
    else
        log_warn "Rust 安装可能未完成，请重启终端后运行 'rustc --version' 验证"
    fi
}

# 安装 Node.js
install_node() {
    if command_exists node; then
        log_info "Node.js 已安装: $(node --version)"
        return 0
    fi

    log_info "正在安装 Node.js..."

    if command_exists winget; then
        winget install OpenJS.NodeJS.LTS -e
    else
        log_warn "请手动安装 Node.js: https://nodejs.org/"
    fi

    if command_exists node; then
        log_info "Node.js 安装成功: $(node --version)"
    else
        log_warn "Node.js 安装可能未完成，请重启终端后运行 'node --version' 验证"
    fi
}

# 安装 pnpm
install_pnpm() {
    if command_exists pnpm; then
        log_info "pnpm 已安装: $(pnpm --version)"
        return 0
    fi

    log_info "正在安装 pnpm..."

    if command_exists npm; then
        npm install -g pnpm
    else
        log_warn "npm 未安装，无法使用 npm 安装 pnpm"
        log_info "请在 Node.js 安装后手动运行: npm install -g pnpm"
    fi

    if command_exists pnpm; then
        log_info "pnpm 安装成功: $(pnpm --version)"
    else
        log_warn "pnpm 安装可能未完成"
    fi
}

# 安装 C++ 构建工具
install_build_tools() {
    log_info "检查 C++ 构建工具..."

    if command_exists cl; then
        log_info "C++ 构建工具已安装"
        return 0
    fi

    log_warn "C++ 构建工具未安装"
    log_info "请手动安装 Visual Studio Build Tools:"
    echo ""
    echo "1. 下载: https://visualstudio.microsoft.com/visual-cpp-build-tools/"
    echo "2. 运行安装程序"
    echo "3. 选择 '使用 C++ 的桌面开发' 工作负载"
    echo "4. 确保勾选 'Windows 11 SDK'"
    echo ""

    if command_exists winget; then
        read -p "是否尝试使用 winget 安装? (需要管理员权限) (y/n): " confirm
        if [[ "$confirm" == "y" || "$confirm" == "Y" ]]; then
            log_info "正在安装 Visual Studio Build Tools..."
            winget install Microsoft.VisualStudio.2022.BuildTools -e
            log_warn "安装完成后请重启终端"
        fi
    fi
}

# 安装 WebView2
install_webview2() {
    log_info "检查 WebView2..."

    if command_exists reg; then
        # 检查注册表中的 WebView2
        if reg query "HKLM\SOFTWARE\WOW6432Node\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}" >/dev/null 2>&1; then
            log_info "WebView2 已安装"
            return 0
        fi
    fi

    log_warn "WebView2 未安装或版本过旧"
    log_info "请手动安装 WebView2 Evergreen Runtime:"
    echo ""
    echo "下载: https://developer.microsoft.com/en-us/microsoft-edge/webview2/"
    echo ""

    if command_exists winget; then
        read -p "是否尝试使用 winget 安装? (y/n): " confirm
        if [[ "$confirm" == "y" || "$confirm" == "Y" ]]; then
            winget install Microsoft.WebView2 -e
            log_info "WebView2 安装完成"
        fi
    fi
}

# 配置国内镜像
configure_mirror() {
    log_info "配置 pnpm 国内镜像..."

    if command_exists pnpm; then
        pnpm config set registry https://registry.npmmirror.com
        log_info "pnpm 镜像已配置"
    else
        log_warn "pnpm 未安装，跳过镜像配置"
    fi

    log_info "Rust 镜像配置（可选）:"
    echo ""
    echo "创建或编辑 ~/.cargo/config.toml，内容如下："
    echo ""
    cat << 'EOF'
[source.crates-io]
replace-with = 'ustc'

[source.ustc]
registry = "sparse+https://mirrors.ustc.edu.cn/crates.io-index/"
EOF
    echo ""
}

# 安装项目依赖
install_dependencies() {
    log_info "检查项目依赖..."

    if [ ! -f "package.json" ]; then
        log_warn "未找到 package.json，跳过依赖安装"
        log_info "请进入项目目录后手动运行: pnpm install"
        return 0
    fi

    if ! command_exists pnpm; then
        log_warn "pnpm 未安装，无法安装项目依赖"
        return 0
    fi

    log_info "正在安装项目依赖..."
    pnpm install

    if [ -d "node_modules" ]; then
        log_info "项目依赖安装完成"
    else
        log_warn "项目依赖安装可能失败"
    fi
}

# 验证安装
verify_installation() {
    log_info "=== 验证安装结果 ==="
    echo ""

    echo "Rust:"
    if command_exists rustc; then
        echo "  ✓ rustc: $(rustc --version)"
        echo "  ✓ cargo: $(cargo --version)"
    else
        echo "  ✗ 未安装"
    fi

    echo ""
    echo "Node.js:"
    if command_exists node; then
        echo "  ✓ node: $(node --version)"
        echo "  ✓ npm: $(npm --version)"
    else
        echo "  ✗ 未安装"
    fi

    echo ""
    echo "pnpm:"
    if command_exists pnpm; then
        echo "  ✓ pnpm: $(pnpm --version)"
    else
        echo "  ✗ 未安装"
    fi

    echo ""
    echo "C++ 编译器:"
    if command_exists cl; then
        echo "  ✓ cl: 已安装"
    else
        echo "  ✗ 未安装 (Visual Studio Build Tools)"
    fi

    echo ""
    echo "WebView2:"
    echo "  ? 请手动验证：打开 edge://version 查看 WebView2 版本"
}

# 打印完成信息
print_summary() {
    echo ""
    echo "=================================================="
    echo "              安装检查完成"
    echo "=================================================="
    echo ""
    echo "下一步操作:"
    echo "  1. 进入项目目录: cd smota_tools/serial-tool"
    echo "  2. 启动开发环境: pnpm tauri dev"
    echo ""
    echo "文档位置: doc/windows-setup.md"
    echo ""
}

# 主函数
main() {
    echo "=================================================="
    echo "     Windows 开发环境安装脚本"
    echo "=================================================="
    echo ""

    check_windows

    install_rust
    install_node
    install_pnpm
    install_build_tools
    install_webview2
    configure_mirror

    echo ""
    read -p "是否立即安装项目依赖? (y/n): " install_deps
    if [[ "$install_deps" == "y" || "$install_deps" == "Y" ]]; then
        install_dependencies
    fi

    verify_installation
    print_summary
}

# 运行主函数
main "$@"
