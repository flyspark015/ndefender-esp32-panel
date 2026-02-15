# 🖥 OLED Debug Display

The OLED is used as a lightweight, non-blocking debug screen.
If the OLED is missing or fails to initialize, the system continues running normally.

## Display Content
- Mode (IDLE / SCAN / LOCK / MAN)
- Video selection
- TX/RX age
- RX error counters
- VRX frequencies and RSSI
- Last command + OK/ER status

## Notes
- Update interval is non-blocking.
- OLED shows the system is alive and responding to commands.
