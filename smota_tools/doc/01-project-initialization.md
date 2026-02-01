# 串口工具开发文档

本文档记录 SMOTA 串口工具的完整开发过程，供单片机开发人员参考。

---

## 第一阶段：项目初始化

### 1.1 阶段目标

使用 Tauri 框架创建一个 Vue 3 + TypeScript 的桌面应用基础项目，为后续串口通信功能开发做准备。

### 1.2 为什么选择 Tauri

| 框架 | 优点 | 缺点 |
|------|------|------|
| **Tauri** | 体积小（仅约 5MB）、Rust 后端、性能高、安全 | 需要 Windows WebView2 |
| Electron | 生态丰富、跨平台成熟 | 体积大（100MB+）、内存占用高 |
| Qt | 原生性能、强跨平台 | 学习曲线陡、商业授权问题 |

**选择 Tauri 的理由**：
- 串口工具不需要复杂的 Web 功能
- 体积小、分发方便
- Rust 后端处理串口通信性能好

### 1.3 创建项目

#### 1.3.1 使用 cargo create-tauri-app 命令

```bash
# 进入 smota_tools 目录
cd C:\Users\w1545\Desktop\SMOTA\smota_tools

# 创建项目（指定模板为 vue-ts，即 Vue3 + TypeScript）
cargo create-tauri-app serial-tool --template vue-ts --manager pnpm --yes
```

**命令参数说明**：
| 参数 | 说明 |
|------|------|
| `serial-tool` | 项目名称，也是目录名 |
| `--template vue-ts` | 使用 Vue3 + TypeScript 模板 |
| `--manager pnpm` | 使用 pnpm 作为包管理器 |
| `--yes` | 跳过交互式提问，使用默认值 |

#### 1.3.2 安装前端依赖

```bash
cd serial-tool
pnpm install
```

**pnpm 与 npm/yarn 的区别**：
- pnpm 使用硬链接，磁盘空间占用更小
- 安装速度更快
- 依赖管理更严格

### 1.4 项目结构说明

```
serial-tool/
├── src/                          # 前端代码（Vue3）
│   ├── assets/                   # 静态资源
│   │   └── main.css             # 全局样式（Tailwind CSS）
│   ├── plugins/                  # Vue 插件
│   │   └── naive-ui.ts          # Naive UI 组件库配置
│   ├── App.vue                  # 根组件
│   ├── main.ts                  # 应用入口
│   └── vite-env.d.ts            # Vite 类型声明
├── src-tauri/                    # 后端代码（Rust）
│   ├── src/
│   │   ├── main.rs              # Rust 程序入口
│   │   └── lib.rs               # Tauri 命令定义
│   ├── Cargo.toml               # Rust 依赖配置
│   ├── tauri.conf.json          # Tauri 应用配置
│   └── capabilities/            # 权限配置
├── public/                       # 公共静态资源
├── package.json                  # 前端依赖配置
├── vite.config.ts               # Vite 构建配置
└── tsconfig.json                # TypeScript 配置
```

### 1.5 安装的依赖包

#### 1.5.1 前端依赖（package.json）

| 包名 | 版本 | 用途 |
|------|------|------|
| `vue` | 3.5.27 | 前端框架 |
| `@tauri-apps/api` | 2.9.1 | Tauri 前端 API |
| `naive-ui` | 2.43.2 | Vue3 组件库 |
| `@vueuse/core` | 14.1.0 | Vue 组合式工具库 |
| `tailwindcss` | 4.1.18 | CSS 框架 |
| `vite` | 6.4.1 | 构建工具 |
| `typescript` | 5.6.3 | 类型系统 |

#### 1.5.2 开发依赖

| 包名 | 用途 |
|------|------|
| `@tauri-apps/cli` | Tauri 命令行工具 |
| `@vitejs/plugin-vue` | Vite 的 Vue 插件 |
| `vue-tsc` | Vue 的 TypeScript 检查 |
| `postcss` | CSS 处理工具 |
| `autoprefixer` | CSS 前缀自动补全 |

### 1.6 配置说明

#### 1.6.1 Tailwind CSS 配置

**文件**: `src/assets/main.css`

```css
@import "tailwindcss";
```

**说明**: Tailwind CSS 4.0 使用新的 `@import` 语法，比旧版本的配置更简单。

#### 1.6.2 Naive UI 插件配置

**文件**: `src/plugins/naive-ui.ts`

```typescript
import { type Plugin } from "vue";
import {
  create,
  NConfigProvider,
  NMessageProvider,
  NNotificationProvider,
  NDialogProvider,
} from "naive-ui";

export const naiveUiPlugin: Plugin = {
  install(app) {
    const naive = create({
      components: [
        NConfigProvider,
        NMessageProvider,
        NNotificationProvider,
        NDialogProvider,
      ],
    });
    app.use(naive);
  },
};
```

**说明**: 创建一个 Naive UI 插件，包含消息通知、对话框等全局组件。

#### 1.6.3 应用入口

**文件**: `src/main.ts`

```typescript
import { createApp } from "vue";
import "./assets/main.css";        // 引入 Tailwind CSS
import { naiveUiPlugin } from "./plugins/naive-ui";
import App from "./App.vue";

const app = createApp(App);
app.use(naiveUiPlugin);           // 注册 Naive UI 插件
app.mount("#app");                 // 挂载到 index.html 的 #app
```

### 1.7 Git 版本控制配置

#### 1.7.1 初始化仓库

```bash
cd serial-tool
git init
git branch -m main                # 重命名为 main 分支
```

#### 1.7.2 提交代码

```bash
git add -A
git commit -m "feat: 初始化 Tauri 串口工具项目"
```

**提交信息规范**：
| 类型 | 说明 | 示例 |
|------|------|------|
| `feat` | 新功能 | `feat: 添加串口列表获取功能` |
| `fix` | 修复 bug | `fix: 修复串口关闭时的内存泄漏` |
| `docs` | 文档更新 | `docs: 更新串口参数配置说明` |
| `refactor` | 重构 | `refactor: 重构串口通信模块` |

### 1.8 如何运行和调试

#### 1.8.1 开发模式运行

```bash
cd serial-tool
pnpm tauri dev
```

**说明**：
- 开发模式会启动 Vite 开发服务器（热更新）
- 自动打开应用窗口
- 修改代码后会自动重新编译

#### 1.8.2 构建生产版本

```bash
cd serial-tool
pnpm tauri build
```

**说明**：
- 生成可执行文件（.exe）
- 位于 `src-tauri/target/release/bundle/msi/` 目录

#### 1.8.3 查看 Tauri 环境信息

```bash
cd serial-tool
cargo tauri info
```

**输出示例**：
```
[✔] Environment
    - OS: Windows 10.0.26100 x86_64 (X64)
    ✔ WebView2: 144.0.3719.93
    ✔ MSVC: Visual Studio 生成工具 2026
    ✔ rustc: 1.93.0
    ✔ cargo: 1.93.0
    - node: 20.12.2
    - pnpm: 10.28.2
```

### 1.9 常见问题

#### Q1: `pnpm tauri dev` 报错 "WebView2 not found"

**解决方法**：
1. 下载安装 WebView2 运行时：https://developer.microsoft.com/en-us/microsoft-edge/webview2/
2. 或使用 Edge 浏览器版本（Windows 11 自带）

#### Q2: `cargo tauri info` 报错 "link.exe not found"

**解决方法**：
1. 确保已安装 Visual Studio Build Tools
2. 将 cl.exe 路径加入系统 PATH：
   ```
   C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64
   ```

#### Q3: Tailwind CSS 不生效

**解决方法**：
1. 确保 `main.css` 内容正确：`@import "tailwindcss";`
2. 确保 `main.ts` 引入了样式文件
3. 重启开发服务器：`pnpm tauri dev`

### 1.10 下一步

下一阶段将实现 Rust 后端的串口管理模块，包括：
- 串口列表获取
- 串口打开/关闭
- 串口参数配置
- 数据发送/接收

---

## 文档版本

| 版本 | 日期 | 说明 |
|------|------|------|
| v1.0 | 2025-01-31 | 初始版本，项目初始化说明 |
