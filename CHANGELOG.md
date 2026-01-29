# Changelog

本文件记录项目的所有重要变更。

格式基于 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.0.0/)，
版本号遵循 [Semantic Versioning](https://semver.org/spec/v2.0.0.html)。

## [Unreleased]

### Added
- smOTA宏定义配置
- 密钥生成工具（ECDSA-P256 + AES-128）
- 项目文档（需求、结构、配置、密钥管理、协议规范）

### Planned

- 完整的 OTA 状态机实现
- 支持断点续传
- 支持分页发送处理
- PC端快速编译验证的demo
- Flash 模拟读写逻辑
- 固件包加载和解析
- ECDSA 签名验证集成
- AES 解密传输模拟
- 版本比较和防回滚
- 错误处理和日志输出
- 测试固件生成工具
