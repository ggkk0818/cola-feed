# Cola Feed

[English](README.md) | [简体中文](README-CN.md)

一个基于电子墨水屏和Web数据管理的物联网信息显示系统。

## 项目简介

Cola Feed 是一个综合性的物联网解决方案，结合了硬件电子墨水屏显示和基于Web的信息管理系统。它能够实时在电子墨水屏上显示数据，并通过Web界面进行集中的信息管理。

## 功能特性

- **电子墨水屏显示**: 基于ESP32-S3的低功耗电子墨水屏显示
- **Web管理界面**: 内置Web界面用于信息管理
- **REST API**: 完整的信息数据操作API
- **UDP发现**: 自动设备发现和通信
- **Docker支持**: 通过Docker容器轻松部署
- **黑暗主题**: 支持暗黑模式的现代化UI

## 项目结构

```
cola-feed/
├── cola-epaper/          # ESP32-S3电子墨水屏固件
│   ├── src/              # 主源代码
│   │   ├── modules/      # 功能模块（WiFi、网络、绘图等）
│   │   └── utils/        # 工具函数
│   └── platformio.ini    # PlatformIO配置
└── feed-server/          # Node.js Web服务器
    ├── src/              # 服务器源代码
    │   ├── routes/       # API路由
    │   └── services/     # 业务逻辑服务
    ├── public/           # Web界面 (HTML/CSS/JS)
    ├── data/             # 数据存储
    └── docker/           # Docker配置
```

## 子项目

### cola-epaper

ESP32-S3电子墨水屏固件，包含以下模块：
- **WiFi模块**: 网络连接管理
- **网络模块**: UDP发现和通信
- **Web服务器模块**: 设备配置界面
- **Feed控制器**: 信息数据处理和显示逻辑
- **绘图模块**: 电子墨水屏渲染

**技术栈**: PlatformIO, Arduino框架, ESP32-S3

### feed-server

提供信息管理API和UI的Web服务器：
- **Feed服务**: 信息数据管理和存储
- **客户端服务**: UDP客户端发现和通信
- **REST API**: 完整的信息数据CRUD操作
- **Web界面**: 现代化响应式界面

**技术栈**: Node.js, Express.js, Docker

## 快速开始

### 前置要求

- cola-epaper: PlatformIO IDE
- feed-server: Node.js 18+ 和 Docker

### 安装

#### cola-epaper (ESP32-S3固件)

```bash
cd cola-epaper
# 安装PlatformIO依赖
pio upgrade
pio install espressif32

# 编译并上传
pio run --target upload
```

#### feed-server (Web服务器)

```bash
cd feed-server
npm install

# 开发模式
npm start

# Docker部署
npm run docker:build
docker-compose up
```

## 使用说明

1. **硬件设置**: 将cola-epaper固件烧录到ESP32-S3
2. **网络配置**: 连接设备的WiFi热点进行配置
3. **服务器部署**: 使用Docker部署feed-server
4. **设备发现**: 设备会自动发现并连接到服务器
5. **信息管理**: 访问Web界面 `http://localhost:3000`

## 配置

### 环境变量 (feed-server)

- `PORT`: 服务器端口（默认: 3000）
- `UDP_PORT`: UDP发现端口（默认: 6113）
- `DATA_DIR`: 数据存储目录（默认: ./data）

### 设备配置 (cola-epaper)

访问设备的Web界面进行WiFi和服务器配置。

## 硬件要求

- ESP32-S3开发板
- 电子墨水屏（GxEPD2库支持）
- USB-C数据线（供电和编程）

## 许可证

ISC

