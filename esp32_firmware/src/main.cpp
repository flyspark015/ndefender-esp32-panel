#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1327.h>

#include "config.h"

static Adafruit_SSD1327 display(128, 96, &Wire, -1);
static bool oled_ok = false;

static uint32_t lastTelemetryMs = 0;
static uint32_t lastOledMs = 0;
static uint32_t lastTxMs = 0;

static bool led_r = false;
static bool led_y = false;
static bool led_g = false;

static void setLEDs(bool r, bool y, bool g) {
  led_r = r;
  led_y = y;
  led_g = g;
  digitalWrite(PIN_LED_RED, r ? HIGH : LOW);
  digitalWrite(PIN_LED_YELLOW, y ? HIGH : LOW);
  digitalWrite(PIN_LED_GREEN, g ? HIGH : LOW);
}

static void drawBootBanner() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1327_WHITE);

  display.setCursor(0, 0);
  display.println("N-Defender ESP32");
  display.println("Panel Firmware");
  display.println("");
  display.print("FW: ");
  display.println(FW_VERSION);
  display.print("Build: ");
  display.println(FW_BUILD_TIME);
  display.display();
}

static void drawStatus() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1327_WHITE);

  const uint32_t now = millis();
  const uint32_t tx_age = (lastTxMs == 0) ? 0 : (now - lastTxMs);

  display.setCursor(0, 0);
  display.println("Status: HEARTBEAT");
  display.print("Uptime: ");
  display.print(now);
  display.println(" ms");
  display.print("Last TX: ");
  display.print(tx_age);
  display.println(" ms");

  display.print("LED RYG: ");
  display.print(led_r ? 1 : 0);
  display.print(led_y ? 1 : 0);
  display.println(led_g ? 1 : 0);

  display.display();
}

static void sendTelemetry() {
  const uint32_t ms = millis();
  const uint32_t heap = ESP.getFreeHeap();

  Serial.print("{\"type\":\"telemetry\",");
  Serial.print("\"timestamp_ms\":");
  Serial.print(ms);
  Serial.print(",\"sel\":1");
  Serial.print(",\"led\":{\"r\":");
  Serial.print(led_r ? 1 : 0);
  Serial.print(",\"y\":");
  Serial.print(led_y ? 1 : 0);
  Serial.print(",\"g\":");
  Serial.print(led_g ? 1 : 0);
  Serial.print("}");
  Serial.print(",\"sys\":{\"uptime_ms\":");
  Serial.print(ms);
  Serial.print(",\"heap\":");
  Serial.print(heap);
  Serial.print("}}");
  Serial.println();

  lastTxMs = ms;
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(50);

  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_YELLOW, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);

  setLEDs(false, false, true);

  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  Wire.setClock(OLED_I2C_HZ);
  oled_ok = display.begin(0x3C);
  if (!oled_ok) {
    oled_ok = display.begin(0x3D);
  }
  if (oled_ok) {
    drawBootBanner();
  }

  lastTelemetryMs = millis();
  sendTelemetry();
}

void loop() {
  const uint32_t now = millis();

  if ((now - lastTelemetryMs) >= TELEMETRY_INTERVAL_MS) {
    lastTelemetryMs = now;
    sendTelemetry();
  }

  if (oled_ok && (now - lastOledMs) >= 1000) {
    lastOledMs = now;
    drawStatus();
  }
}
