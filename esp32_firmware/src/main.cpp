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
static uint32_t lastRxMs = 0;

static uint32_t rxErrors = 0;
static uint32_t rxOverflow = 0;

static bool led_r = false;
static bool led_y = false;
static bool led_g = false;

static char rxLine[SERIAL_MAX_LINE];
static size_t rxLen = 0;
static bool rxDrop = false;

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
  const uint32_t rx_age = (lastRxMs == 0) ? 0 : (now - lastRxMs);

  display.setCursor(0, 0);
  display.println("Status: HEARTBEAT");
  display.print("Uptime: ");
  display.print(now);
  display.println(" ms");
  display.print("Last TX: ");
  display.print(tx_age);
  display.println(" ms");
  display.print("Last RX: ");
  display.print(rx_age);
  display.println(" ms");

  display.print("LED RYG: ");
  display.print(led_r ? 1 : 0);
  display.print(led_y ? 1 : 0);
  display.println(led_g ? 1 : 0);

  display.print("RX Err/Ov: ");
  display.print(rxErrors);
  display.print("/");
  display.println(rxOverflow);

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

static void printJsonEscaped(const char *s) {
  while (*s) {
    const char c = *s++;
    if (c == '\"' || c == '\\') {
      Serial.print('\\');
    }
    Serial.print(c);
  }
}

static void sendCommandAck(const char *id, const char *cmd, bool ok, const char *err) {
  const uint32_t ms = millis();
  Serial.print("{\"type\":\"command_ack\",\"timestamp_ms\":");
  Serial.print(ms);
  Serial.print(",\"id\":\"");
  printJsonEscaped(id ? id : "unknown");
  Serial.print("\",\"ok\":");
  Serial.print(ok ? "true" : "false");
  Serial.print(",\"err\":");
  if (err && err[0] != '\0') {
    Serial.print("\"");
    printJsonEscaped(err);
    Serial.print("\"");
  } else {
    Serial.print("null");
  }
  if (cmd && cmd[0] != '\0') {
    Serial.print(",\"data\":{\"cmd\":\"");
    printJsonEscaped(cmd);
    Serial.print("\"}");
  }
  Serial.print("}");
  Serial.println();
  lastTxMs = ms;
}

static bool jsonFindStringField(const char *line, const char *key, char *out, size_t out_len) {
  if (!line || !key || !out || out_len == 0) {
    return false;
  }

  char pattern[32];
  const size_t key_len = strlen(key);
  if (key_len + 3 >= sizeof(pattern)) {
    return false;
  }
  pattern[0] = '\"';
  memcpy(pattern + 1, key, key_len);
  pattern[key_len + 1] = '\"';
  pattern[key_len + 2] = '\0';

  const char *p = strstr(line, pattern);
  if (!p) {
    return false;
  }
  p += strlen(pattern);
  while (*p && isspace(static_cast<unsigned char>(*p))) {
    ++p;
  }
  if (*p != ':') {
    return false;
  }
  ++p;
  while (*p && isspace(static_cast<unsigned char>(*p))) {
    ++p;
  }
  if (*p != '\"') {
    return false;
  }
  ++p;

  size_t w = 0;
  while (*p && *p != '\"') {
    if (*p == '\\' && p[1] != '\0') {
      ++p;
    }
    if (w + 1 < out_len) {
      out[w++] = *p;
    }
    ++p;
  }
  if (*p != '\"') {
    return false;
  }
  out[w] = '\0';
  return true;
}

static void dispatchCommand(const char *id, const char *cmd, const char *line) {
  (void)line;
  if (strcmp(cmd, "GET_STATUS") == 0) {
    sendCommandAck(id, cmd, true, "");
    return;
  }
  if (strcmp(cmd, "SET_VRX_FREQ") == 0 ||
      strcmp(cmd, "START_SCAN") == 0 ||
      strcmp(cmd, "STOP_SCAN") == 0 ||
      strcmp(cmd, "LOCK_STRONGEST") == 0 ||
      strcmp(cmd, "VIDEO_SELECT") == 0 ||
      strcmp(cmd, "SET_LEDS") == 0) {
    sendCommandAck(id, cmd, false, "not_implemented");
    return;
  }

  sendCommandAck(id, cmd, false, "unknown_cmd");
}

static void handleRxLine(const char *line) {
  if (!line) {
    return;
  }
  const char *p = line;
  while (*p && isspace(static_cast<unsigned char>(*p))) {
    ++p;
  }
  if (*p != '{') {
    rxErrors++;
    return;
  }

  char id[SERIAL_MAX_ID] = {0};
  char cmd[SERIAL_MAX_CMD] = {0};

  const bool has_id = jsonFindStringField(p, "id", id, sizeof(id));
  const bool has_cmd = jsonFindStringField(p, "cmd", cmd, sizeof(cmd));

  if (!has_cmd) {
    sendCommandAck(has_id ? id : "unknown", "", false, "missing_cmd");
    return;
  }
  if (!has_id) {
    sendCommandAck("unknown", cmd, false, "missing_id");
    return;
  }

  dispatchCommand(id, cmd, p);
}

static void serialPoll() {
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\n') {
      if (!rxDrop) {
        rxLine[rxLen] = '\0';
        if (rxLen > 0) {
          lastRxMs = millis();
          handleRxLine(rxLine);
        }
      } else {
        rxOverflow++;
      }
      rxLen = 0;
      rxDrop = false;
      continue;
    }
    if (c == '\r') {
      continue;
    }
    if (rxDrop) {
      continue;
    }
    if (rxLen + 1 < SERIAL_MAX_LINE) {
      rxLine[rxLen++] = c;
    } else {
      rxDrop = true;
    }
  }
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

  serialPoll();

  if (oled_ok && (now - lastOledMs) >= 1000) {
    lastOledMs = now;
    drawStatus();
  }
}
