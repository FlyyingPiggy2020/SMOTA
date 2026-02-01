<#
.SYNOPSIS
    SMOTA Tool 快速验证脚本
.DESCRIPTION
    自动执行 Rust 编译、前端构建、开发服务器启动
    并提供 UI 检查清单
#>

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ScriptDir

function Write-Header {
    param([string]$Title)
    Write-Host "`n========================================" -ForegroundColor Cyan
    Write-Host "  $Title" -ForegroundColor White -Bold
    Write-Host "========================================`n" -ForegroundColor Cyan
}

function Write-Step {
    param([int]$Step, [int]$Total, [string]$Message)
    Write-Host "[$Step/$Total] $Message" -ForegroundColor Yellow
    Write-Host ("-" * 50) -ForegroundColor DarkGray
}

function Test-Command {
    param([string]$Name, [string]$Command, [string]$WorkingDir)
    Write-Host "执行: $Command" -ForegroundColor DarkGray
    $process = Start-Process -FilePath "cmd.exe" -ArgumentList "/c $Command" -WorkingDirectory $WorkingDir -NoNewWindow -PassThru -Wait
    return $process.ExitCode
}

Write-Header "SMOTA Tool 快速验证脚本"

# 检查 Rust
Write-Step -Step 1 -Total 4 -Message "检查 Rust 编译..."
Set-Location "$ScriptDir\src-tauri"
$exitCode = cargo check 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host "❌ Rust 编译失败" -ForegroundColor Red
    Write-Host $exitCode
    Write-Host "`n请按 Enter 退出..." -ForegroundColor Gray
    Read-Host
    exit 1
}
Write-Host "✅ Rust 编译通过" -ForegroundColor Green

# 安装依赖
Write-Step -Step 2 -Total 4 -Message "安装前端依赖..."
Set-Location $ScriptDir
$exitCode = pnpm install 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host "❌ 依赖安装失败" -ForegroundColor Red
    Write-Host $exitCode
    Write-Host "`n请按 Enter 退出..." -ForegroundColor Gray
    Read-Host
    exit 1
}
Write-Host "✅ 依赖安装完成" -ForegroundColor Green

# 前端构建
Write-Step -Step 3 -Total 4 -Message "构建前端..."
$exitCode = pnpm build 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host "❌ 前端构建失败" -ForegroundColor Red
    Write-Host $exitCode
    Write-Host "`n请按 Enter 退出..." -ForegroundColor Gray
    Read-Host
    exit 1
}
Write-Host "✅ 前端构建成功" -ForegroundColor Green

# 启动开发服务器
Write-Step -Step 4 -Total 4 -Message "启动开发服务器..."

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "  验证完成！" -ForegroundColor White -Bold
Write-Host "========================================" -ForegroundColor Cyan

Write-Host "`n[UI 检查清单] 请在浏览器打开 http://localhost:1420/ 检查：" -ForegroundColor White
Write-Host "  ☐ 窗口标题显示 '串口调试工具'" -ForegroundColor Gray
Write-Host "  ☐ 显示串口连接面板" -ForegroundColor Gray
Write-Host "  ☐ 显示串口参数设置" -ForegroundColor Gray
Write-Host "  ☐ 显示数据接收区域（黑色背景）" -ForegroundColor Gray
Write-Host "  ☐ 显示数据发送区域" -ForegroundColor Gray
Write-Host "  ☐ Naive UI 组件样式正常" -ForegroundColor Gray
Write-Host "  ☐ 中文文字正常显示" -ForegroundColor Gray

Write-Host "`n[功能检查清单]" -ForegroundColor White
Write-Host "  ☐ 刷新串口列表" -ForegroundColor Gray
Write-Host "  ☐ 连接串口" -ForegroundColor Gray
Write-Host "  ☐ 发送数据测试" -ForegroundColor Gray
Write-Host "  ☐ Hex 模式切换" -ForegroundColor Gray
Write-Host "  ☐ 时间戳显示" -ForegroundColor Gray
Write-Host "  ☐ 发送历史功能" -ForegroundColor Gray
Write-Host "  ☐ 热插拔检测" -ForegroundColor Gray

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "  开发服务器已启动" -ForegroundColor White -Bold
Write-Host "  http://localhost:1420/" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "`n按 Ctrl+C 停止服务器，或按 Enter 启动浏览器..." -ForegroundColor Gray

Read-Host

# 尝试启动浏览器
try {
    Start-Process "http://localhost:1420/"
} catch {
    Write-Host "无法自动打开浏览器，请手动访问 http://localhost:1420/" -ForegroundColor Yellow
}

# 启动开发服务器
Write-Host "`n正在启动开发服务器... (按 Ctrl+C 停止)" -ForegroundColor Cyan
pnpm dev
