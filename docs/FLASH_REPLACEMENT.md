# 🔄 Flash Replacement

This `esp32_firmware/` folder is the final production firmware for the N-Defender ESP32 panel.
Flashing it replaces any previous firmware on the device.

## Flash Steps
1. Open `esp32_firmware/src/esp32.ino` in Arduino IDE.
2. Select board: `ESP32S3 Dev Module`.
3. Select the correct serial port.
4. Click **Upload**.

## Verification After Flash
- Telemetry JSON lines appear at 1 Hz over serial.
- OLED shows boot banner + live status (if connected).
- LEDs respond to `SET_LEDS` command.

