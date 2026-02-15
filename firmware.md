# Firmware Package Guide

## Purpose

This firmware provides the ESP32 hardware control layer for the N-Defender system. It drives LEDs, controls VRX modules, reads RSSI via ADC, renders a debug OLED screen, and communicates with the Raspberry Pi using newline-delimited JSON over USB serial.

## Package Contents

- `esp32_firmware/src/main.cpp` PlatformIO entrypoint (primary build target).
- `esp32_firmware/include/config.h` Pin and firmware configuration.
- `esp32_firmware/esp32.ino` Arduino IDE friendly entrypoint (same logic as `main.cpp`).
- `docs/SERIAL_PROTOCOL.md` Production serial protocol reference.

## Required Libraries (Arduino)

Install these libraries if building with Arduino IDE:

- `Adafruit GFX Library` version 1.12.4
- `Adafruit BusIO` version 1.17.4
- `Adafruit SSD1327` version 1.0.4

## Build Environment Requirements (PlatformIO)

- Python 3.9+
- PlatformIO CLI
- ESP32-S3 USB drivers

## PlatformIO Build

```bash
cd esp32_firmware
pio run
```

## PlatformIO Flash

```bash
cd esp32_firmware
pio run -t upload --upload-port <PORT>
```

## Arduino IDE Build

1. Open `esp32_firmware/esp32.ino`.
2. Select board `ESP32S3 Dev Module`.
3. Set the serial speed to `115200`.
4. Install the libraries listed above.
5. Build and upload.

## Configuration Options

Edit `esp32_firmware/include/config.h`:

- `SERIAL_BAUD` and `TELEMETRY_INTERVAL_MS`
- LED pins (locked defaults)
- OLED I2C pins and clock
- VRX control pins and RSSI pins
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
