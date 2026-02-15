// Baseline firmware import (reference only). This file is excluded from build.
#if 0
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1327.h>

/* =========================================================
   N-DEFENDER ESP32 FRONT PANEL -- FINAL PRODUCTION FW
   Contract V1: capabilities + telemetry + event + cmd/cmd_ack
   Active buzzer (DC siren) digital ON/OFF via transistor
   Telemetry aliases: sel + ui.hold + (f/r inside each vrx[])
   OLED cmd overlay: show last cmd + 3s beep confirm
   ========================================================= */

/* ===================== PIN MAP (LOCKED) ===================== */
#define LED_RED     16
#define LED_YELLOW  15
#define LED_GREEN    7

#define BUZZER_PIN  17   // Active DC siren (ON=HIGH)

#define OLED_SDA    13
#define OLED_SCL    12

// Shared SPI bus
#define VRX_DATA     3
#define VRX_CLK      5

// VRX-1 FS58R3MW (5.8G)
#define VRX1_LE      4
#define VRX1_RSSI    8

// VRX-2 FS3137RX (3.3G)
#define VRX2_LE     10
#define VRX2_RSSI   14

// VRX-3 FS1264R (1.2G)
#define VRX3_LE     11
#define VRX3_RSSI    9

// UI
#define BTN_SCAN    18
#define JOY_X        1
#define JOY_Y        2
#define JOY_SW       6

/* ===================== DEVICE / PROTO ===================== */
static const int   PROTO_VER   = 1;
static const char* FW_VERSION  = "1.2.0";
static const char* DEVICE_MODEL= "N-Defender FrontPanel";
static const char* HW_MODEL    = "ESP32-S3";

/* ===================== TIMING ===================== */
static const uint32_t TELEMETRY_MS = 100;   // 10 Hz
static const uint32_t RSSI_MS      = 120;
static const uint32_t UI_MS        = 150;

/* ===================== OLED ===================== */
Adafruit_SSD1327 display(128, 96, &Wire, -1);
bool oled_ok = false;

/* ===================== TYPES ===================== */
enum JoyDir { J_NONE, J_UP, J_DOWN, J_LEFT, J_RIGHT };
enum Page { PAGE_OVERVIEW, PAGE_DETAIL, PAGE_SCAN, PAGE_TEST };
enum VrxMode { MODE_AUTO, MODE_MANUAL };

struct VRX {
  int id;
  const char* name;
  const char* band;
  uint8_t lePin;
  uint8_t rssiPin;

  VrxMode mode;
  bool scan;
  bool lock;

  const uint16_t* freqs;
  int n;
  int idx;
  uint16_t freq_mhz;

  uint16_t best_freq_mhz;
  uint16_t best_rssi_raw;

  uint16_t rssi_raw;
};

/* ===================== FREQ TABLES ===================== */
static const uint16_t F58[] = {5645,5705,5740,5800,5865,5885,5945};
static const uint16_t F33[] = {3100,3200,3300,3400,3500,3600,3700};
static const uint16_t F12[] = {1080,1120,1200,1280,1400,1600,1800,2000,2200};

VRX vrx[3] = {
  {1,"VRX-1 FS58R3MW","5.8G", VRX1_LE, VRX1_RSSI, MODE_AUTO,false,false, F58, 7, 3, 5800, 5800, 0, 0},
  {2,"VRX-2 FS3137RX","3.3G", VRX2_LE, VRX2_RSSI, MODE_AUTO,false,false, F33, 7, 2, 3300, 3300, 0, 0},
  {3,"VRX-3 FS1264R", "1.2G", VRX3_LE, VRX3_RSSI, MODE_AUTO,false,false, F12, 9, 3, 1280, 1280, 0, 0},
};

/* ===================== GLOBAL STATE ===================== */
int sel_vrx = 1;                 // 1..3
Page page = PAGE_OVERVIEW;

bool holdEnabled = false;
bool muteEnabled = false;
bool panicEnabled = false;
int  videoSel = 1;

String thresholdProfile = "Balanced";
String bandProfile = "5.8G";

uint16_t minLock = 900;
uint16_t alertThresh = 1400;
uint16_t dangerThresh = 1900;
uint16_t clearThresh = 1200;

/* ===================== TEST PAGE CHECKLIST ===================== */
bool chk_tel=false, chk_uart_rx=false, chk_cmd_ack=false, chk_beep=false, chk_scan=false, chk_tune=false, chk_hold=false, chk_mute=false, chk_video=false;

/* ===================== TIMERS ===================== */
uint32_t lastTelemetryMs=0, lastRssiMs=0, lastUiMs=0;

/* ===================== UART RX ===================== */
String rxLine;
uint32_t cmdErrors=0;

/* ===================== JOYSTICK ===================== */
int joyCX=2048, joyCY=2048;
uint32_t joyLastMove=0;

/* ===================== ACTIVE BUZZER SCHEDULER ===================== */
bool buzOn=false;
uint32_t buzOffAt=0;

static inline void buzzerHWOff(){ digitalWrite(BUZZER_PIN, LOW); buzOn=false; }
static inline void buzzerHWOn(){ if(!muteEnabled){ digitalWrite(BUZZER_PIN, HIGH); buzOn=true; } }

static void buzzerBeepMs(uint32_t ms){
  if(muteEnabled) return;
  buzzerHWOn();
  buzOffAt = millis() + ms;
}

static void buzzerTick(){
  if(buzOn && (int32_t)(millis()-buzOffAt)>=0) buzzerHWOff();
  if(panicEnabled && !muteEnabled){
    if((millis()/150)%2==0) buzzerHWOn();
    else buzzerHWOff();
  }
}

/* ===================== OLED COMMAND OVERLAY ===================== */
bool cmdOverlayActive = false;
uint32_t cmdOverlayUntil = 0;
String lastCmd = "";
String lastReq = "";
bool lastCmdOk = true;
String lastErr = "";

static void cmdOverlayShow(const String& req, const String& cmd, bool ok, const String& err) {
  cmdOverlayActive = true;
  cmdOverlayUntil = millis() + 5000;   // show 5 seconds
  lastReq = req;
  lastCmd = cmd;
  lastCmdOk = ok;
  lastErr = err;

  // non-blocking long confirm beep (3 seconds)
  buzzerBeepMs(3000);
}

/* ===================== LED ===================== */
static void setLED(bool r,bool y,bool g){
  digitalWrite(LED_RED,r);
  digitalWrite(LED_YELLOW,y);
  digitalWrite(LED_GREEN,g);
}

/* ===================== VRX SPI (25-bit LSB-first) ===================== */
static inline void clkPulse(){
  digitalWrite(VRX_CLK,HIGH); delayMicroseconds(1);
  digitalWrite(VRX_CLK,LOW);  delayMicroseconds(1);
}
static inline void sendBit(uint8_t b){
  digitalWrite(VRX_DATA, b ? HIGH : LOW);
  clkPulse();
}
static void vrxWriteReg(uint8_t le, uint8_t addr4, uint32_t data20){
  addr4 &= 0x0F;
  data20 &= 0xFFFFF;
  uint32_t packet = (data20 << 5) | (1UL<<4) | addr4;

  digitalWrite(le, LOW);
  for(int i=0;i<25;i++) sendBit((packet >> i) & 1U);
  digitalWrite(le, HIGH); delayMicroseconds(2);
  digitalWrite(le, LOW);  delayMicroseconds(80);
}
static uint32_t synthWord(uint16_t rf_mhz){
  int lo = (int)rf_mhz - 479; if(lo<0) lo=0;
  int div = lo/2;
  int N = div/32;
  int A = div%32;
  return ((uint32_t)N<<7) | (uint32_t)A;
}
static void vrxInit(VRX& v){ vrxWriteReg(v.lePin, 0x00, 0x00008); }
static void vrxTune(VRX& v, uint16_t mhz){
  vrxWriteReg(v.lePin, 0x01, synthWord(mhz));
  v.freq_mhz = mhz;
}

/* ===================== RSSI ===================== */
static uint16_t readRSSI(uint8_t pin){
  uint32_t s=0;
  for(int i=0;i<6;i++){ s += analogRead(pin); delayMicroseconds(200); }
  return (uint16_t)(s/6);
}

/* ===================== FPV STATE ===================== */
static String fpv_state(){
  if(holdEnabled) return "hold";
  bool anyScan=false, anyLock=false;
  for(int i=0;i<3;i++){
    if(vrx[i].scan) anyScan=true;
    if(vrx[i].lock) anyLock=true;
  }
  if(anyLock) return "locked";
  if(anyScan) return "scanning";
  return "stopped";
}

static void applyThresholdProfile(const String& p){
  thresholdProfile = p;
  if(p=="Sensitive"){ minLock=700; alertThresh=1100; dangerThresh=1600; clearThresh=900; }
  else if(p=="Strict"){ minLock=1100; alertThresh=1500; dangerThresh=2100; clearThresh=1300; }
  else { thresholdProfile="Balanced"; minLock=900; alertThresh=1400; dangerThresh=1900; clearThresh=1200; }
}

/* ===================== OLED UI ===================== */
static void drawBar(int x,int y,int w,int h,uint16_t v){
  int fill = map((int)v,0,4095,0,w);
  fill = constrain(fill,0,w);
  display.drawRect(x,y,w,h,SSD1327_WHITE);
  if(fill>=2) display.fillRect(x+1,y+1,fill-2,h-2,SSD1327_WHITE);
}

static void uiOverview(){
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1327_WHITE);

  display.setCursor(0,0);
  display.println("FPV OVERVIEW (3 VRX)");

  display.setCursor(0,12);
  display.print("SEL VRX"); display.print(sel_vrx);
  display.print("  "); display.print(fpv_state());

  int y=26;
  for(int i=0;i<3;i++){
    display.setCursor(0,y);
    display.print((sel_vrx==(i+1))?">":" ");
    display.print(vrx[i].band);
    display.print(" F:"); display.print(vrx[i].freq_mhz);
    display.print(" R:"); display.println(vrx[i].rssi_raw);
    drawBar(0,y+10,128,8,vrx[i].rssi_raw);
    y += 20;
  }

  display.setCursor(0,92);
  display.println("BTN=SCAN  SW=DETAIL");
  display.display();
}

static void uiDetail(){
  int i = sel_vrx-1;
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1327_WHITE);

  display.setCursor(0,0);
  display.print("DETAIL VRX"); display.println(sel_vrx);
  display.println(vrx[i].name);

  display.setCursor(0,24);
  display.print("Band: "); display.println(vrx[i].band);

  display.setCursor(0,34);
  display.print("Freq: "); display.print(vrx[i].freq_mhz); display.println(" MHz");

  display.setCursor(0,44);
  display.print("RSSI: "); display.println(vrx[i].rssi_raw);

  drawBar(0,58,128,10,vrx[i].rssi_raw);

  display.setCursor(0,74);
  display.println("L/R VRX  U/D Tune");
  display.setCursor(0,86);
  display.println("SW=OVR  SW->TEST");
  display.display();
}

static void uiScan(){
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1327_WHITE);
  display.setCursor(0,0);
  display.println("SCANNING...");
  for(int i=0;i<3;i++){
    display.setCursor(0,14+i*14);
    display.print("V"); display.print(i+1);
    display.print(" best "); display.print(vrx[i].best_freq_mhz);
    display.print(" R"); display.println(vrx[i].best_rssi_raw);
  }
  display.display();
}

static void uiTest(){
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1327_WHITE);

  int y=0;
  auto row=[&](const char* label,bool ok){
    display.setCursor(0,y);
    display.print(ok?"[OK] ":"[  ] ");
    display.print(label);
    y+=10;
  };

  row("TELEMETRY", chk_tel);
  row("UART RX",   chk_uart_rx);
  row("CMD ACK",   chk_cmd_ack);
  row("BEEP",      chk_beep);
  row("SCAN CMD",  chk_scan);
  row("TUNE CMD",  chk_tune);
  row("HOLD CMD",  chk_hold);
  row("MUTE CMD",  chk_mute);
  row("VIDEO CMD", chk_video);

  display.setCursor(0,92);
  display.println("SW=EXIT TEST");
  display.display();
}

static void uiCmdOverlay(){
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1327_WHITE);

  display.setCursor(0,0);
  display.println("SERIAL CMD RECEIVED");

  display.setCursor(0,14);
  display.print("REQ: ");
  display.println(lastReq.length() ? lastReq : "-");

  display.setCursor(0,28);
  display.print("CMD: ");
  String c = lastCmd;
  if (c.length() > 18) c = c.substring(0,18);
  display.println(c);

  display.setCursor(0,42);
  display.print("RESULT: ");
  display.println(lastCmdOk ? "OK" : "FAIL");

  display.setCursor(0,56);
  display.print("ERR: ");
  if (lastErr.length() == 0) display.println("-");
  else {
    String e = lastErr;
    if (e.length() > 18) e = e.substring(0,18);
    display.println(e);
  }

  display.setCursor(0,86);
  display.println("Beep=cmd confirm");
  display.display();
}

/* ===================== SERIAL JSON ===================== */
static void sendCapabilities(){
  uint32_t ms = millis();
  Serial.print("{\"type\":\"capabilities\",\"proto\":"); Serial.print(PROTO_VER);
  Serial.print(",\"esp_ms\":"); Serial.print(ms);
  Serial.print(",\"ts_ms\":");  Serial.print(ms);
  Serial.print(",\"device\":{\"model\":\""); Serial.print(DEVICE_MODEL);
  Serial.print("\",\"fw_version\":\""); Serial.print(FW_VERSION);
  Serial.print("\",\"hw\":\""); Serial.print(HW_MODEL); Serial.print("\"}");
  Serial.print(",\"features\":{");
  Serial.print("\"fpv_scan\":true,\"fpv_lock\":true,\"fpv_hold\":true,\"manual_tune\":true,");
  Serial.print("\"band_profiles\":[\"5.8G\",\"3.3G\",\"1.2G\"],");
  Serial.print("\"threshold_profiles\":[\"Sensitive\",\"Balanced\",\"Strict\"],");
  Serial.print("\"video_switch\":true,\"buzzer\":true,\"leds\":true,\"physical_mute_btn\":true}");
  Serial.print(",\"vrx\":{\"count\":3,\"ids\":[1,2,3]}}");
  Serial.println();
}

static void sendEvent(const char* ev, int value){
  uint32_t ms = millis();
  Serial.print("{\"type\":\"event\",\"proto\":"); Serial.print(PROTO_VER);
  Serial.print(",\"esp_ms\":"); Serial.print(ms);
  Serial.print(",\"ts_ms\":");  Serial.print(ms);
  Serial.print(",\"event\":\""); Serial.print(ev);
  Serial.print("\",\"value\":"); Serial.print(value);
  Serial.println("}");
}

static void sendCmdAck(const String& req_id, const String& cmd, bool ok, const String& err){
  uint32_t ms = millis();
  Serial.print("{\"type\":\"cmd_ack\",\"proto\":"); Serial.print(PROTO_VER);
  Serial.print(",\"req_id\":\""); Serial.print(req_id);
  Serial.print("\",\"cmd\":\""); Serial.print(cmd);
  Serial.print("\",\"ok\":"); Serial.print(ok?"true":"false");
  Serial.print(",\"err\":");
  if(err.length()==0) Serial.print("null");
  else { Serial.print("\""); Serial.print(err); Serial.print("\""); }
  Serial.print(",\"esp_ms\":"); Serial.print(ms);
  Serial.print(",\"ts_ms\":");  Serial.print(ms);
  Serial.println("}");
}

/* ======= UPDATED TELEMETRY (WITH ALIASES sel/ui/f/r) ======= */
static void sendTelemetry(){
  chk_tel = true;
  uint32_t ms = millis();

  Serial.print("{\"type\":\"telemetry\",\"proto\":"); Serial.print(PROTO_VER);
  Serial.print(",\"esp_ms\":"); Serial.print(ms);
  Serial.print(",\"ts_ms\":");  Serial.print(ms);

  // alias: sel
  Serial.print(",\"sel\":"); Serial.print(sel_vrx);

  // alias: ui.hold + ui.vrx
  Serial.print(",\"ui\":{\"hold\":"); Serial.print(holdEnabled?1:0);
  Serial.print(",\"vrx\":"); Serial.print(sel_vrx);
  Serial.print("}");

  Serial.print(",\"link\":{\"rx_age_ms\":0,\"errors\":"); Serial.print(cmdErrors); Serial.print("}");

  Serial.print(",\"panel\":{\"sel_vrx\":"); Serial.print(sel_vrx);
  Serial.print(",\"hold\":"); Serial.print(holdEnabled?1:0);
  Serial.print(",\"mute\":"); Serial.print(muteEnabled?1:0);
  Serial.print(",\"panic\":"); Serial.print(panicEnabled?1:0);
  Serial.print("}");

  Serial.print(",\"fpv\":{\"band\":\""); Serial.print(vrx[sel_vrx-1].band);
  Serial.print("\",\"threshold\":\""); Serial.print(thresholdProfile);
  Serial.print("\",\"state\":\""); Serial.print(fpv_state());
  Serial.print("\",\"locked_vrx\":[],\"active_vrx\":[1,2,3]}");

  Serial.print(",\"vrx\":[");
  for(int i=0;i<3;i++){
    Serial.print("{\"id\":"); Serial.print(vrx[i].id);

    Serial.print(",\"mode\":\""); Serial.print(vrx[i].mode==MODE_AUTO?"AUTO":"MANUAL"); Serial.print("\"");
    Serial.print(",\"lock\":"); Serial.print(vrx[i].lock?1:0);
    Serial.print(",\"scan\":"); Serial.print(vrx[i].scan?1:0);

    // existing keys
    Serial.print(",\"freq_mhz\":"); Serial.print(vrx[i].freq_mhz);
    Serial.print(",\"idx\":"); Serial.print(vrx[i].idx);
    Serial.print(",\"rssi_raw\":"); Serial.print(vrx[i].rssi_raw);

    // alias keys per VRX
    Serial.print(",\"f\":"); Serial.print(vrx[i].freq_mhz);
    Serial.print(",\"r\":"); Serial.print(vrx[i].rssi_raw);

    Serial.print(",\"rssi_dbm\":null}");

    if(i<2) Serial.print(",");
  }
  Serial.println("]}");
}

/* ===================== SCAN/TUNE ===================== */
static void tuneIndex(int v, int idx){
  if(idx < 0) idx = 0;
  if(idx >= vrx[v].n) idx = vrx[v].n-1;
  vrx[v].idx = idx;
  vrx[v].mode = MODE_MANUAL;
  vrxTune(vrx[v], vrx[v].freqs[idx]);
  vrx[v].lock = true;
}

static void scanOne(int v){
  if(holdEnabled) return;
  vrx[v].scan = true;
  vrx[v].lock = false;
  vrx[v].mode = MODE_AUTO;

  uint16_t bestR=0; int bestI=0;
  for(int i=0;i<vrx[v].n;i++){
    vrxTune(vrx[v], vrx[v].freqs[i]);
    delay(35);
    uint16_t r = readRSSI(vrx[v].rssiPin);
    if(r > bestR){ bestR=r; bestI=i; }
  }

  vrx[v].best_rssi_raw = bestR;
  vrx[v].best_freq_mhz = vrx[v].freqs[bestI];
  vrx[v].idx = bestI;
  vrxTune(vrx[v], vrx[v].best_freq_mhz);
  if(bestR >= minLock) vrx[v].lock = true;

  vrx[v].scan = false;
}

static void scanAll(){
  if(holdEnabled) return;
  chk_scan = true;

  page = PAGE_SCAN;
  buzzerBeepMs(450); // long beep start
  sendEvent("SCAN_START_PRESS", 1);

  for(int v=0; v<3; v++){
    scanOne(v);
    buzzerBeepMs(70);
    delay(60);
  }

  sendEvent("SCAN_STOP_PRESS", 1);
  page = PAGE_OVERVIEW;
}

/* ===================== SIMPLE JSON FIELD EXTRACTION ===================== */
static bool getStringField(const String& s, const char* key, String& out){
  String k = String("\"") + key + "\":\"";
  int i = s.indexOf(k);
  if(i<0) return false;
  i += k.length();
  int j = s.indexOf("\"", i);
  if(j<0) return false;
  out = s.substring(i,j);
  return true;
}
static bool getIntField(const String& s, const char* key, int& out){
  String k = String("\"") + key + "\":";
  int i = s.indexOf(k);
  if(i<0) return false;
  i += k.length();
  while(i<(int)s.length() && s[i]==' ') i++;
  int j=i;
  while(j<(int)s.length() && (isDigit(s[j])||s[j]=='-')) j++;
  if(j<=i) return false;
  out = s.substring(i,j).toInt();
  return true;
}

/* ===================== CMD HANDLER ===================== */
static void handleCmdLine(const String& line){
  chk_uart_rx = true;

  String type="", cmd="", req="";
  getStringField(line, "type", type);
  getStringField(line, "cmd", cmd);
  getStringField(line, "req_id", req);

  if(type != "cmd" || cmd.length()==0){ cmdErrors++; return; }
  if(req.length()==0) req="no_req";

  auto ackOK=[&](){
    chk_cmd_ack=true;
    sendCmdAck(req, cmd, true, "");
    cmdOverlayShow(req, cmd, true, "");
  };
  auto ackER=[&](const String& e){
    chk_cmd_ack=true;
    sendCmdAck(req, cmd, false, e);
    cmdOverlayShow(req, cmd, false, e);
  };

  if(cmd=="TEST_BEEP"){
    chk_beep=true;
    // just overlay + 3s beep (non-blocking)
    cmdOverlayShow(req, cmd, true, "");
    sendCmdAck(req, cmd, true, "");
    return;
  }

  if(cmd=="FPV_SCAN_START"){ scanAll(); ackOK(); return; }

  if(cmd=="FPV_HOLD_SET"){
    int h=0; if(!getIntField(line,"hold",h)) { ackER("missing hold"); return; }
    holdEnabled = (h!=0);
    chk_hold=true;
    sendEvent("HOLD_SET", holdEnabled?1:0);
    ackOK();
    return;
  }

  if(cmd=="MUTE_SET"){
    int m=0; if(!getIntField(line,"mute",m)) { ackER("missing mute"); return; }
    bool newMute = (m!=0);
    chk_mute=true;

    // ACK + overlay before applying mute=1 (so you can still hear/see it)
    sendEvent("MUTE_SET", newMute?1:0);
    sendCmdAck(req, cmd, true, "");
    cmdOverlayShow(req, cmd, true, "");

    muteEnabled = newMute;
    if(muteEnabled) buzzerHWOff();
    return;
  }

  if(cmd=="VIDEO_SELECT"){
    int s=0; if(!getIntField(line,"sel",s)) { ackER("missing sel"); return; }
    if(s<1||s>3){ ackER("sel_oob"); return; }
    videoSel=s;
    sel_vrx=s;
    chk_video=true;
    sendEvent("VIDEO_SELECT", s);
    ackOK();
    return;
  }

  if(cmd=="FPV_TUNE_FREQ"){
    int vid=0,f=0;
    if(!getIntField(line,"vrx_id",vid) || !getIntField(line,"freq_mhz",f)) { ackER("missing vrx_id/freq_mhz"); return; }
    if(vid<1||vid>3){ ackER("vrx_id_oob"); return; }
    int v=vid-1;
    vrx[v].mode = MODE_MANUAL;
    vrxTune(vrx[v], (uint16_t)f);
    vrx[v].lock = true;
    chk_tune=true;
    sendEvent("FPV_TUNE_FREQ", vid);
    ackOK();
    return;
  }

  if(cmd=="FPV_TUNE_INDEX"){
    int vid=0,idx=0;
    if(!getIntField(line,"vrx_id",vid) || !getIntField(line,"idx",idx)) { ackER("missing vrx_id/idx"); return; }
    if(vid<1||vid>3){ ackER("vrx_id_oob"); return; }
    int v=vid-1;
    if(idx<0 || idx>=vrx[v].n){ ackER("idx_oob"); return; }
    tuneIndex(v, idx);
    chk_tune=true;
    sendEvent("FPV_TUNE_INDEX", vid);
    ackOK();
    return;
  }

  if(cmd=="FPV_SET_THRESHOLD_PROFILE"){
    String th="";
    if(!getStringField(line,"threshold",th)) { ackER("missing threshold"); return; }
    applyThresholdProfile(th);
    ackOK();
    return;
  }

  if(cmd=="FPV_SET_BAND_PROFILE"){
    String b="";
    if(!getStringField(line,"band",b)) { ackER("missing band"); return; }
    bandProfile=b;
    ackOK();
    return;
  }

  ackER("unknown_cmd");
}

/* ===================== INPUT / DEBOUNCE ===================== */
static inline bool pressedLow(uint8_t p){ return digitalRead(p)==LOW; }
struct Debounce{
  uint8_t p=0; bool last=false; uint32_t t=0; bool fell=false;
  void begin(uint8_t pin){ p=pin; last=pressedLow(p); t=millis(); }
  void update(){
    fell=false;
    bool n=pressedLow(p);
    if(n!=last && (millis()-t)>30){
      t=millis();
      if(!last && n) fell=true;
      last=n;
    }
  }
} dbScan, dbJoy;

static JoyDir readJoyDir(){
  if(millis()-joyLastMove < 160) return J_NONE;
  int dx = analogRead(JOY_X) - joyCX;
  int dy = analogRead(JOY_Y) - joyCY;
  if(dy > 450){ joyLastMove=millis(); return J_UP; }
  if(dy < -450){ joyLastMove=millis(); return J_DOWN; }
  if(dx > 450){ joyLastMove=millis(); return J_RIGHT; }
  if(dx < -450){ joyLastMove=millis(); return J_LEFT; }
  return J_NONE;
}

/* ===================== ALERT OUTPUTS ===================== */
static void updateAlerts(){
  uint16_t r = vrx[sel_vrx-1].rssi_raw;

  if(holdEnabled){
    setLED(false,true,false);
    return;
  }

  if(r >= dangerThresh){
    setLED(true,false,false);
    if(!muteEnabled){
      if((millis()/150)%2==0) buzzerHWOn();
      else buzzerHWOff();
    }
    return;
  }

  if(r >= alertThresh){
    setLED(false,true,false);
    if(!muteEnabled){
      if((millis()/900)%2==0) buzzerBeepMs(120);
    }
    return;
  }

  setLED(false,false,true);
}

/* ===================== SETUP ===================== */
void setup(){
  Serial.begin(115200);
  delay(120);

  pinMode(LED_RED,OUTPUT);
  pinMode(LED_YELLOW,OUTPUT);
  pinMode(LED_GREEN,OUTPUT);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW); // FORCE OFF

  pinMode(BTN_SCAN, INPUT_PULLUP);
  pinMode(JOY_SW, INPUT_PULLUP);

  pinMode(VRX_DATA, OUTPUT);
  pinMode(VRX_CLK, OUTPUT);

  pinMode(VRX1_LE, OUTPUT);
  pinMode(VRX2_LE, OUTPUT);
  pinMode(VRX3_LE, OUTPUT);

  digitalWrite(VRX_DATA, LOW);
  digitalWrite(VRX_CLK, LOW);
  digitalWrite(VRX1_LE, LOW);
  digitalWrite(VRX2_LE, LOW);
  digitalWrite(VRX3_LE, LOW);

  analogReadResolution(12);

  Wire.begin(OLED_SDA, OLED_SCL);
  Wire.setClock(400000);
  oled_ok = display.begin(0x3C);
  if(!oled_ok) oled_ok = display.begin(0x3D);

  delay(200);
  joyCX = analogRead(JOY_X);
  joyCY = analogRead(JOY_Y);

  dbScan.begin(BTN_SCAN);
  dbJoy.begin(JOY_SW);

  for(int i=0;i<3;i++){
    vrxInit(vrx[i]);
    vrxTune(vrx[i], vrx[i].best_freq_mhz);
    delay(20);
  }

  applyThresholdProfile("Balanced");
  setLED(false,false,true);

  // boot beep (short-short)
  buzzerBeepMs(80); delay(120); buzzerBeepMs(80);

  sendCapabilities();
  sendTelemetry();

  if(oled_ok) uiOverview();
}

/* ===================== LOOP ===================== */
void loop(){
  buzzerTick();

  // RSSI update
  if(millis() - lastRssiMs > RSSI_MS){
    lastRssiMs = millis();
    for(int i=0;i<3;i++){
      vrx[i].rssi_raw = readRSSI(vrx[i].rssiPin);
    }
  }

  // inputs
  dbScan.update();
  dbJoy.update();

  if(dbScan.fell && page != PAGE_SCAN){
    scanAll();
  }

  // Joy SW cycles pages (Overview -> Detail -> Test -> Overview)
  if(dbJoy.fell){
    if(page == PAGE_OVERVIEW) page = PAGE_DETAIL;
    else if(page == PAGE_DETAIL) page = PAGE_TEST;
    else page = PAGE_OVERVIEW;
    buzzerBeepMs(60);
  }

  JoyDir d = readJoyDir();

  if(page == PAGE_OVERVIEW || page == PAGE_DETAIL){
    if(d == J_LEFT)  { sel_vrx = (sel_vrx % 3) + 1; buzzerBeepMs(40); }
    if(d == J_RIGHT) { sel_vrx = (sel_vrx - 2 + 3) % 3 + 1; buzzerBeepMs(40); }
  }

  if(page == PAGE_DETAIL && !holdEnabled){
    int v = sel_vrx-1;
    if(d == J_UP){
      tuneIndex(v, (vrx[v].idx + 1) % vrx[v].n);
      sendEvent("LOCAL_TUNE_UP", sel_vrx);
      buzzerBeepMs(50);
    }
    if(d == J_DOWN){
      tuneIndex(v, (vrx[v].idx - 1 + vrx[v].n) % vrx[v].n);
      sendEvent("LOCAL_TUNE_DOWN", sel_vrx);
      buzzerBeepMs(50);
    }
  }

  updateAlerts();

  // UART RX (newline JSON)
  while(Serial.available()){
    char c = (char)Serial.read();
    if(c=='\n'){
      if(rxLine.length() && rxLine[0]=='{') handleCmdLine(rxLine);
      rxLine = "";
    } else if(c!='\r'){
      rxLine += c;
      if(rxLine.length() > 900){ rxLine=""; cmdErrors++; }
    }
  }

  // telemetry
  if(millis() - lastTelemetryMs >= TELEMETRY_MS){
    lastTelemetryMs = millis();
    sendTelemetry();
  }

  // OLED refresh (command overlay has priority)
  if(oled_ok && millis() - lastUiMs > UI_MS){
    lastUiMs = millis();

    if(cmdOverlayActive){
      uiCmdOverlay();
      if((int32_t)(millis() - cmdOverlayUntil) >= 0){
        cmdOverlayActive = false;
      }
    } else {
      if(page == PAGE_OVERVIEW) uiOverview();
      else if(page == PAGE_DETAIL) uiDetail();
      else if(page == PAGE_SCAN) uiScan();
      else uiTest();
    }
  }
}
#endif
