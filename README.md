# N-Defender ESP32 Panel Firmware

[![Version](https://img.shields.io/badge/version-v1.0.0--esp32--panel--green-blue)](https://github.com/flyspark015/ndefender-esp32-panel/releases)
[![Build](https://github.com/flyspark015/ndefender-esp32-panel/actions/workflows/ci.yml/badge.svg)](https://github.com/flyspark015/ndefender-esp32-panel/actions)
[![Production Ready](https://img.shields.io/badge/status-production%20ready-brightgreen)](https://github.com/flyspark015/ndefender-esp32-panel)

Production-ready ESP32 firmware for VRX control, RSSI sampling, LED status, OLED debug display, and video switch control.

## 🚀 Overview
This firmware turns the ESP32-S3 into a deterministic hardware controller. It exposes a strict newline-delimited JSON serial protocol to the Raspberry Pi backend, providing telemetry and command execution with robust acknowledgements.

## 🧠 System Architecture
- **ESP32 role**: Real-time hardware controller and telemetry producer.
- **Raspberry Pi backend**: Sends commands and consumes telemetry over USB serial.
- **Serial contract**: Newline-delimited JSON. Every command returns `command_ack`.
- **VRX control**: Frequency tuning via bit-banged SPI.
- **Video switch**: GPIO-based selection for analog video switch.

## 🔌 Hardware Mapping (Locked)

| Component | GPIO |
|-----------|------|
| LED Red   | 16   |
| LED Yellow| 15   |
| LED Green | 7    |
| OLED SDA  | 13   |
| OLED SCL  | 12   |
| VRX DATA  | 3    |
| VRX LE    | 4    |
| VRX CLK   | 5    |
| RSSI ADC  | 8    |

Full pin documentation: `docs/HARDWARE_PINS.md`

## 📡 Serial Protocol Summary
- **Transport**: USB serial, 115200 baud
- **Format**: JSON per line, newline-delimited
- **Commands**: `SET_VRX_FREQ`, `START_SCAN`, `STOP_SCAN`, `LOCK_STRONGEST`, `VIDEO_SELECT`, `SET_LEDS`, `GET_STATUS`
- **ACK**: Every command returns `command_ack`
- **Telemetry**: `telemetry` message with `vrx`, `video`, `led`, `sys`

Full reference: `docs/SERIAL_PROTOCOL.md`

## 🖥 OLED Debug Display
- Shows mode, TX/RX age, VRX freq/RSSI, video selection, and last command status.
- Non-blocking and optional; firmware continues without OLED.

Details: `docs/OLED_DEBUG.md`

## 🧪 Verification Checklist
- [x] Telemetry streaming
- [x] LED control
- [x] VRX tuning
- [x] Scan engine
- [x] Lock strongest
- [x] Video select
- [x] OLED debug working

## 🔄 Flash & Replacement Instructions
See `docs/FLASH_REPLACEMENT.md`.

## 🛡 Production Readiness
- Non-blocking design
- ACK enforced for every command
- Robust serial framing with overflow protection
- Error counters for RX framing issues
- Stable telemetry schema

## ⚠ Known Limitations
- Command parsing is key-based string scanning, not a full JSON object parser.
- `args` fields are accepted but not strictly required as a nested object.

## 🛣 Future Roadmap
- Add watchdog + recovery strategy
- Add RSSI smoothing and calibration profile
- Add structured logging (`log_event`)
- Add command rate limiting
- Add persistent config profiles

## 📁 Repository Structure
- `esp32_firmware/` Single-file Arduino sketch and firmware docs
- `docs/` Protocol, hardware, OLED, flash docs
- `tools/` UART detection and test tools
- `tests/` Protocol and UART detection tests

## ✅ Capabilities
- Robust serial framing + `command_ack`
- VRX tuning via `SET_VRX_FREQ`
- RSSI sampling (ADC)
- Non-blocking scan engine (`START_SCAN` / `STOP_SCAN`)
- Lock strongest (`LOCK_STRONGEST`)
- Video switch control (`VIDEO_SELECT`)
- LED control (`SET_LEDS`)
- OLED debug display (mode, VRX, link stats, last command)

