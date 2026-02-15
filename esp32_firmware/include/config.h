#pragma once

#include <Arduino.h>

// Firmware identity
#define FW_VERSION "0.1.0"
#define FW_BUILD_TIME __DATE__ " " __TIME__

// Serial
constexpr uint32_t SERIAL_BAUD = 115200;
constexpr uint32_t TELEMETRY_INTERVAL_MS = 1000;
constexpr size_t SERIAL_MAX_LINE = 512;
constexpr size_t SERIAL_MAX_ID = 64;
constexpr size_t SERIAL_MAX_CMD = 64;

// LEDs (locked wiring)
constexpr uint8_t PIN_LED_RED = 16;
constexpr uint8_t PIN_LED_YELLOW = 15;
constexpr uint8_t PIN_LED_GREEN = 7;

// OLED (SSD1327 128x96 I2C)
constexpr uint8_t PIN_OLED_SDA = 13;
constexpr uint8_t PIN_OLED_SCL = 12;
constexpr uint32_t OLED_I2C_HZ = 400000;

// VRX SPI control (locked wiring)
constexpr uint8_t PIN_VRX_DATA = 3;
constexpr uint8_t PIN_VRX_CLK = 5;
constexpr uint8_t PIN_VRX1_LE = 4;
constexpr uint8_t PIN_VRX1_RSSI = 8;

// Placeholders for multi-VRX and video switch (configured later)
constexpr uint8_t PIN_VRX2_LE = 10;
constexpr uint8_t PIN_VRX2_RSSI = 14;
constexpr uint8_t PIN_VRX3_LE = 11;
constexpr uint8_t PIN_VRX3_RSSI = 9;

// Video switch pins (board-specific; configure as needed)
constexpr uint8_t PIN_VIDEO_SEL_1 = 20;
constexpr uint8_t PIN_VIDEO_SEL_2 = 21;
constexpr uint8_t PIN_VIDEO_SEL_3 = 47;
