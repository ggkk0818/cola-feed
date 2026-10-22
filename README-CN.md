# Cola Feed

[English](README.md) | [简体中文](README-CN.md)

一个物联网喂奶记录系统：在 Web 界面点击 **喂奶**，距离上次喂奶的时间就会推送到家中的低功耗电子墨水屏上。

## 项目简介

Cola Feed 用于记录喂奶时间，并把“距上次喂奶已过多久”持续显示在省电的电子墨水屏上。一个轻量的 Node.js 服务负责保存数据、通过 UDP 自动发现局域网内的设备、缓存天气信息，并向显示屏推送更新。系统支持两条显示链路：

- **直连 Wi-Fi** — `cola-epaper`（ESP32-S3 + 4.2 寸三色墨水屏）自带 HTTP 服务，直接接收 `feed-server` 推送的数据。
- **BLE 网关** — `feed-server → cola-ble-gateway（LilyGo T-Dongle-S3）→ cola-epaper-i7（ESP32-C5 + 7.5 寸三色墨水屏）`。网关缓存最新数据并通过 BLE GATT 服务提供给墨水屏，因此电池供电的墨水屏无需连接 Wi-Fi。

```
浏览器 ──HTTP──► feed-server (:3000)
                     │
                     │  UDP :6113  ◄── 发现广播（设备 → 服务器）
                     │  PUT /api/feedData（服务器 → 设备:80）
                     ├──────────────► cola-epaper   （ESP32-S3，4.2 寸墨水屏）
                     └──────────────► cola-ble-gateway（T-Dongle-S3，TFT + TF 卡）
                                             │
                                             │  BLE GATT feed 服务
                                             ▼
                                      cola-epaper-i7（ESP32-C5，7.5 寸墨水屏）
```

## 功能特性

- **喂奶记录** — 一键记录、已过时间显示、JSON 持久化（服务端保留最近 12 条，下发给设备最近 6 条）。
- **两种墨水屏终端** — 直连 Wi-Fi 的 ESP32-S3 版本，以及带传感器和电量监测、仅用 BLE 的 ESP32-C5 版本。
- **自动发现** — 设备联网后立即广播 `{"type":"discover", ...}` 到 UDP 6113，此后每 5 分钟广播一次。
- **主动推送** — 服务器每 5 分钟同步一次各设备，记录喂奶后立即触发同步。
- **天气信息** — 可选的 QWeather 兼容接口（实时天气 + 7 天预报，屏幕显示 4 天），每小时刷新，支持 gzip/deflate/brotli 解码。
- **BLE 传输** — 超过 BLE MTU 的数据采用分片通知（`CF1|<序号>|<总数>|<数据>`，每片最多 160 字节数据）。
- **Wi-Fi 配网** — 网关从 SD 卡提供 Vue 3 单页配网界面，密码使用 RSA-OAEP 加密提交，并支持 Captive Portal 跳转。
- **低功耗界面** — 按区域局刷墨水屏、根据加速度计自动旋转、电量低于 2% 进入低电量模式、空闲时进入轻睡眠。
- **部署方式** — 可直接用 Node.js 运行，也可用 Docker / Docker Compose 部署；Web 界面响应式并适配明暗主题（中文）。

## 目录结构

```
cola-feed/
├── feed-server/                  # Node.js/Express 喂奶 API + Web 界面
│   ├── public/                   # 静态界面（index.html、app.js、style.css）
│   ├── scripts/                  # Docker 镜像构建脚本
│   ├── src/
│   │   ├── config/               # 环境变量配置加载
│   │   ├── mock/                 # 本地调试用的示例数据
│   │   ├── routes/               # REST API 路由
│   │   ├── services/             # 喂奶记录、客户端/UDP 发现、天气服务
│   │   └── utils/                # 日期时间工具
│   ├── data/                     # 运行时 JSON 数据（已 gitignore）
│   ├── server.js                 # 启动入口
│   ├── Dockerfile
│   └── docker-compose.yml
├── cola-ble-gateway/             # ESP32-S3 BLE 网关（LilyGo T-Dongle-S3）
│   ├── boards/dongles3.json      # 自定义板卡定义（16 MB Flash）
│   ├── include/
│   │   ├── config/BoardPins.h    # LCD / TF / I2C / LED 引脚定义
│   │   └── modules/              # bluetooth、display、feed、i2c、input、led、network、storage
│   ├── src/
│   │   ├── main.cpp              # 外设任务 + 通信任务
│   │   └── modules/              # 与 include/modules 对应的实现
│   └── platformio.ini
├── cola-epaper-i7/               # ESP32-C5 墨水屏客户端（仅 BLE）
│   ├── include/modules/
│   │   ├── app/                  # AppRuntime：任务、睡眠、界面状态
│   │   ├── bluetooth/            # BLE 网关客户端与分片重组
│   │   ├── display/              # 7.5 寸三色驱动、中文字库、天气图标
│   │   ├── feed/                 # 数据解析与时间计算
│   │   ├── i2c/                  # SHTC3 / ADXL343 / MAX17048 驱动
│   │   └── weather/              # 天气数据结构
│   ├── src/
│   │   ├── main.cpp              # 入口（AppRuntime）
│   │   └── modules/              # 与 include/modules 对应的实现
│   ├── tools/                    # PowerShell 字库头文件生成脚本
│   └── platformio.ini
├── cola-epaper/                  # ESP32-S3 直连 Wi-Fi 墨水屏（旧版方案）
│   ├── include/modules/          # drawing、feed、network、webserver、wifi
│   ├── include/utils/            # 通用工具
│   ├── src/
│   │   ├── main.cpp              # 入口
│   │   ├── modules/              # 与 include/modules 对应的实现
│   │   └── utils/                # 工具实现
│   └── platformio.ini
├── wifi-setup-web/               # Vue 3 + Vite 配网单页应用
│   ├── src/{api,assets,components,router,utils,views}/
│   ├── index.html
│   └── vite.config.js
├── README.md
└── README-CN.md
```

构建产物与依赖目录（`.pio/`、`node_modules/`、`dist/`、`data/`、`*.log`）不纳入版本管理。

## 子项目说明

### feed-server

系统的核心，也是唯一常驻运行的服务。

- **Feed 服务** — 将记录保存在 `data/feed-records.json`，最多保留 12 条；30 分钟内再次点击喂奶会更新当前记录的 `endTime`/`duration`，而不是新建记录。
- **Client 服务** — 监听 UDP 6113 接收发现广播，将设备信息保存在 `data/clients.json`，每 5 分钟向设备 `PUT /api/feedData` 推送一次，记录喂奶后立即推送。若推送失败且最后一次心跳已超过 10 分钟，设备会被标记为离线。
- **Weather 服务** — 每小时请求一次 QWeather 兼容接口（`/v7/weather/now` 与 `/v7/weather/7d`），自动处理压缩响应，输出 `{ now, daily3d, updateTime }`；未配置时下发 `weatherData: null`。
- **Web 界面** — 中文双标签页：喂奶概览（已过时间与历史记录）和已发现客户端列表；配色跟随系统明暗模式。

**技术栈**：Node.js 18+、Express 5、Docker（Node 20 Alpine）。

### cola-ble-gateway

连接局域网与纯 BLE 墨水屏的网关。

- 使用保存在 NVS 中的凭据连接 Wi-Fi；首次上电，或断网后连续 5 次重连失败时，开启配网 AP（`Cola-gateway-XXXX`，密码 `12345678`，DNS 53），并从 SD 卡 `/html` 提供配网页面。
- 保留 `PUT /api/feedData` 接口，校验并缓存数据；成功入网后通过 BLE 对外提供，进入配网模式时关闭 BLE 服务。
- 驱动 0.96 寸 ST7735 TFT（80×160，横向使用）：显示开机 Logo（取自 `/images/logo.png`）、配网提示、连接结果以及 BLE/缓存状态；板载按键可切换屏幕开关。
- 外设（TF 卡、LCD、I2C、LED）与通信（Wi-Fi + BLE）分别运行在绑核的 FreeRTOS 任务中，保证显示响应。
- BLE 设备名：`Cola-gateway-mesh`。

**技术栈**：PlatformIO、Arduino、ESP32-S3、Arduino BLE、ArduinoJson、mbedTLS、PNGdec。

### cola-epaper-i7

电池供电的 7.5 寸墨水屏终端，刻意不接入 Wi-Fi。

- 扫描网关的 feed 服务并写入请求，每 30 秒轮询一次（10 秒超时），对分片 JSON 响应进行重组。
- 渲染 800×480 三色界面：顶部为时间与电量/充电状态，侧栏为室外天气、室内温湿度和 4 天预报，主区域为距上次喂奶的时长与时间戳。
- 只刷新发生变化的区域，连续 10 次局刷后强制全刷；根据加速度计自动调整方向；电量低于 2% 时显示低电量页面并关闭 BLE。
- 读取 SHTC3（温湿度）、ADXL343（方向 + GPIO 10 活动中断）与 MAX17048（电量计，带趋势判断与充电状态去抖）。

**技术栈**：PlatformIO、Arduino、ESP32-C5（32 MB Flash / 8 MB PSRAM 覆盖配置）、GxEPD2、Adafruit GFX、ArduinoJson、SHTC3。

### cola-epaper

最初的直连 Wi-Fi 版本，适合单设备简单部署。

- 从 NVS（`wifi_cfg` / `ssid` + `pwd`）读取 Wi-Fi 凭据并连接，自带 HTTP 服务，在 4.2 寸三色墨水屏（400×300）上显示喂奶间隔。
- 显示 `NOW`、`XM`、`XH` 或 `XHYM`（相对最新 `endTime`）、WiFi 图标/IP，并在间隔超过白天 90 分钟、夜间 3 小时阈值时于数字下方绘制红色提醒线。
- 最多每分钟检查一次；只有显示字段发生变化且间隔落在 5 分钟整数倍（或切换为 `NOW`/无数据）时才刷新。
- 与网关发送相同的 UDP 发现广播，实际数据推送由 `feed-server` 完成。
- 内置网页只是占位页面，**不提供**设备端 Wi-Fi 配网。

**技术栈**：PlatformIO、Arduino、ESP32-S3-DevKitC-1、GxEPD2（`GxEPD2_420c`）、U8g2、ArduinoJson。

### wifi-setup-web

用于连接 Wi-Fi 的小型单页应用。

- 默认路由即配网表单：网络名称输入框、扫描按钮、密码输入框、连接按钮。
- 点击网络名称输入框弹出选择对话框，首次打开自动扫描；支持手动输入 SSID；扫描按钮带 loading 状态防止重复点击。
- 连接前先获取设备 RSA 公钥，使用 RSA-OAEP（SHA-256）加密密码，再调用 `POST /api/wifi/connect`。
- 构建产物尽量收敛为单个 `assets/app.js`，方便直接拷贝到网关 SD 卡。

**技术栈**：Vue 3、Vue Router（hash 模式）、Axios、jsencrypt、Vite。

## 通信协议

### UDP 发现

设备联网后立即向 `255.255.255.255:6113` 广播，此后每 5 分钟一次：

```json
{
  "type": "discover",
  "chip_id": "a1b2c3d4",
  "device_name": "Cola-ePaper",
  "ip": "192.168.1.50"
}
```

`device_name` 对两种墨水屏均为 `Cola-ePaper`，网关为 `Cola-gateway`；其他名称会被服务器忽略。报文中的 `ip` 仅作展示用途，服务器实际使用 UDP 来源地址。

### HTTP 数据推送

服务器向 `http://<设备IP>/api/feedData` 发送 `PUT` 请求：

```json
{
  "serverTime": "2026-10-22 11:35:00",
  "records": [
    {
      "id": "0123456789abcdef0123456789abcdef",
      "startTime": "2026-10-22 11:20:00",
      "endTime": "2026-10-22 11:35:00",
      "duration": 900
    }
  ],
  "weatherData": {
    "now": { "temp": "21", "icon": "100", "text": "晴" },
    "daily3d": [
      {
        "fxDate": "2026-10-22",
        "tempMax": "23",
        "tempMin": "12",
        "iconDay": "100",
        "textDay": "晴"
      }
    ],
    "updateTime": "2026-10-22T03:30:00.000Z"
  }
}
```

`serverTime` 必须为 `YYYY-MM-DD HH:mm:ss` 格式，记录 ID 必须为 32 位字符串（服务端生成的是 32 位十六进制）。`cola-epaper` 会忽略 `weatherData`；网关会把它经 BLE 转发给墨水屏。网关在配网中或未联网时会以 503 拒绝该请求。

### BLE 数据服务

| 项目 | UUID / 取值 |
| --- | --- |
| 服务 | `4e6a1000-5c4f-45af-8f1f-c41c01ab1000` |
| 数据通知 | `4e6a1001-5c4f-45af-8f1f-c41c01ab1001` |
| 广播写入 | `4e6a1002-5c4f-45af-8f1f-c41c01ab1002` |
| 设备名 | `Cola-gateway-mesh` |

客户端向广播特征写入 `{"device_name":"Cola-ePaper"}`，网关随后按顺序通知缓存的 JSON 数据：

```
CF1|1|7|{"serverTime":"2026-10-22 11:35:00","records":[
CF1|2|7|...
```

每条通知最多携带 160 字节 JSON，间隔 15 ms。客户端要求分片序号连续，出现缺口会丢弃本次数据，并在收到最后一片后才应用负载。

## 快速开始

### 前置要求

- **feed-server**：Node.js 18+（推荐 Node 20）；Docker 可选。
- **固件**：PlatformIO Core（命令行或 IDE）。
- **网关**：一张 FAT 格式的 microSD 卡。
- 所有设备都能访问的 2.4 GHz Wi-Fi 网络。

### 1. 启动服务器

```bash
cd feed-server
npm install
npm run start:dev      # 通过 dotenv-cli 加载 .env.dev
# 或：npm start
```

打开 <http://localhost:3000>。创建 `.env.dev`（或直接导出环境变量），至少包含：

```dotenv
PORT=3000
UDP_PORT=6113
DATA_DIR=./data

# 可选：QWeather 兼容天气接口
WEATHER_API_BASE_URL=https://your-qweather-api-host
WEATHER_API_KEY=your-api-key
WEATHER_LOCATION_ID=101011000
WEATHER_REFRESH_INTERVAL_MS=3600000
WEATHER_REQUEST_TIMEOUT_MS=5000
```

使用 Docker：

```bash
docker compose up -d --build
# 仅构建镜像：npm run docker:build
```

Docker 部署默认不启用天气；如需启用，请在 `docker-compose.yml` 的 `environment:` 中补充 `WEATHER_*` 变量。

### 2. 准备网关 SD 卡

```bash
cd wifi-setup-web
npm install
npm run build
```

将 `dist/` 内的文件按以下结构拷贝到 SD 卡：

```
/html/index.html
/html/assets/app.js
/html/assets/...
/images/logo.png      # 可选：开机 Logo
```

### 3. 烧录网关

```bash
cd cola-ble-gateway
pio run -e T-Dongle-S3
pio run -e T-Dongle-S3 -t upload
```

首次上电后 TFT 会显示 AP 名称和密码。连接 `Cola-gateway-XXXX`（密码 `12345678`），打开 <http://192.168.4.1> 选择家庭 Wi-Fi。设备联网后也可通过其局域网 IP 访问同一页面。

### 4. 烧录 BLE 墨水屏

```bash
cd cola-epaper-i7
pio run
pio run -t upload
```

设备每 30 秒扫描一次网关服务，自身不保存任何配置。ESP32-C5 的平台与工具链来自 `platformio.ini` 中固定的 pioarduino 发布地址，首次编译会自动下载。

### 5. 烧录直连 Wi-Fi 墨水屏（可选）

```bash
cd cola-epaper
pio run -t upload
```

需要提前在 NVS 的 `wifi_cfg` 命名空间中写入 `ssid` 与 `pwd`。该版本没有配网界面，目前需要借助一个临时固件/工具写入（或自行添加配网路由）。联网后设备会广播自身 IP 并开始接收推送。

### 6. 开始使用

打开 Web 界面并点击 **喂奶**。服务器会更新或新建当前记录，立即推送到所有已知设备，墨水屏会在数秒内完成刷新。

## 配置说明

### feed-server 环境变量

| 变量 | 默认值 | 说明 |
| --- | --- | --- |
| `PORT` | `3000` | API 与 Web 界面的 HTTP 端口 |
| `UDP_PORT` | `6113` | UDP 发现端口（需与固件一致） |
| `DATA_DIR` | `feed-server/data` | `feed-records.json` 与 `clients.json` 的存放目录 |
| `WEATHER_API_BASE_URL` | 空 | QWeather 兼容接口地址；为空时关闭天气功能 |
| `WEATHER_API_KEY` | 空 | 接口密钥 |
| `WEATHER_LOCATION_ID` | 空 | 地区 ID（也接受 `WEATHER_LOCATION` 作为别名） |
| `WEATHER_REFRESH_INTERVAL_MS` | `3600000` | 天气缓存刷新间隔 |
| `WEATHER_REQUEST_TIMEOUT_MS` | `5000` | 天气请求超时 |

### 固件配置

- **cola-ble-gateway** — Wi-Fi 凭据保存在 NVS；AP 名称后缀、AP 密码、SD 网页根目录（`/html`）以及内置 RSA 密钥对均为 `src/modules/network/WifiProvisioningModule.cpp` 中的常量；引脚定义在 `include/config/BoardPins.h`。
- **cola-epaper-i7** — 屏幕引脚与布局常量在 `src/modules/display/DisplayModule.cpp`；BLE UUID 与轮询超时在 `src/modules/bluetooth/BleGatewayClient.cpp`。
- **cola-epaper** — 屏幕引脚在 `src/modules/drawing/DrawingModule.cpp`；Wi-Fi 键名在 `src/modules/wifi/WifiModule.cpp`。

### 重新生成中文字库（可选）

`cola-epaper-i7` 已内置 `FontCN12/16/24/32/64/96` 头文件。`tools/` 下的脚本可将源 `.c` 点阵数据转换为头文件/源文件；运行前请先修改脚本参数默认值（其中包含原作者本机的绝对路径）。

## API 参考

### feed-server（端口 3000）

| 方法 | 路径 | 说明 |
| --- | --- | --- |
| `POST` | `/api/feed` | 记录一次喂奶（30 分钟内复用当前记录）并立即触发设备同步 |
| `GET` | `/api/status` | 最新记录与完整历史 |
| `GET` | `/api/clients` | 已发现设备及其心跳/更新时间、在线状态 |

响应统一使用 `code`/`data`，并附带历史遗留的 `messagge` 字段（注意拼写）。示例：

```json
{
  "code": 200,
  "data": {
    "lastFeed": {
      "id": "0123456789abcdef0123456789abcdef",
      "startTime": "2026-10-22 11:20:00",
      "endTime": "2026-10-22 11:35:00",
      "duration": 900
    },
    "records": [ "..." ]
  },
  "messagge": "成功"
}
```

### 设备 HTTP 接口

| 方法 | 路径 | 可用条件 | 说明 |
| --- | --- | --- | --- |
| `GET` | `/` | 始终 | 网关返回 SD 卡 `/html` 中的配网页面；cola-epaper 返回占位页面 |
| `PUT` | `/api/feedData` | 网关已联网；cola-epaper 已联网 | 接收喂奶与天气数据 |
| `GET` | `/api/wifi/list` | 仅网关配网模式 | 扫描可见 SSID |
| `GET` | `/api/rsa/public_key` | 仅网关配网模式 | 返回用于加密密码的 PEM 公钥 |
| `POST` | `/api/wifi/connect` | 仅网关配网模式 | 提交 `{ "ssid", "password" }`；密码可为 RSA-OAEP 密文，也兼容明文回退 |

网关还会响应 `/generate_204`、`/hotspot-detect.html`、`/connecttest.txt`、`/ncsi.txt`、`/fwlink` 并跳转到 AP 地址，使手机和电脑自动弹出配网页面。非配网模式下，Wi-Fi 相关接口返回 403。

## 硬件要求

| 项目 | MCU / 开发板 | 显示屏 | 其他硬件 |
| --- | --- | --- | --- |
| `cola-epaper` | ESP32-S3-DevKitC-1 | 4.2 寸三色墨水屏（`GxEPD2_420c`，400×300） | — |
| `cola-epaper-i7` | ESP32-C5-DevKitC-1（32 MB Flash，8 MB PSRAM） | 7.5 寸三色墨水屏（`GDEY075Z08`，800×480） | SHTC3、ADXL343、MAX17048、电池 |
| `cola-ble-gateway` | LilyGo T-Dongle-S3（ESP32-S3，16 MB Flash） | 0.96 寸 ST7735 TFT（80×160，横向使用） | microSD（SD_MMC）、DotStar RGB 灯、按键 |

## 使用说明与限制

- 内置 Web 界面仅提供中文；时间按服务器本地时区格式化为 `YYYY-MM-DD HH:mm:ss`（Docker 中 `TZ` 默认 `Asia/Shanghai`）。
- 天气功能完全可选。未配置时屏幕显示 `--` 与未知图标，其余功能不受影响。
- 下发给设备的是最近 6 条记录，服务端最多保留 12 条。
- 短暂断网时网关会在后台重连，BLE 链路仍然可用；进入配网模式后 BLE 会被关闭。此时墨水屏继续显示最后一次的画面，时钟会基于最后一次服务端时间继续走时。
- 配网 AP 密码（`12345678`）与配网 RSA 密钥对均为开发默认值，部署到可信网络之外前请务必更换。
- 服务器没有鉴权，默认运行在可信局域网内，请勿直接暴露到公网。

## 许可证

ISC
