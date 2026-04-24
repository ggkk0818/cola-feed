# Cola Feed

[English](README.md) | [简体中文](README-CN.md)

An IoT feed display system featuring e-paper display and web-based data management.

## Overview

Cola Feed is a comprehensive IoT solution that combines hardware e-paper displays with a web-based feed management system. It enables real-time data display on e-paper screens with centralized feed management through a web interface.

## Features

- **E-paper Display**: ESP32-S3 based e-paper display with low power consumption
- **Web Management**: Built-in web interface for feed management
- **REST API**: Full-featured API for feed data operations
- **UDP Discovery**: Automatic device discovery and communication
- **Docker Support**: Easy deployment with Docker containers
- **Dark Theme**: Modern UI with dark mode support

## Project Structure

```
cola-feed/
├── cola-epaper/          # ESP32-S3 e-paper display firmware
│   ├── src/              # Main source code
│   │   ├── modules/      # Function modules (WiFi, Network, Drawing, etc.)
│   │   └── utils/        # Utility functions
│   └── platformio.ini    # PlatformIO configuration
└── feed-server/          # Node.js web server
    ├── src/              # Server source code
    │   ├── routes/       # API routes
    │   └── services/     # Business logic services
    ├── public/           # Web UI (HTML/CSS/JS)
    ├── data/             # Data storage
    └── docker/           # Docker configuration
```

## Sub-projects

### cola-epaper

ESP32-S3 firmware for e-paper display with the following modules:
- **WiFi Module**: Network connection management
- **Network Module**: UDP discovery and communication
- **Web Server Module**: Device configuration interface
- **Feed Controller**: Feed data processing and display logic
- **Drawing Module**: E-paper screen rendering

**Tech Stack**: PlatformIO, Arduino Framework, ESP32-S3

### feed-server

Web server providing feed management API and UI:
- **Feed Service**: Feed data management and storage
- **Client Service**: UDP client discovery and communication
- **REST API**: Full CRUD operations for feed data
- **Web UI**: Modern responsive interface

**Tech Stack**: Node.js, Express.js, Docker

## Quick Start

### Prerequisites

- For cola-epaper: PlatformIO IDE
- For feed-server: Node.js 18+ and Docker

### Installation

#### cola-epaper (ESP32-S3 Firmware)

```bash
cd cola-epaper
# Install PlatformIO dependencies
pio upgrade
pio install espressif32

# Build and upload
pio run --target upload
```

#### feed-server (Web Server)

```bash
cd feed-server
npm install

# Development
npm start

# Docker deployment
npm run docker:build
docker-compose up
```

## Usage

1. **Hardware Setup**: Flash cola-epaper firmware to ESP32-S3
2. **Network Configuration**: Connect to device's WiFi hotspot for configuration
3. **Server Deployment**: Deploy feed-server using Docker
4. **Device Discovery**: Devices automatically discover and connect to server
5. **Feed Management**: Access web UI at `http://localhost:3000`

## Configuration

### Environment Variables (feed-server)

- `PORT`: Server port (default: 3000)
- `UDP_PORT`: UDP discovery port (default: 6113)
- `DATA_DIR`: Data storage directory (default: ./data)

### Device Configuration (cola-epaper)

Access device's web interface for WiFi and server configuration.

## Hardware Requirements

- ESP32-S3 development board
- E-paper display (supported by GxEPD2 library)
- USB-C cable for power and programming

## License

ISC

