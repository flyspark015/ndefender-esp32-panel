#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1327.h>

/*
  N-Defender ESP32 Panel Firmware (Single-File Arduino Sketch)
  Target: ESP32-S3-DevKitC-1-N8R2

  Features (current phase):
  - 1 Hz telemetry heartbeat
  - Robust serial framing + command ACK
  - SET_LEDS command
  - OLED debug banner + status

  Notes:
  - No buzzer, no buttons/joystick, no microSD logging.
  - VRX control, scan/lock, and video switching will be added in later phases.
*/

/* ===================== CONFIG ===================== */
#define FW_VERSION "0.1.0"
#define FW_BUILD_TIME __DATE__ " " __TIME__

static const uint32_t SERIAL_BAUD = 115200;
static const uint32_t TELEMETRY_INTERVAL_MS = 1000;
static const uint32_t RSSI_INTERVAL_MS = 120;
static const uint32_t OLED_I2C_HZ = 400000;

static const size_t SERIAL_MAX_LINE = 512;
static const size_t SERIAL_MAX_ID = 64;
static const size_t SERIAL_MAX_CMD = 64;

// LED wiring (locked)
static const uint8_t PIN_LED_RED = 16;
static const uint8_t PIN_LED_YELLOW = 15;
static const uint8_t PIN_LED_GREEN = 7;

// OLED (SSD1327 128x96 I2C)
static const uint8_t PIN_OLED_SDA = 13;
static const uint8_t PIN_OLED_SCL = 12;

// VRX wiring placeholders (for later phases)
static const uint8_t PIN_VRX_DATA = 3;
static const uint8_t PIN_VRX_CLK = 5;
static const uint8_t PIN_VRX1_LE = 4;
static const uint8_t PIN_VRX1_RSSI = 8;
static const uint8_t PIN_VRX2_LE = 10;
static const uint8_t PIN_VRX2_RSSI = 14;
static const uint8_t PIN_VRX3_LE = 11;
static const uint8_t PIN_VRX3_RSSI = 9;

// Video switch placeholders (board-specific)
static const uint8_t PIN_VIDEO_SEL_1 = 20;
static const uint8_t PIN_VIDEO_SEL_2 = 21;
static const uint8_t PIN_VIDEO_SEL_3 = 47;

/* ===================== GLOBALS ===================== */
static Adafruit_SSD1327 display(128, 96, &Wire, -1);
static bool oled_ok = false;

static uint32_t lastTelemetryMs = 0;
static uint32_t lastOledMs = 0;
static uint32_t lastTxMs = 0;
static uint32_t lastRxMs = 0;
static uint32_t lastRssiMs = 0;

static uint32_t rxErrors = 0;
static uint32_t rxOverflow = 0;

static bool led_r = false;
static bool led_y = false;
static bool led_g = false;

static char rxLine[SERIAL_MAX_LINE];
static size_t rxLen = 0;
static bool rxDrop = false;

struct VrxState {
  uint8_t id;
  uint8_t lePin;
  uint8_t rssiPin;
  uint64_t freq_hz;
  uint16_t rssi_raw;
  uint16_t best_rssi;
  uint64_t best_freq_hz;
};

static VrxState vrx[3] = {
  {1, PIN_VRX1_LE, PIN_VRX1_RSSI, 5740000000ULL, 0, 0, 0},
  {2, PIN_VRX2_LE, PIN_VRX2_RSSI, 5800000000ULL, 0, 0, 0},
  {3, PIN_VRX3_LE, PIN_VRX3_RSSI, 5860000000ULL, 0, 0, 0},
};

struct ScanState {
  bool active;
  uint32_t dwell_ms;
  uint64_t start_hz;
  uint64_t stop_hz;
  uint64_t step_hz;
  uint64_t current_hz;
  uint32_t last_step_ms;
};

static ScanState scan = {false, 200, 5645000000ULL, 5865000000ULL, 2000000ULL, 5645000000ULL, 0};

static inline void clkPulse() {
  digitalWrite(PIN_VRX_CLK, HIGH);
  delayMicroseconds(1);
  digitalWrite(PIN_VRX_CLK, LOW);
  delayMicroseconds(1);
}

static inline void sendBit(uint8_t b) {
  digitalWrite(PIN_VRX_DATA, b ? HIGH : LOW);
  clkPulse();
}

static void vrxWriteReg(uint8_t le, uint8_t addr4, uint32_t data20) {
  addr4 &= 0x0F;
  data20 &= 0xFFFFF;
  const uint32_t packet = (data20 << 5) | (1UL << 4) | addr4;

  digitalWrite(le, LOW);
  for (int i = 0; i < 25; i++) {
    sendBit((packet >> i) & 1U);
  }
  digitalWrite(le, HIGH);
  delayMicroseconds(2);
  digitalWrite(le, LOW);
  delayMicroseconds(80);
}

static uint32_t synthWordFromMhz(uint32_t rf_mhz) {
  int lo = static_cast<int>(rf_mhz) - 479;
  if (lo < 0) {
    lo = 0;
  }
  const int div = lo / 2;
  const int N = div / 32;
  const int A = div % 32;
  return (static_cast<uint32_t>(N) << 7) | static_cast<uint32_t>(A);
}

static void vrxInit(uint8_t le) {
  vrxWriteReg(le, 0x00, 0x00008);
}

static void vrxTuneHz(uint8_t le, uint64_t freq_hz) {
  const uint32_t mhz = static_cast<uint32_t>(freq_hz / 1000000ULL);
  vrxWriteReg(le, 0x01, synthWordFromMhz(mhz));
}

static uint16_t readRSSI(uint8_t pin) {
  uint32_t sum = 0;
  for (int i = 0; i < 6; i++) {
    sum += analogRead(pin);
    delayMicroseconds(200);
  }
  return static_cast<uint16_t>(sum / 6);
}

static void resetScanBest() {
  for (int i = 0; i < 3; i++) {
    vrx[i].best_rssi = 0;
    vrx[i].best_freq_hz = vrx[i].freq_hz;
  }
}

static void startScan(uint32_t dwell_ms, uint64_t step_hz, uint64_t start_hz, uint64_t stop_hz) {
  scan.active = true;
  scan.dwell_ms = dwell_ms;
  scan.step_hz = step_hz;
  scan.start_hz = start_hz;
  scan.stop_hz = stop_hz;
  scan.current_hz = start_hz;
  scan.last_step_ms = millis();
  resetScanBest();
  for (int i = 0; i < 3; i++) {
    vrx[i].freq_hz = scan.current_hz;
    vrxTuneHz(vrx[i].lePin, vrx[i].freq_hz);
  }
}

static void stopScan() {
  scan.active = false;
}

static void scanTick() {
  if (!scan.active) {
    return;
  }
  const uint32_t now = millis();
  if (now - scan.last_step_ms < scan.dwell_ms) {
    return;
  }

  for (int i = 0; i < 3; i++) {
    if (vrx[i].rssi_raw >= vrx[i].best_rssi) {
      vrx[i].best_rssi = vrx[i].rssi_raw;
      vrx[i].best_freq_hz = vrx[i].freq_hz;
    }
  }

  uint64_t next_hz = scan.current_hz + scan.step_hz;
  if (next_hz > scan.stop_hz) {
    next_hz = scan.start_hz;
    resetScanBest();
  }
  scan.current_hz = next_hz;
  scan.last_step_ms = now;

  for (int i = 0; i < 3; i++) {
    vrx[i].freq_hz = scan.current_hz;
    vrxTuneHz(vrx[i].lePin, vrx[i].freq_hz);
  }
}

/* ===================== HELPERS ===================== */
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
  Serial.print(",\"vrx\":[");
  for (int i = 0; i < 3; i++) {
    Serial.print("{\"id\":");
    Serial.print(vrx[i].id);
    Serial.print(",\"freq_hz\":");
    Serial.print(static_cast<unsigned long long>(vrx[i].freq_hz));
    Serial.print(",\"rssi_raw\":");
    Serial.print(vrx[i].rssi_raw);
    Serial.print("}");
    if (i < 2) {
      Serial.print(",");
    }
  }
  Serial.print("]");
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
    if (c == '"' || c == '\\') {
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
  pattern[0] = '"';
  memcpy(pattern + 1, key, key_len);
  pattern[key_len + 1] = '"';
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
  if (*p != '"') {
    return false;
  }
  ++p;

  size_t w = 0;
  while (*p && *p != '"') {
    if (*p == '\\' && p[1] != '\0') {
      ++p;
    }
    if (w + 1 < out_len) {
      out[w++] = *p;
    }
    ++p;
  }
  if (*p != '"') {
    return false;
  }
  out[w] = '\0';
  return true;
}

static bool jsonFindIntField(const char *line, const char *key, int *out) {
  if (!line || !key || !out) {
    return false;
  }

  char pattern[32];
  const size_t key_len = strlen(key);
  if (key_len + 3 >= sizeof(pattern)) {
    return false;
  }
  pattern[0] = '"';
  memcpy(pattern + 1, key, key_len);
  pattern[key_len + 1] = '"';
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
  bool neg = false;
  if (*p == '-') {
    neg = true;
    ++p;
  }
  if (!isdigit(static_cast<unsigned char>(*p))) {
    return false;
  }
  int value = 0;
  while (isdigit(static_cast<unsigned char>(*p))) {
    value = value * 10 + (*p - '0');
    ++p;
  }
  *out = neg ? -value : value;
  return true;
}

static bool jsonFindInt64Field(const char *line, const char *key, int64_t *out) {
  if (!line || !key || !out) {
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
  bool neg = false;
  if (*p == '-') {
    neg = true;
    ++p;
  }
  if (!isdigit(static_cast<unsigned char>(*p))) {
    return false;
  }
  int64_t value = 0;
  while (isdigit(static_cast<unsigned char>(*p))) {
    value = value * 10 + (*p - '0');
    ++p;
  }
  *out = neg ? -value : value;
  return true;
}

static void dispatchCommand(const char *id, const char *cmd, const char *line) {
  (void)line;
  if (strcmp(cmd, "GET_STATUS") == 0) {
    sendCommandAck(id, cmd, true, "");
    return;
  }
  if (strcmp(cmd, "SET_LEDS") == 0) {
    int r = 0;
    int y = 0;
    int g = 0;
    const bool has_r = jsonFindIntField(line, "r", &r);
    const bool has_y = jsonFindIntField(line, "y", &y);
    const bool has_g = jsonFindIntField(line, "g", &g);
    if (!has_r || !has_y || !has_g) {
      sendCommandAck(id, cmd, false, "missing_args");
      return;
    }
    if ((r != 0 && r != 1) || (y != 0 && y != 1) || (g != 0 && g != 1)) {
      sendCommandAck(id, cmd, false, "args_oob");
      return;
    }
    setLEDs(r == 1, y == 1, g == 1);
    sendCommandAck(id, cmd, true, "");
    return;
  }
  if (strcmp(cmd, "SET_VRX_FREQ") == 0) {
    int vrx_id = 0;
    int64_t freq_hz = 0;
    const bool has_id = jsonFindIntField(line, "vrx_id", &vrx_id);
    const bool has_freq = jsonFindInt64Field(line, "freq_hz", &freq_hz);
    if (!has_id || !has_freq) {
      sendCommandAck(id, cmd, false, "missing_args");
      return;
    }
    if (vrx_id < 1 || vrx_id > 3) {
      sendCommandAck(id, cmd, false, "vrx_id_oob");
      return;
    }
    if (freq_hz < 1000000LL || freq_hz > 6500000000LL) {
      sendCommandAck(id, cmd, false, "freq_oob");
      return;
    }
    const int idx = vrx_id - 1;
    vrx[idx].freq_hz = static_cast<uint64_t>(freq_hz);
    vrxTuneHz(vrx[idx].lePin, vrx[idx].freq_hz);
    sendCommandAck(id, cmd, true, "");
    return;
  }
  if (strcmp(cmd, "START_SCAN") == 0) {
    int dwell_ms = 0;
    int64_t step_hz = 0;
    int64_t start_hz = 0;
    int64_t stop_hz = 0;
    const bool has_dwell = jsonFindIntField(line, "dwell_ms", &dwell_ms);
    const bool has_step = jsonFindInt64Field(line, "step_hz", &step_hz);
    const bool has_start = jsonFindInt64Field(line, "start_hz", &start_hz);
    const bool has_stop = jsonFindInt64Field(line, "stop_hz", &stop_hz);
    if (!has_dwell || !has_step || !has_start || !has_stop) {
      sendCommandAck(id, cmd, false, "missing_args");
      return;
    }
    if (dwell_ms < 10 || dwell_ms > 5000) {
      sendCommandAck(id, cmd, false, "dwell_oob");
      return;
    }
    if (step_hz <= 0 || start_hz <= 0 || stop_hz <= 0 || start_hz >= stop_hz) {
      sendCommandAck(id, cmd, false, "range_oob");
      return;
    }
    startScan(static_cast<uint32_t>(dwell_ms),
              static_cast<uint64_t>(step_hz),
              static_cast<uint64_t>(start_hz),
              static_cast<uint64_t>(stop_hz));
    sendCommandAck(id, cmd, true, "");
    return;
  }
  if (strcmp(cmd, "STOP_SCAN") == 0) {
    stopScan();
    sendCommandAck(id, cmd, true, "");
    return;
  }
  if (strcmp(cmd, "LOCK_STRONGEST") == 0) {
    int vrx_id = 0;
    const bool has_id = jsonFindIntField(line, "vrx_id", &vrx_id);
    bool any_locked = false;
    if (has_id) {
      if (vrx_id < 1 || vrx_id > 3) {
        sendCommandAck(id, cmd, false, "vrx_id_oob");
        return;
      }
      const int idx = vrx_id - 1;
      if (vrx[idx].best_rssi == 0) {
        sendCommandAck(id, cmd, false, "no_best");
        return;
      }
      vrx[idx].freq_hz = vrx[idx].best_freq_hz;
      vrxTuneHz(vrx[idx].lePin, vrx[idx].freq_hz);
      any_locked = true;
    } else {
      for (int i = 0; i < 3; i++) {
        if (vrx[i].best_rssi == 0) {
          continue;
        }
        vrx[i].freq_hz = vrx[i].best_freq_hz;
        vrxTuneHz(vrx[i].lePin, vrx[i].freq_hz);
        any_locked = true;
      }
    }
    stopScan();
    if (!any_locked) {
      sendCommandAck(id, cmd, false, "no_best");
      return;
    }
    sendCommandAck(id, cmd, true, "");
    return;
  }
  if (strcmp(cmd, "VIDEO_SELECT") == 0) {
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

/* ===================== Arduino Entry ===================== */
void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(50);

  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_YELLOW, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);

  setLEDs(false, false, true);

  pinMode(PIN_VRX_DATA, OUTPUT);
  pinMode(PIN_VRX_CLK, OUTPUT);
  pinMode(PIN_VRX1_LE, OUTPUT);
  pinMode(PIN_VRX2_LE, OUTPUT);
  pinMode(PIN_VRX3_LE, OUTPUT);

  digitalWrite(PIN_VRX_DATA, LOW);
  digitalWrite(PIN_VRX_CLK, LOW);
  digitalWrite(PIN_VRX1_LE, LOW);
  digitalWrite(PIN_VRX2_LE, LOW);
  digitalWrite(PIN_VRX3_LE, LOW);

  analogReadResolution(12);

  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  Wire.setClock(OLED_I2C_HZ);
  oled_ok = display.begin(0x3C);
  if (!oled_ok) {
    oled_ok = display.begin(0x3D);
  }
  if (oled_ok) {
    drawBootBanner();
  }

  for (int i = 0; i < 3; i++) {
    vrxInit(vrx[i].lePin);
    vrxTuneHz(vrx[i].lePin, vrx[i].freq_hz);
    delay(5);
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

  if ((now - lastRssiMs) >= RSSI_INTERVAL_MS) {
    lastRssiMs = now;
    for (int i = 0; i < 3; i++) {
      vrx[i].rssi_raw = readRSSI(vrx[i].rssiPin);
    }
  }

  scanTick();

  serialPoll();

  if (oled_ok && (now - lastOledMs) >= 1000) {
    lastOledMs = now;
    drawStatus();
  }
}
