# Flash Replacement

This `esp32_firmware/` folder is the new final firmware for the N-Defender ESP32 panel. After flashing, the old firmware is replaced.

Verification after flashing:
- Confirm telemetry JSON lines are streaming over serial (1 Hz heartbeat).
- Confirm OLED shows boot banner and live status (if connected).
- Confirm LEDs show expected initial state (green on at boot).
