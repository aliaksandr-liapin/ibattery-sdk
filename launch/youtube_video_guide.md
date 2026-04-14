# YouTube Demo Video — Recording Guide

## Overview
- **Title**: "Battery Intelligence SDK for IoT — 3 Platforms, BLE to Grafana Dashboard"
- **Length**: 2-3 minutes
- **Goal**: Show the complete pipeline working on real hardware
- **No talking required** — use text overlays and let the terminal/dashboard speak for themselves. Or do a simple voiceover if you're comfortable.

## Tools Needed
- **Screen recording**: QuickTime Player (built-in macOS)
  - File → New Screen Recording → select area
  - Or use OBS Studio (free) for more control
- **Phone camera**: For the hardware shots (boards, wiring, breadboard)
- **Video editing**: iMovie (free, built-in) to combine clips

## Recording Plan — 6 Clips

### Clip 1: Hardware Overview (15 sec, phone camera)
- Show all 3 boards on your desk: nRF52840-DK, NUCLEO-L476RG, ESP32-C3 DevKitM
- Show the ESP32-C3 with voltage divider + LiPo connected on breadboard
- Text overlay: "3 platforms, 1 SDK, same codebase"

### Clip 2: Build + Flash (20 sec, screen recording)
- Terminal: run the build command for ESP32-C3
- Show the "Successfully created esp32c3 image" output
- Flash command + "Hard resetting via RTS pin"
- Text overlay: "Build in 30 seconds, flash in 5"

### Clip 3: Serial Telemetry (15 sec, screen recording)
- Terminal: show boot banner scrolling:
  ```
  === iBattery SDK — On-Target Validation ===
  Platform: ESP32-C3 (DevKitM)
  Chemistry: LiPo (3.7V)
  Transport: BLE enabled
  ```
- Show telemetry packets flowing:
  ```
  [v2 t=2040] V=4119 mV T=38.50 C SOC=91.33% PWR=1
  ```
- Text overlay: "Real battery voltage via voltage divider"

### Clip 4: BLE Gateway (20 sec, screen recording)
- Terminal: `ibattery-gateway scan` → shows "iBattery-ESP32C3" found
- Terminal: `ibattery-gateway stream` → live packets flowing
- Text overlay: "BLE telemetry to Python gateway"

### Clip 5: Grafana Dashboard (30 sec, screen recording)
- Browser: Grafana dashboard with all panels live
  - Voltage graph trending
  - SoC gauge at 90%+
  - Temperature
  - Power state showing ACTIVE → IDLE → SLEEP transition
- Zoom into individual panels
- Text overlay: "Real-time dashboard via InfluxDB + Grafana"

### Clip 6: GitHub + Docs (15 sec, screen recording)
- Browser: GitHub repo page showing CI badge (green), releases, README
- Browser: docs site at aliaksandr-liapin.github.io/ibattery-sdk/
- Text overlay: "Open source — Apache 2.0"
- End card: GitHub URL

## Total: ~2 minutes

## Step-by-Step Recording Process

### 1. Prepare (5 min)
- Make sure ESP32-C3 is connected with voltage divider + LiPo
- Start Docker: `cd cloud && docker compose up -d`
- Open Grafana in browser: http://localhost:3000
- Open 2 terminal windows
- Have the GitHub repo open in another browser tab

### 2. Record hardware clip (phone, 1 min)
- Lay all 3 boards on a clean surface
- Point camera at each one briefly
- Close-up of ESP32-C3 + breadboard + LiPo

### 3. Record screen clips (QuickTime, 5 min)
- **QuickTime**: File → New Screen Recording → drag to select area
- Record each terminal/browser clip separately — easier to edit
- Clip 2: Build + flash (run the commands live)
- Clip 3: Serial output (just cat the serial port)
- Clip 4: Gateway scan + stream (run in terminal)
- Clip 5: Grafana dashboard (just browse the panels)
- Clip 6: GitHub + docs site

### 4. Edit in iMovie (15 min)
- Import all clips
- Trim dead time (waiting for builds, etc.)
- Add text overlays for each section
- Add background music (optional — iMovie has free tracks)
- Export as 1080p

### 5. Upload to YouTube
- Title: "Battery Intelligence SDK for IoT — 3 Platforms, BLE to Grafana Dashboard"
- Description:
  ```
  Open-source embedded C SDK providing battery monitoring for IoT devices.
  Runs on nRF52840, STM32L476, and ESP32-C3 (Zephyr RTOS).

  Full pipeline: voltage measurement → SoC estimation → BLE telemetry → 
  Python gateway → InfluxDB → Grafana dashboard.

  GitHub: https://github.com/aliaksandr-liapin/ibattery-sdk
  Docs: https://aliaksandr-liapin.github.io/ibattery-sdk/
  Article: https://dev.to/aliaksandrliapin/how-i-built-a-portable-battery-sdk-that-runs-on-3-mcu-platforms-28gp
  ```
- Tags: embedded, iot, esp32, nrf52840, stm32, zephyr, bluetooth, battery, grafana, open source
- Thumbnail: Grafana dashboard screenshot with "3 MCU Platforms" text overlay

## Tips
- Keep clips SHORT — cut anything that's just waiting
- Terminal font size: bump it up so it's readable at 720p
- Terminal: increase font to 16-18pt before recording
- Close unnecessary browser tabs and notifications
- Dark terminal theme looks best on YouTube
