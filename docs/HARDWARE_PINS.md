# Hardware Pins

Locked wiring defaults (ESP32-S3):

LEDs:
- Red: GPIO16
- Yellow: GPIO15
- Green: GPIO7

OLED (SSD1327 I2C):
- SDA: GPIO13
- SCL: GPIO12

VRX (FS58R3MW / MM238RW SPI):
- DATA: GPIO3
- LE: GPIO4
- CLK: GPIO5
- RSSI: GPIO8 (ADC)

Video switch:
- Configurable in `esp32_firmware/include/config.h` (board-specific placeholders)
