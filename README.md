# N-Defender ESP32 Panel Firmware

ESP32 firmware that controls VRX modules, reads RSSI via ADC, drives LEDs, and renders a debug OLED screen. Communication with the Raspberry Pi is via newline-delimited JSON over USB serial.

## Capabilities

Current firmware capabilities:
- Robust serial framing and command ACK for every command
- VRX tuning with `SET_VRX_FREQ`
- RSSI sampling (ADC) per VRX
- Non-blocking scan engine (`START_SCAN` / `STOP_SCAN`)
- Lock strongest signal (`LOCK_STRONGEST`)
- Video switch control (`VIDEO_SELECT`)
- LED control (`SET_LEDS`)
- OLED debug display (mode, TX/RX age, VRX freq/RSSI, last command status)

See `docs/SERIAL_PROTOCOL.md` for full API details.

## Folder Structure

- `esp32_firmware/` Firmware package
  - `src/esp32.ino` Single-file Arduino sketch (complete firmware)
  - `README.md` Firmware-specific setup and verification guide
- `firmware.md` Firmware package guide and setup instructions
- `docs/SERIAL_PROTOCOL.md` Serial API reference
- `tools/python_serial_tester.py` UART auto-detection and test tool

## Required Libraries

These are declared in `esp32_firmware/platformio.ini`:

- `adafruit/Adafruit GFX Library@1.12.4`
- `adafruit/Adafruit BusIO@1.17.4`
- `adafruit/Adafruit SSD1327@1.0.4`

## Build Environment Requirements

- Python 3.9+ (for PlatformIO)
- PlatformIO CLI (`pip install platformio`)
- USB drivers for ESP32-S3
- A compatible ESP32-S3 board (default: `esp32-s3-devkitc-1`)

## Build

```bash
cd esp32_firmware
pio run
```

## Flash

```bash
cd esp32_firmware
pio run -t upload --upload-port <PORT>
```

## Configuration Options

Edit constants at the top of `esp32_firmware/src/esp32.ino` to adjust:

- `SERIAL_BAUD`, `TELEMETRY_INTERVAL_MS`
- LED pins (locked defaults)
- OLED I2C pins and clock
- VRX control pins and RSSI pins
- Video switch placeholders (board-specific)

## How To Verify It Is Running

1. Open a serial monitor at 115200 baud.
2. Expect a 1 Hz telemetry heartbeat.
3. OLED (if connected) shows boot banner and live status.

Example telemetry lines:

```json
{"type":"telemetry","timestamp_ms":1000,"sel":1,"led":{"r":0,"y":0,"g":1},"sys":{"uptime_ms":1000,"heap":123456}}
{"type":"telemetry","timestamp_ms":2000,"sel":1,"led":{"r":0,"y":0,"g":1},"sys":{"uptime_ms":2000,"heap":123456}}
{"type":"telemetry","timestamp_ms":3000,"sel":1,"led":{"r":0,"y":0,"g":1},"sys":{"uptime_ms":3000,"heap":123456}}
```
