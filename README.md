# Cola Feed

[English](README.md) | [简体中文](README-CN.md)

An IoT baby-feeding tracker: call the feed API and the time since the last feeding is pushed to low-power e-paper displays around the home.

## Overview

Cola Feed records feeding sessions and keeps the elapsed time visible on battery-friendly e-paper screens. A small Node.js server owns the data, discovers devices on the LAN over UDP, caches weather information, and pushes updates to the displays. Two display paths are supported:

- **Direct Wi-Fi** — `cola-epaper` (ESP32-S3 + 4.2" three-color e-paper) runs its own HTTP server and receives data straight from `feed-server`.
- **BLE gateway** — `feed-server → cola-ble-gateway (LilyGo T-Dongle-S3) → cola-epaper-i7 (ESP32-C5 + 7.5" three-color e-paper)`. The gateway caches the latest payload and serves it over a BLE GATT service, so the battery-powered e-paper never needs Wi-Fi.

```
Browser ──HTTP──► feed-server (:3000)
                     │
                     │  UDP :6113  ◄── discovery broadcast (device → server)
                     │  PUT /api/feedData (server → device:80)
                     ├──────────────► cola-epaper   (ESP32-S3, 4.2" e-paper)
                     └──────────────► cola-ble-gateway (T-Dongle-S3, TFT + TF card)
                                             │
                                             │  BLE GATT feed service
                                             ▼
                                      cola-epaper-i7 (ESP32-C5, 7.5" e-paper)
```

## Features

- **Feeding log** — elapsed timer, persistent JSON history (last 12 records, last 6 pushed to devices).
- **Two e-paper targets** — a direct-Wi-Fi ESP32-S3 board and a BLE-only ESP32-C5 board with sensors and battery monitoring.
- **Automatic discovery** — devices broadcast `{"type":"discover", ...}` to UDP 6113 on connect and every 5 minutes.
- **Push updates** — the server refreshes each device every 5 minutes, and immediately after a feeding is recorded.
- **Weather** — optional QWeather-compatible integration (current conditions + 7-day forecast, 4 days rendered) with hourly refresh and gzip/deflate/brotli decoding.
- **BLE transport** — chunked notifications (`CF1|<index>|<total>|<payload>`, 160-byte chunks) for payloads larger than the BLE MTU.
- **Wi-Fi provisioning** — Vue 3 single-page app served from the gateway's SD card, with RSA-OAEP encrypted password submission and captive-portal redirects.
- **Power aware UI** — partial e-paper refresh per screen region, accelerometer-based orientation, low-battery mode below 2%, light sleep between updates.
- **Deployment** — plain Node.js or Docker / Docker Compose, with a light/dark responsive web UI (Chinese).

## Repository Layout

```
cola-feed/
├── feed-server/                  # Node.js/Express feed API + web UI
│   ├── public/                   # Static UI (index.html, app.js, style.css)
│   ├── scripts/                  # Docker image build helper
│   ├── src/
│   │   ├── config/               # Environment configuration loader
│   │   ├── mock/                 # Sample feed payload for local testing
│   │   ├── routes/               # REST API routes
│   │   ├── services/             # Feed, client/UDP discovery, weather
│   │   └── utils/                # Date/time helpers
│   ├── data/                     # Runtime JSON storage (gitignored)
│   ├── server.js                 # Entry point
│   ├── Dockerfile
│   └── docker-compose.yml
├── cola-ble-gateway/             # ESP32-S3 BLE gateway (LilyGo T-Dongle-S3)
│   ├── boards/dongles3.json      # Custom board definition (16 MB flash)
│   ├── include/
│   │   ├── config/BoardPins.h    # LCD / TF / I2C / LED pin map
│   │   └── modules/              # bluetooth, display, feed, i2c, input, led, network, storage
│   ├── src/
│   │   ├── main.cpp              # Peripheral task + communication task
│   │   └── modules/              # Implementations mirroring include/modules
│   └── platformio.ini
├── cola-epaper-i7/               # ESP32-C5 e-paper client (BLE only)
│   ├── include/modules/
│   │   ├── app/                  # AppRuntime: tasks, sleep, display state
│   │   ├── bluetooth/            # BLE gateway client + chunk assembly
│   │   ├── display/              # 7.5" 3-color driver, CJK fonts, weather icons
│   │   ├── feed/                 # Feed payload parsing and time math
│   │   ├── i2c/                  # SHTC3 / ADXL343 / MAX17048 drivers
│   │   └── weather/              # Weather data structures
│   ├── src/
│   │   ├── main.cpp              # Entry point (AppRuntime)
│   │   └── modules/              # Implementations mirroring include/modules
│   ├── tools/                    # PowerShell font-header generators
│   └── platformio.ini
├── cola-epaper/                  # ESP32-S3 direct-Wi-Fi e-paper (legacy variant)
│   ├── include/modules/          # drawing, feed, network, webserver, wifi
│   ├── include/utils/            # Small helpers
│   ├── src/
│   │   ├── main.cpp              # Entry point
│   │   ├── modules/              # Implementations mirroring include/modules
│   │   └── utils/                # Implementation of helpers
│   └── platformio.ini
├── wifi-setup-web/               # Vue 3 + Vite Wi-Fi provisioning SPA
│   ├── src/{api,assets,components,router,utils,views}/
│   ├── index.html
│   └── vite.config.js
├── README.md
└── README-CN.md
```

Build outputs and dependencies (`.pio/`, `node_modules/`, `dist/`, `data/`, `*.log`) are intentionally not tracked.

## Components

### feed-server

The central brain and the only long-running service.

- **Feed service** — stores records in `data/feed-records.json`, keeps the newest 12, and coalesces rapid presses: a feed within 30 minutes of the last record updates that record's `endTime`/`duration` instead of creating a new one.
- **Client service** — listens on UDP 6113 for discovery broadcasts, stores known devices in `data/clients.json`, and pushes to each device's `PUT /api/feedData` every 5 minutes (immediately after a feed). If a push fails and the last heartbeat is more than 10 minutes old, the device is marked offline.
- **Weather service** — polls a QWeather-compatible API (`/v7/weather/now` and `/v7/weather/7d`) once an hour, tolerates compressed responses, and returns `{ now, daily3d, updateTime }`. Without configuration, `weatherData` is sent as `null`.
- **Web UI** — Chinese two-tab interface: feeding overview with elapsed time and history, plus the discovered client list. Colors follow the OS light/dark preference.

**Stack**: Node.js 18+, Express 5, Docker (Node 20 Alpine).

### cola-ble-gateway

A bridge between the LAN and the BLE-only e-paper.

- Connects to Wi-Fi with credentials stored in NVS; on first boot, or after a dropped link and five failed reconnect attempts, it opens a captive-portal AP (`Cola-gateway-XXXX`, password `12345678`, DNS 53) and serves the provisioning SPA from `/html` on the SD card.
- Keeps `PUT /api/feedData` for the feed server, validates and caches the payload, and exposes it over BLE once it has joined a network; the BLE service is torn down in provisioning mode.
- Drives a 0.96" ST7735 TFT (80×160, used rotated) with boot logo (from `/images/logo.png`), provisioning instructions, connection results, and BLE/cache status; the on-board button toggles the screen.
- Runs peripherals (TF card, LCD, I2C, LED) and communications (Wi-Fi + BLE) in pinned FreeRTOS tasks to keep the display responsive.
- BLE device name: `Cola-gateway-mesh`.

**Stack**: PlatformIO, Arduino, ESP32-S3, Arduino BLE, ArduinoJson, mbedTLS, PNGdec.

### cola-epaper-i7

The battery-powered 7.5" display, intentionally Wi-Fi-free.

- Scans for the gateway's feed service, writes a request, and reassembles the chunked JSON response on a 30-second poll cycle (10-second timeout).
- Renders an 800×480 three-color layout: clock and battery/charging status on top, outdoor weather, indoor temperature/humidity, and a 4-day forecast in the sidebar, and the elapsed feeding time plus timestamp in the main area.
- Refreshes only dirty regions, forcing a full refresh after 10 consecutive partial refreshes; rotates the layout using the accelerometer; enters a low-power screen and disables BLE below 2% battery.
- Reads SHTC3 (temperature/humidity), ADXL343 (orientation + activity interrupt on GPIO 10), and MAX17048 (fuel gauge, with trend-based charging detection and debouncing).

**Stack**: PlatformIO, Arduino, ESP32-C5 (32 MB flash / 8 MB PSRAM overrides), GxEPD2, Adafruit GFX, ArduinoJson, SHTC3.

### cola-epaper

The original direct-Wi-Fi variant, kept for simpler single-device setups.

- Connects to Wi-Fi from NVS (`wifi_cfg` / `ssid` + `pwd`), hosts an HTTP server, and renders the feed interval on a 4.2" three-color e-paper (400×300).
- Shows `NOW`, `XM`, `XH`, or `XHYM` since the latest `endTime`, the WiFi icon/IP, and a red underline once the interval exceeds the daytime (90 min) or nighttime (3 h) threshold.
- Checks at most once per minute and refreshes only when a displayed field changes and the interval lands on a 5-minute boundary (or switches to `NOW`/no-data).
- Broadcasts the same UDP discovery packet as the gateway and delegates the actual data push to `feed-server`.
- Its built-in web page is a placeholder; it does **not** provide on-device Wi-Fi provisioning.

**Stack**: PlatformIO, Arduino, ESP32-S3-DevKitC-1, GxEPD2 (`GxEPD2_420c`), U8g2, ArduinoJson.

### wifi-setup-web

A small SPA for joining a Wi-Fi network.

- Default route is the setup form: SSID field, scan button, password field, connect button.
- Opening the SSID field shows a dialog that scans on first open, supports manual SSID entry, and guards the scan button with a loading state.
- Fetches the device's RSA public key and encrypts the password with RSA-OAEP (SHA-256) before `POST /api/wifi/connect`.
- Built as a nearly single-bundle output (`assets/app.js`) for easy copying to the gateway SD card.

**Stack**: Vue 3, Vue Router (hash history), Axios, jsencrypt, Vite.

## Protocols

### UDP discovery

Devices broadcast to `255.255.255.255:6113` right after connecting and then every 5 minutes:

```json
{
  "type": "discover",
  "chip_id": "a1b2c3d4",
  "device_name": "Cola-ePaper",
  "ip": "192.168.1.50"
}
```

`device_name` is `Cola-ePaper` for both e-paper variants and `Cola-gateway` for the gateway. The server ignores any other name. The reported `ip` is informational; the server uses the UDP source address.

### HTTP feed push

The server sends `PUT http://<device-ip>/api/feedData` with:

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

`serverTime` must be `YYYY-MM-DD HH:mm:ss`; record IDs must be 32 characters (the server generates 32 hex characters). `cola-epaper` ignores `weatherData`; the gateway forwards it to the e-paper over BLE. The gateway rejects the request with 503 while provisioning or disconnected.

### BLE feed service

| Item | UUID / value |
| --- | --- |
| Service | `4e6a1000-5c4f-45af-8f1f-c41c01ab1000` |
| Feed data (notify) | `4e6a1001-5c4f-45af-8f1f-c41c01ab1001` |
| Broadcast write | `4e6a1002-5c4f-45af-8f1f-c41c01ab1002` |
| Device name | `Cola-gateway-mesh` |

The client writes `{"device_name":"Cola-ePaper"}` to the broadcast characteristic. The gateway then notifies the cached JSON payload in sequence:

```
CF1|1|7|{"serverTime":"2026-10-22 11:35:00","records":[
CF1|2|7|...
```

Each notification carries at most 160 bytes of JSON and is sent every 15 ms. The client requires sequential chunk indexes, rejects gaps, and applies the payload only after the final chunk.

## Quick Start

### Prerequisites

- **feed-server**: Node.js 18+ (Node 20 recommended); Docker optional.
- **Firmware**: PlatformIO Core (CLI or IDE).
- **Gateway**: a FAT-formatted microSD card.
- A 2.4 GHz Wi-Fi network reachable by all devices.

### 1. Run the server

```bash
cd feed-server
npm install
npm run start:dev      # loads .env.dev via dotenv-cli
# or: npm start
```

Open <http://localhost:3000> (default accounts: `admin` / `admin`, `mom` / `mom`). Create `.env.dev` (or export the variables) with at least:

```dotenv
PORT=3000
UDP_PORT=6113
DATA_DIR=./data

# Optional weather integration (QWeather-compatible)
WEATHER_API_BASE_URL=https://your-qweather-api-host
WEATHER_API_KEY=your-api-key
WEATHER_LOCATION_ID=101011000
WEATHER_REFRESH_INTERVAL_MS=3600000
WEATHER_REQUEST_TIMEOUT_MS=5000
```

With Docker:

```bash
docker compose up -d --build
# optional image-only build: npm run docker:build
```

Weather is disabled in Docker by default; add the `WEATHER_*` variables to the `environment:` block of `docker-compose.yml` to enable it.

### 2. Prepare the gateway SD card

```bash
cd wifi-setup-web
npm install
npm run build
```

Copy the contents of `dist/` to the SD card as:

```
/html/index.html
/html/assets/app.js
/html/assets/...
/images/logo.png      # optional boot logo
```

### 3. Flash the gateway

```bash
cd cola-ble-gateway
pio run -e T-Dongle-S3
pio run -e T-Dongle-S3 -t upload
```

On first boot the TFT shows the AP name and password. Join `Cola-gateway-XXXX` with `12345678`, open <http://192.168.4.1>, and choose the home Wi-Fi. The same page can also be reached at the device's LAN IP after it connects.

### 4. Flash the BLE e-paper

```bash
cd cola-epaper-i7
pio run
pio run -t upload
```

The board scans for the gateway service every 30 seconds. No configuration is stored on this device. The ESP32-C5 platform and toolchain are downloaded from the pioarduino release URL pinned in `platformio.ini` on the first build.

### 5. Flash the direct-Wi-Fi e-paper (optional)

```bash
cd cola-epaper
pio run -t upload
```

Wi-Fi credentials must be present in NVS under namespace `wifi_cfg` with keys `ssid` and `pwd`. This variant has no provisioning UI, so write them with a small utility sketch (or add a provisioning route) first. Once connected, the device broadcasts its IP and starts receiving pushes.

### 6. Feed the baby

Open the web UI and log in to view feeding records and clients; record a feeding via `POST /api/feed` (the web UI no longer exposes a feed button). The server updates or creates the current record, pushes to all known devices immediately, and the e-paper refreshes within seconds.

## Configuration

### feed-server environment variables

| Variable | Default | Description |
| --- | --- | --- |
| `PORT` | `3000` | HTTP port for the API and web UI |
| `UDP_PORT` | `6113` | UDP discovery port (must match firmware) |
| `DATA_DIR` | `feed-server/data` | Directory for `feed-records.json`, `clients.json`, and `users.json` |
| `WEATHER_API_BASE_URL` | empty | QWeather-compatible API base URL; weather is disabled when empty |
| `WEATHER_API_KEY` | empty | API key |
| `WEATHER_LOCATION_ID` | empty | Location ID (`WEATHER_LOCATION` is accepted as an alias) |
| `WEATHER_REFRESH_INTERVAL_MS` | `3600000` | Weather cache refresh interval |
| `WEATHER_REQUEST_TIMEOUT_MS` | `5000` | Weather request timeout |

### Firmware configuration

- **cola-ble-gateway** — Wi-Fi credentials are stored in NVS. The AP SSID suffix, AP password, SD web root (`/html`), and the embedded RSA key pair are constants in `src/modules/network/WifiProvisioningModule.cpp`. Pin assignments live in `include/config/BoardPins.h`.
- **cola-epaper-i7** — display pins and layout constants are in `src/modules/display/DisplayModule.cpp`; BLE UUIDs and poll timeouts are in `src/modules/bluetooth/BleGatewayClient.cpp`.
- **cola-epaper** — display pins are in `src/modules/drawing/DrawingModule.cpp`; Wi-Fi keys are read in `src/modules/wifi/WifiModule.cpp`.

### Regenerating CJK fonts (optional)

`cola-epaper-i7` ships pre-generated `FontCN12/16/24/32/64/96` headers. The scripts in `tools/` convert a source `.c` bitmap dump into the header/source pair; edit their parameter defaults (they contain absolute paths from the original author) before running.

## API Reference

### feed-server (port 3000)

| Method | Path | Description |
| --- | --- | --- |
| `POST` | `/api/feed` | Record a feeding (creates a record, or extends the current one within 30 minutes) and trigger an immediate device sync; anonymous |
| `GET` | `/api/status` | Latest record and full history; requires an admin-type role |
| `GET` | `/api/clients` | Discovered devices with heartbeat/update times and online flag; requires an admin-type role |
| `GET` | `/api/auth/public-key` | PEM public key used to encrypt the login password |
| `POST` | `/api/auth/login` | Submit `{ "userName", "password": "<RSA-OAEP ciphertext>" }`; returns a Bearer token |
| `GET` | `/api/auth/me` | Current logged-in user |
| `POST` | `/api/auth/logout` | Revoke the current token |

All responses use `code`/`data`; the server also keeps a legacy `messagge` field (note the spelling). Example:

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

### Authentication and users

The web login page is `/login.html`. It first fetches `/api/auth/public-key`, encrypts the password in the browser with RSA-OAEP (SHA-256), and submits the ciphertext. The server stores only a salted scrypt hash in the `password` field, formatted as `scrypt$N$r$p$salt$hash`.

- `GET /api/status` and `GET /api/clients` require `Authorization: Bearer <token>` and an admin-type role (1, 3, or 5). Otherwise the server returns HTTP 401 with `reason` set to `UNAUTHORIZED` or `FORBIDDEN`.
- `POST /api/feed` stays anonymous so existing LAN callers keep working.
- Tokens live in server memory and expire after 7 days by default; a restart invalidates them. Per-channel lifetimes are already modeled: after a future QR-code login verifies the user, call `issueToken(user, { channel: 'qr' })` for a 365-day token.
- Accounts live in `data/users.json`. Roles: 1 administrator, 2 feed user, 3 feed admin, 4 food user, 5 food admin.
- Default accounts: `admin` (roles `[1,2,3,4,5]`, password `admin`) and `mom` (roles `[2,4]`, password `mom`). Replace the default passwords before deployment; there is no password-change API yet, so update the scrypt hash in `data/users.json` directly.
- An existing Docker data volume does not gain `users.json` automatically. When upgrading, copy `data/users.json` into the volume (or recreate the volume), otherwise no account can log in.

### Device HTTP endpoints

| Method | Path | Availability | Description |
| --- | --- | --- | --- |
| `GET` | `/` | always | Provisioning UI from SD `/html` (gateway) or placeholder page (cola-epaper) |
| `PUT` | `/api/feedData` | gateway connected; cola-epaper connected | Accept a feed/weather payload |
| `GET` | `/api/wifi/list` | gateway provisioning mode only | Scan visible SSIDs |
| `GET` | `/api/rsa/public_key` | gateway provisioning mode only | PEM public key for password encryption |
| `POST` | `/api/wifi/connect` | gateway provisioning mode only | Submit `{ "ssid", "password" }`; password may be RSA-OAEP ciphertext or plain text fallback |

The gateway also answers `/generate_204`, `/hotspot-detect.html`, `/connecttest.txt`, `/ncsi.txt`, and `/fwlink` with a redirect to its AP address so phones and laptops open the captive portal automatically. Outside provisioning mode, the Wi-Fi endpoints return 403.

## Hardware

| Project | MCU / board | Display | Extra hardware |
| --- | --- | --- | --- |
| `cola-epaper` | ESP32-S3-DevKitC-1 | 4.2" three-color e-paper (`GxEPD2_420c`, 400×300) | — |
| `cola-epaper-i7` | ESP32-C5-DevKitC-1 (32 MB flash, 8 MB PSRAM) | 7.5" three-color e-paper (`GDEY075Z08`, 800×480) | SHTC3, ADXL343, MAX17048, battery |
| `cola-ble-gateway` | LilyGo T-Dongle-S3 (ESP32-S3, 16 MB flash) | 0.96" ST7735 TFT (80×160, used rotated) | microSD (SD_MMC), DotStar RGB LED, button |

## Operational Notes

- The bundled web UI is Chinese-only; dates are formatted as `YYYY-MM-DD HH:mm:ss` in the server's local time zone (`TZ`, default `Asia/Shanghai` in Docker).
- Weather is entirely optional. Without it, displays show `--`/unknown icons and everything else keeps working.
- Only the latest 6 records are pushed to devices; the server keeps 12.
- The BLE link stays up during brief Wi-Fi drops (the gateway retries in the background), but is disabled in provisioning mode. The e-paper keeps showing its last snapshot, with the clock advancing from the last known server time.
- The Wi-Fi AP password (`12345678`) and the provisioning RSA key pair are development defaults. Replace both before deploying outside a trusted network.
- `/api/status` and `/api/clients` now require a Bearer token; `/api/feed` stays anonymous. Default accounts are `admin`/`admin` and `mom`/`mom`; replace them before deployment. The server is still designed for a trusted LAN, so do not expose it directly to the internet.

## License

ISC
