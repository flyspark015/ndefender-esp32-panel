# N-Defender ESP32 Panel Firmware

ESP32 firmware for VRX control, RSSI sampling, LED status, OLED debug display, and analog video switch control. The ESP32 communicates with the Raspberry Pi via newline-delimited JSON over USB serial.

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

## Repo Layout

- `esp32_firmware/` PlatformIO firmware project
- `docs/` Protocol + hardware docs
- `tools/` Host tools
- `tests/` Protocol parser tests
