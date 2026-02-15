# ESP32 Firmware (Single-File Arduino Sketch)

## Purpose

This firmware runs on the ESP32-S3-DevKitC-1-N8R2 board to provide the N-Defender panel hardware controller. It drives LEDs, renders a debug OLED screen, and streams telemetry over USB serial in newline-delimited JSON. It also accepts control commands and always replies with `command_ack`.

## Board Selection and Arduino IDE Setup

1. Install the ESP32 board package in Arduino IDE:
   - Open `Tools > Board > Boards Manager`.
   - Search for `esp32` by Espressif Systems.
   - Install the latest stable version.
2. Select the board:
   - `Tools > Board > ESP32 Arduino > ESP32S3 Dev Module`
3. Set the serial port:
   - `Tools > Port > <your ESP32 port>`
4. Set serial speed:
   - `Tools > Serial Monitor > 115200`

## Required Libraries (Arduino)

Install via `Tools > Manage Libraries`:

- `Adafruit GFX Library` version 1.12.4
- `Adafruit BusIO` version 1.17.4
- `Adafruit SSD1327` version 1.0.4

## Build and Flash Steps

1. Open the sketch: `esp32_firmware/src/esp32.ino`
2. Click `Verify` to compile.
3. Click `Upload` to flash.

## Configuration (Pins and Options)

These constants are defined at the top of `esp32.ino`:

- `SERIAL_BAUD` (default 115200)
- `TELEMETRY_INTERVAL_MS` (default 1000)

Locked wiring defaults:

- LED Red: GPIO16
- LED Yellow: GPIO15
- LED Green: GPIO7

OLED I2C:

- SDA: GPIO13
- SCL: GPIO12
- Clock: 400kHz

VRX placeholders (used in later phases):

- DATA: GPIO3
- CLK: GPIO5
- LE: GPIO4
- RSSI: GPIO8

Video switch placeholders (board-specific):

- GPIO20 / GPIO21 / GPIO47

## How To Verify After Flashing

1. Open Serial Monitor at 115200 baud.
2. Expect a 1 Hz telemetry heartbeat.
3. If the OLED is connected, it shows a boot banner and live status.

Example telemetry lines:

```json
{"type":"telemetry","timestamp_ms":1000,"sel":1,"led":{"r":0,"y":0,"g":1},"sys":{"uptime_ms":1000,"heap":123456}}
{"type":"telemetry","timestamp_ms":2000,"sel":1,"led":{"r":0,"y":0,"g":1},"sys":{"uptime_ms":2000,"heap":123456}}
{"type":"telemetry","timestamp_ms":3000,"sel":1,"led":{"r":0,"y":0,"g":1},"sys":{"uptime_ms":3000,"heap":123456}}
```

## Suggestions for Production Hardening (Planned)

- Add watchdog and recovery logic.
- Rate-limit and validate command traffic.
- Add RSSI sampling + VRX control + scan/lock engine.
- Add video switch control with configurable GPIOs.
- Extend OLED debug screen with mode + last command.
