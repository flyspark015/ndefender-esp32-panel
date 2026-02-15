# Firmware Package Guide

## Purpose

This firmware provides the ESP32 hardware control layer for the N-Defender system. It drives LEDs, renders a debug OLED screen, and communicates with the Raspberry Pi using newline-delimited JSON over USB serial.

## Single-File Sketch

All firmware logic lives in a single Arduino sketch file:

- `esp32_firmware/src/esp32.ino`

No additional `.h` or `.cpp` files are required.

## Required Libraries (Arduino)

Install these libraries via `Tools > Manage Libraries`:

- `Adafruit GFX Library` version 1.12.4
- `Adafruit BusIO` version 1.17.4
- `Adafruit SSD1327` version 1.0.4

## Arduino IDE Setup

1. Install the ESP32 board package in Arduino IDE:
   - `Tools > Board > Boards Manager`.
   - Search for `esp32` by Espressif Systems.
   - Install the latest stable version.
2. Select the board:
   - `Tools > Board > ESP32 Arduino > ESP32S3 Dev Module`
3. Set the serial port:
   - `Tools > Port > <your ESP32 port>`
4. Set serial speed:
   - `Tools > Serial Monitor > 115200`

## Build and Flash

1. Open `esp32_firmware/src/esp32.ino`.
2. Click `Verify` to compile.
3. Click `Upload` to flash.

## Configuration Options

Edit constants at the top of `esp32.ino`:

- `SERIAL_BAUD` and `TELEMETRY_INTERVAL_MS`
- LED pins (locked defaults)
- OLED I2C pins and clock
- VRX pin placeholders
- Video switch placeholders

## Verification Checklist

- On boot, OLED shows banner and status (if connected).
- Serial output shows 1 Hz telemetry lines.
- `SET_LEDS` command returns `command_ack`.

Example telemetry lines:

```json
{"type":"telemetry","timestamp_ms":1000,"sel":1,"led":{"r":0,"y":0,"g":1},"sys":{"uptime_ms":1000,"heap":123456}}
{"type":"telemetry","timestamp_ms":2000,"sel":1,"led":{"r":0,"y":0,"g":1},"sys":{"uptime_ms":2000,"heap":123456}}
{"type":"telemetry","timestamp_ms":3000,"sel":1,"led":{"r":0,"y":0,"g":1},"sys":{"uptime_ms":3000,"heap":123456}}
```
