# 🔌 Hardware Pins (Locked)

These pins are fixed in the current production build.

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

## Notes
- VRX RSSI pins for other channels are configured in `esp32.ino`.
- Video switch GPIOs are configurable and may vary by board.
