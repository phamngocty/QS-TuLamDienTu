# 📦 Project Layout (copy these files into your PlatformIO project)

> Board target: **ESP32-C3** (Arduino) Default: **Backfire OFF**; GPIOs centralized in `src/pins.h`

---

## `platformio.ini`

```ini
[env:esp32c3]
platform = espressif32@6.6.0
board = esp32-c3-devkitm-1
framework = arduino
monitor_speed = 115200
upload_speed = 921600
build_flags =
  -D ARDUINO_USB_MODE=1
  -D ARDUINO_USB_CDC_ON_BOOT=1
  -D CORE_DEBUG_LEVEL=1
lib_deps =
  me-no-dev/ESP Async WebServer @ ^1.2.4
  ottowinter/ESPAsyncTCP @ ^1.2.2
  bblanchon/ArduinoJson @ ^7.0.0
```

---

## `src/pins.h`

```cpp
#pragma once
#include <Arduino.h>

// ===== Centralized GPIO mapping (change here) =====
// RMT-capable pin recommended for RPM_IN if possible on your C3 module
static constexpr uint8_t PIN_RPM_IN      = 2;  // RMT input from coil/injector via opto/schmitt
static constexpr uint8_t PIN_SHIFT_NPN   = 4;  // NPN proximity sensor (active LOW via pull-up/opto)
static constexpr uint8_t PIN_CUT_IGN     = 6;  // Relay NC for ignition cut (driver)
static constexpr uint8_t PIN_CUT_INJ     = 7;  // Relay NC for injector cut (driver)
static constexpr uint8_t PIN_PWM_TEST    = 8;  // PWM out to simulate RPM
static constexpr uint8_t PIN_STATUS_LED  = 3;  // Optional LED
```

---

## `src/config.h`

```cpp
#pragma once
#include <Arduino.h>

// ===== Defaults & Limits =====
// Modes
enum class Mode : uint8_t { MANUAL = 0, AUTO = 1 };
enum class RpmSource : uint8_t { COIL = 0, INJECTOR = 1 };
enum class CutOutputSel : uint8_t { IGN = 0, INJ = 1 };

struct AutoBand { uint16_t rpm_lo; uint16_t rpm_hi; uint16_t cut_ms; };

struct QSConfig {
  Mode mode = Mode::AUTO;
  RpmSource rpm_source = RpmSource::COIL;
  float ppr = 1.0f;                 // 0.5 / 1 / 2 selectable
  uint16_t rpm_min = 2500;          // Below this no cut
  uint16_t manual_kill_ms = 70;     // Manual cut time
  uint16_t debounce_shift_ms = 15;  // Shift sensor debounce
  uint16_t holdoff_ms = 200;        // Lockout after cut
  CutOutputSel cut_output = CutOutputSel::IGN; // default output
  // Backfire (relay-simple): OFF by default per user request
  bool backfire_enabled = false;
  uint16_t backfire_extra_ms = 15;  // Extra cut when backfire enabled
  uint16_t backfire_min_rpm = 4500; // Only above this rpm
  // AP timeout
  uint16_t ap_timeout_s = 120;      // 0 = never auto close
  // Calibration
  float rpm_scale = 1.0f;           // rpm_display = rpm_raw * rpm_scale
  // Auto map (up to 6 bands typical)
  AutoBand map[4] = {
    {2500, 4000, 85},
    {4000, 7000, 65},
    {7000,10000, 50},
    {10000,20000,45}
  };
  uint8_t map_count = 4;
};

// Limits / safety
static constexpr uint16_t CUT_MS_MAX = 150; // hard cap
static constexpr uint16_t CUT_MS_MIN = 20;
```

---

## `src/config_store.h`

```cpp
#pragma once
#include "config.h"

namespace CFG {
  void begin();
  QSConfig get();
  void set(const QSConfig &c);
  bool exportJSON(String &out);
  bool importJSON(const String &in);
}
```

## `src/config_store.cpp`

```cpp
#include "config_store.h"
#include <Preferences.h>
#include <ArduinoJson.h>

static Preferences prefs;
static QSConfig g_cfg;

void CFG::begin(){
  prefs.begin("qs", false);
  // Load minimal keys (if exist)
  g_cfg.mode = (Mode)prefs.getUChar("mode", (uint8_t)g_cfg.mode);
  g_cfg.rpm_source = (RpmSource)prefs.getUChar("rsrc", (uint8_t)g_cfg.rpm_source);
  g_cfg.ppr = prefs.getFloat("ppr", g_cfg.ppr);
  g_cfg.rpm_min = prefs.getUShort("rpmmin", g_cfg.rpm_min);
  g_cfg.manual_kill_ms = prefs.getUShort("mkill", g_cfg.manual_kill_ms);
  g_cfg.debounce_shift_ms = prefs.getUShort("deb", g_cfg.debounce_shift_ms);
  g_cfg.holdoff_ms = prefs.getUShort("hold", g_cfg.holdoff_ms);
  g_cfg.cut_output = (CutOutputSel)prefs.getUChar("cout", (uint8_t)g_cfg.cut_output);
  g_cfg.backfire_enabled = prefs.getBool("bf_en", g_cfg.backfire_enabled);
  g_cfg.backfire_extra_ms = prefs.getUShort("bf_ex", g_cfg.backfire_extra_ms);
  g_cfg.backfire_min_rpm = prefs.getUShort("bf_min", g_cfg.backfire_min_rpm);
  g_cfg.ap_timeout_s = prefs.getUShort("ap_t", g_cfg.ap_timeout_s);
  g_cfg.rpm_scale = prefs.getFloat("rpm_s", g_cfg.rpm_scale);
  // Map load (simple fixed slots)
  for (uint8_t i=0;i<g_cfg.map_count;i++){
    char key[8];
    snprintf(key, sizeof(key), "m%dl", i); g_cfg.map[i].rpm_lo = prefs.getUShort(key, g_cfg.map[i].rpm_lo);
    snprintf(key, sizeof(key), "m%dh", i); g_cfg.map[i].rpm_hi = prefs.getUShort(key, g_cfg.map[i].rpm_hi);
    snprintf(key, sizeof(key), "m%dt", i); g_cfg.map[i].cut_ms  = prefs.getUShort(key, g_cfg.map[i].cut_ms);
  }
}

QSConfig CFG::get(){ return g_cfg; }

void CFG::set(const QSConfig &c){
  g_cfg = c;
  prefs.putUChar("mode", (uint8_t)c.mode);
  prefs.putUChar("rsrc", (uint8_t)c.rpm_source);
  prefs.putFloat("ppr", c.ppr);
  prefs.putUShort("rpmmin", c.rpm_min);
  prefs.putUShort("mkill", c.manual_kill_ms);
  prefs.putUShort("deb", c.debounce_shift_ms);
  prefs.putUShort("hold", c.holdoff_ms);
  prefs.putUChar("cout", (uint8_t)c.cut_output);
  prefs.putBool("bf_en", c.backfire_enabled);
  prefs.putUShort("bf_ex", c.backfire_extra_ms);
  prefs.putUShort("bf_min", c.backfire_min_rpm);
  prefs.putUShort("ap_t", c.ap_timeout_s);
  prefs.putFloat("rpm_s", c.rpm_scale);
  for (uint8_t i=0;i<c.map_count;i++){
    char key[8];
    snprintf(key, sizeof(key), "m%dl", i); prefs.putUShort(key, c.map[i].rpm_lo);
    snprintf(key, sizeof(key), "m%dh", i); prefs.putUShort(key, c.map[i].rpm_hi);
    snprintf(key, sizeof(key), "m%dt", i); prefs.putUShort(key, c.map[i].cut_ms);
  }
}

bool CFG::exportJSON(String &out){
  StaticJsonDocument<1024> d;
  d["mode"] = (uint8_t)g_cfg.mode;
  d["rpm_source"] = (uint8_t)g_cfg.rpm_source;
  d["ppr"] = g_cfg.ppr;
  d["rpm_min"] = g_cfg.rpm_min;
  d["manual_kill_ms"] = g_cfg.manual_kill_ms;
  d["debounce_shift_ms"] = g_cfg.debounce_shift_ms;
  d["holdoff_ms"] = g_cfg.holdoff_ms;
  d["cut_output"] = (uint8_t)g_cfg.cut_output;
  d["backfire_enabled"] = g_cfg.backfire_enabled;
  d["backfire_extra_ms"] = g_cfg.backfire_extra_ms;
  d["backfire_min_rpm"] = g_cfg.backfire_min_rpm;
  d["ap_timeout_s"] = g_cfg.ap_timeout_s;
  d["rpm_scale"] = g_cfg.rpm_scale;
  JsonArray m = d.createNestedArray("map");
  for (uint8_t i=0;i<g_cfg.map_count;i++){
    JsonObject o = m.createNestedObject();
    o["lo"] = g_cfg.map[i].rpm_lo;
    o["hi"] = g_cfg.map[i].rpm_hi;
    o["t"]  = g_cfg.map[i].cut_ms;
  }
  serializeJsonPretty(d, out);
  return true;
}

bool CFG::importJSON(const String &in){
  StaticJsonDocument<2048> d;
  auto err = deserializeJson(d, in);
  if (err) return false;
  QSConfig c = g_cfg; // start from current
  if (d.containsKey("mode")) c.mode = (Mode)(uint8_t)d["mode"].as<uint8_t>();
  if (d.containsKey("rpm_source")) c.rpm_source = (RpmSource)(uint8_t)d["rpm_source"].as<uint8_t>();
  if (d.containsKey("ppr")) c.ppr = d["ppr"].as<float>();
  if (d.containsKey("rpm_min")) c.rpm_min = d["rpm_min"].as<uint16_t>();
  if (d.containsKey("manual_kill_ms")) c.manual_kill_ms = d["manual_kill_ms"].as<uint16_t>();
  if (d.containsKey("debounce_shift_ms")) c.debounce_shift_ms = d["debounce_shift_ms"].as<uint16_t>();
  if (d.containsKey("holdoff_ms")) c.holdoff_ms = d["holdoff_ms"].as<uint16_t>();
  if (d.containsKey("cut_output")) c.cut_output = (CutOutputSel)(uint8_t)d["cut_output"].as<uint8_t>();
  if (d.containsKey("backfire_enabled")) c.backfire_enabled = d["backfire_enabled"].as<bool>();
  if (d.containsKey("backfire_extra_ms")) c.backfire_extra_ms = d["backfire_extra_ms"].as<uint16_t>();
  if (d.containsKey("backfire_min_rpm")) c.backfire_min_rpm = d["backfire_min_rpm"].as<uint16_t>();
  if (d.containsKey("ap_timeout_s")) c.ap_timeout_s = d["ap_timeout_s"].as<uint16_t>();
  if (d.containsKey("rpm_scale")) c.rpm_scale = d["rpm_scale"].as<float>();
  if (d.containsKey("map")){
    JsonArray m = d["map"].as<JsonArray>();
    uint8_t idx=0; for (JsonObject o : m){ if (idx>=4) break; c.map[idx].rpm_lo = o["lo"]; c.map[idx].rpm_hi = o["hi"]; c.map[idx].cut_ms = o["t"]; idx++; }
    c.map_count = min<uint8_t>(m.size(),4);
  }
  CFG::set(c);
  return true;
}
```

---

## `src/log_ring.h`

```cpp
#pragma once
#include <Arduino.h>

struct LogItem {
  uint32_t ts_ms; uint16_t rpm; uint16_t cut_ms; bool auto_mode; bool backfire; char out[4]; char reason[8];
};

namespace LOGR {
  void begin();
  void push(const LogItem &it);
  size_t readAllToJson(String &out);
  void clear();
}
```

## `src/log_ring.cpp`

```cpp
#include "log_ring.h"
#include <ArduinoJson.h>

static constexpr size_t RING_SZ = 64;
static LogItem ring[RING_SZ];
static volatile uint16_t head = 0; // next write

void LOGR::begin(){ head = 0; }

void LOGR::push(const LogItem &it){ ring[head % RING_SZ] = it; head++; }

size_t LOGR::readAllToJson(String &out){
  StaticJsonDocument<4096> d; JsonArray a = d.to<JsonArray>();
  uint16_t cnt = min<uint16_t>(head, RING_SZ);
  for (int i=0;i<cnt;i++){
    const LogItem &it = ring[(head - cnt + i) % RING_SZ];
    JsonObject o = a.createNestedObject();
    o["t"]=it.ts_ms; o["rpm"]=it.rpm; o["cut"]=it.cut_ms; o["auto"]=it.auto_mode; o["bf"]=it.backfire; o["out"]=it.out; o["why"]=it.reason;
  }
  serializeJson(d, out); return cnt;
}

void LOGR::clear(){ head = 0; }
```

---

## `src/rpm_rmt.h`

```cpp
#pragma once
#include <Arduino.h>

namespace RPM {
  void begin(uint8_t pin);
  void setPPR(float ppr);
  void setScale(float s);
  uint16_t get(); // filtered rpm (0 if timeout)
}
```

## `src/rpm_rmt.cpp`

```cpp
#include "rpm_rmt.h"
#include "pins.h"

// Simple period-based mock (replace with real RMT if needed now).
// For skeleton: measure pulse intervals via interrupt on PIN_RPM_IN.

static volatile uint32_t last_us = 0;
static volatile uint32_t period_us = 0;
static float g_ppr = 1.0f; static float g_scale = 1.0f;

static void IRAM_ATTR isr(){
  uint32_t now = micros();
  uint32_t dt = now - last_us; last_us = now; if (dt>50 && dt<1000000) period_us = dt;
}

void RPM::begin(uint8_t pin){
  pinMode(pin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(pin), isr, RISING);
}

void RPM::setPPR(float ppr){ g_ppr = max(0.1f, ppr); }
void RPM::setScale(float s){ g_scale = (s<=0?1.0f:s); }

uint16_t RPM::get(){
  uint32_t p = period_us; if (p==0) return 0;
  // timeout if too old
  if ((micros() - last_us) > 500000) return 0; // 0.5s
  float rpm = 60.0f * 1e6f / (float(p) * g_ppr);
  rpm *= g_scale;
  // crude clamp
  if (rpm<0) rpm=0; if (rpm>20000) rpm=20000;
  return (uint16_t)rpm;
}
```

---

## `src/trigger_input.h`

```cpp
#pragma once
#include <Arduino.h>
namespace TRIG { void begin(uint8_t pin, uint16_t debounce_ms); bool pressed(); }
```

## `src/trigger_input.cpp`

```cpp
#include "trigger_input.h"
static uint8_t gpin; static uint16_t gdeb; static uint32_t last_ms=0; static bool last=false;
void TRIG::begin(uint8_t pin, uint16_t debounce_ms){ gpin=pin; gdeb=debounce_ms; pinMode(pin, INPUT_PULLUP); }
bool TRIG::pressed(){ bool v = (digitalRead(gpin)==LOW); uint32_t now=millis(); if (v!=last){ last=v; last_ms=now; }
  if (v && (now-last_ms)>=gdeb) return true; return false; }
```

---

## `src/cut_output.h`

```cpp
#pragma once
#include <Arduino.h>

enum class CutLine { IGN=0, INJ=1 };

namespace CUT {
  void begin(uint8_t pinIgn, uint8_t pinInj);
  void set(CutLine line, bool cutting); // true = open (cut), false = closed (run)
}
```

## `src/cut_output.cpp`

```cpp
#include "cut_output.h"
#include "pins.h"
static uint8_t pIgn, pInj;
void CUT::begin(uint8_t pinIgn, uint8_t pinInj){ pIgn=pinIgn; pInj=pinInj; pinMode(pIgn, OUTPUT); pinMode(pInj, OUTPUT); digitalWrite(pIgn, LOW); digitalWrite(pInj, LOW); }
void CUT::set(CutLine line, bool cutting){ uint8_t p = (line==CutLine::IGN? pIgn : pInj); digitalWrite(p, cutting? HIGH:LOW); }
```

---

## `src/pwm_test.h`

```cpp
#pragma once
#include <Arduino.h>
namespace PWMTEST { void begin(uint8_t pin); void enable(bool en); void setSim(float rpm, float ppr); }
```

## `src/pwm_test.cpp`

```cpp
#include "pwm_test.h"
static uint8_t gpin; static bool gen=false; static float grpm=0, gppr=1;
void PWMTEST::begin(uint8_t pin){ gpin=pin; pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
void PWMTEST::enable(bool en){ gen=en; }
void PWMTEST::setSim(float rpm, float ppr){ grpm=rpm; gppr=max(0.1f,ppr); }
// crude software generator (tick from loop)
static uint32_t lastToggle=0; static bool lvl=false;
static inline uint32_t periodUs(){ if(!gen || grpm<=0) return 0; float hz = (grpm * gppr) / 60.0f; if(hz<1) hz=1; if(hz>2000) hz=2000; return (uint32_t)(1e6f/hz); }
void pwmTestTick(){ uint32_t p=periodUs(); if(p==0) return; uint32_t now=micros(); if(now-lastToggle>= (p/2)){ lastToggle=now; lvl=!lvl; digitalWrite(gpin,lvl); } }
```

---

## `src/control_sm.h`

```cpp
#pragma once
#include <Arduino.h>
#include "config.h"

enum class State { IDLE=0, ARMED, CUT, RECOVER };
namespace CTRL {
  void begin();
  void tick(); // call in loop
}
```

## `src/control_sm.cpp`

```cpp
#include "control_sm.h"
#include "config_store.h"
#include "rpm_rmt.h"
#include "trigger_input.h"
#include "cut_output.h"
#include "log_ring.h"
#include "pins.h"

static State st = State::IDLE; static uint32_t tEntry=0; static uint16_t lastCut=0; static bool armedEdge=false;

static uint16_t lookupCut(uint16_t rpm, const QSConfig &c){
  // manual
  if (c.mode==Mode::MANUAL) return c.manual_kill_ms;
  // auto (linear between bands)
  for (uint8_t i=0;i<c.map_count;i++){
    auto b=c.map[i];
    if (rpm>=b.rpm_lo && rpm<b.rpm_hi) return b.cut_ms;
  }
  // above last band: use last
  return c.map[c.map_count-1].cut_ms;
}

static void pushLog(uint16_t rpm, uint16_t cut, bool autoMode, bool bf, CutOutputSel sel, const char* why){
  LogItem it{}; it.ts_ms=millis(); it.rpm=rpm; it.cut_ms=cut; it.auto_mode=autoMode; it.backfire=bf; strncpy(it.out,(sel==CutOutputSel::IGN?"IGN":"INJ"),3); strncpy(it.reason, why, 7); LOGR::push(it);
}

void CTRL::begin(){ st=State::IDLE; tEntry=millis(); }

void CTRL::tick(){
  QSConfig cfg = CFG::get();
  // Update RPM helpers
  RPM::setPPR(cfg.ppr); RPM::setScale(cfg.rpm_scale);
  const uint16_t rpm = RPM::get();

  // software tick for PWM test generator
  extern void pwmTestTick(); pwmTestTick();

  switch(st){
    case State::IDLE:
      if (TRIG::pressed()) { st=State::ARMED; tEntry=millis(); armedEdge=true; }
      break;

    case State::ARMED: {
      bool ok = (rpm >= cfg.rpm_min);
      if (!ok) { st=State::IDLE; break; }
      // proceed to CUT
      st=State::CUT; tEntry=millis();
      uint16_t cut = lookupCut(rpm, cfg);
      bool useIgn = (cfg.cut_output==CutOutputSel::IGN);
      bool bf = false;
      if (cfg.backfire_enabled && rpm >= cfg.backfire_min_rpm){
        bf = true; useIgn = true; // force IGN to keep fuel flowing
        cut = min<uint16_t>(CUT_MS_MAX, (uint16_t)(cut + cfg.backfire_extra_ms));
      }
      cut = constrain(cut, CUT_MS_MIN, CUT_MS_MAX);
      // Do cut
      CUT::set(useIgn? CutLine::IGN : CutLine::INJ, true); // open relay (cut)
      delay(cut);
      CUT::set(useIgn? CutLine::IGN : CutLine::INJ, false); // release
      lastCut = cut;
      pushLog(rpm, cut, cfg.mode==Mode::AUTO, bf, cfg.cut_output, "shift");
      st=State::RECOVER; tEntry=millis();
    } break;

    case State::RECOVER:
      if ((millis()-tEntry) >= CFG::get().holdoff_ms) { st=State::IDLE; }
      break;
  }
}
```

---

## `src/web_ui.h`

```cpp
#pragma once
namespace WEB {
  void beginPortal(); // starts AP + web, handles auto-timeout per config
  void loop();        // call in loop
}
```

## `src/web_ui.cpp`

```cpp
#include "web_ui.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "config_store.h"
#include "log_ring.h"

static AsyncWebServer server(80);
static DNSServer dns;
static uint32_t lastHit=0; static bool running=false;

static void handleAPI(){
  server.on("/api/get", HTTP_GET, [](AsyncWebServerRequest *req){
    String js; CFG::exportJSON(js); req->send(200, "application/json", js);
  });
  server.on("/api/set", HTTP_POST, [](AsyncWebServerRequest *req){}, NULL, [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t, size_t){
    String body((char*)data, len); bool ok = CFG::importJSON(body); req->send(ok?200:400, "text/plain", ok?"OK":"BAD");
  });
  server.on("/api/log", HTTP_GET, [](AsyncWebServerRequest *req){ String js; LOGR::readAllToJson(js); req->send(200, "application/json", js); });
  server.on("/api/clearlog", HTTP_POST, [](AsyncWebServerRequest *req){ LOGR::clear(); req->send(200, "text/plain", "OK"); });
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req){
    // Minimal inline UI (could be replaced by SPIFFS in data/)
    String h = F("<!doctype html><html><head><meta name=viewport content='width=device-width,initial-scale=1'>"
                 "<style>body{font-family:sans-serif;margin:16px}pre{white-space:pre-wrap}</style></head><body>"
                 "<h2>ESP32-C3 Quickshifter</h2>"
                 "<button onclick=load()>Reload</button> <button onclick=save()>Save</button> <button onclick=dl()>Export</button>"
                 "<input type=file id=f accept=.json onchange=imp(event)><hr><pre id=o>Loading...</pre>"
                 "<script>let cfg={};async function load(){let r=await fetch('/api/get');cfg=await r.json();document.getElementById('o').textContent=JSON.stringify(cfg,null,2);}"
                 "async function save(){let r=await fetch('/api/set',{method:'POST',body:JSON.stringify(cfg)});alert(await r.text());load()}"
                 "async function dl(){let s=JSON.stringify(cfg,null,2);let a=document.createElement('a');a.href=URL.createObjectURL(new Blob([s],{type:'application/json'}));a.download='qs_config.json';a.click()}"
                 "async function imp(e){let f=e.target.files[0];let t=await f.text();let r=await fetch('/api/set',{method:'POST',body:t});alert(await r.text());load()}"
                 "load();</script></body></html>");
    req->send(200, "text/html", h);
  });
}

void WEB::beginPortal(){
  if (running) return; running=true; lastHit=millis();
  auto cfg = CFG::get();
  WiFi.mode(WIFI_AP);
  String ssid = String("QS-ESP32C3-") + String((uint32_t)ESP.getEfuseMac(), HEX).substring(4);
  WiFi.softAP(ssid.c_str(), "12345678");
  dns.start(53, "*", WiFi.softAPIP());
  handleAPI();
  server.onRequestBody([](AsyncWebServerRequest* req, uint8_t*, size_t, size_t, size_t){ lastHit=millis(); });
  server.onNotFound([](AsyncWebServerRequest* req){ lastHit=millis(); req->redirect("/"); });
  server.begin();
}

void WEB::loop(){
  if (!running) return;
  dns.processNextRequest();
  uint16_t tout = CFG::get().ap_timeout_s;
  if (tout>0 && (millis()-lastHit) > (uint32_t)tout*1000UL){
    // stop AP
    server.end(); dns.stop(); WiFi.softAPdisconnect(true); running=false;
  }
}
```

---

## `src/main.cpp`

```cpp
#include <Arduino.h>
#include "pins.h"
#include "config_store.h"
#include "log_ring.h"
#include "rpm_rmt.h"
#include "trigger_input.h"
#include "cut_output.h"
#include "control_sm.h"
#include "web_ui.h"
#include "pwm_test.h"

void setup(){
  Serial.begin(115200); delay(200);
  pinMode(PIN_STATUS_LED, OUTPUT); digitalWrite(PIN_STATUS_LED, LOW);
  CFG::begin(); LOGR::begin();
  RPM::begin(PIN_RPM_IN);
  TRIG::begin(PIN_SHIFT_NPN, CFG::get().debounce_shift_ms);
  CUT::begin(PIN_CUT_IGN, PIN_CUT_INJ);
  PWMTEST::begin(PIN_PWM_TEST);
  CTRL::begin();
  WEB::beginPortal(); // AP at boot; auto-timeout per config
}

void loop(){
  CTRL::tick();
  WEB::loop();
  // simple heartbeat
  static uint32_t t0=0; if (millis()-t0>500){ t0=millis(); digitalWrite(PIN_STATUS_LED, !digitalRead(PIN_STATUS_LED)); }
}
```

---

## (Optional) `data/index.html`

> Hiện skeleton dùng HTML inline. Nếu muốn tách file UI đẹp hơn, tạo `data/index.html` và nạp qua AsyncWebServer + SPIFFS/LittleFS.

```html
<!doctype html>
<html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<style>body{font-family:sans-serif;margin:16px}label{display:block;margin:8px 0}</style></head>
<body>
<h2>ESP32-C3 Quickshifter UI</h2>
<p>Đây là file UI tuỳ chọn. Có thể nâng cấp thành form chỉnh Mode, PPR (0.5/1/2), Map, Backfire, Calibrate, Test RPM, Log viewer, v.v…</p>
</body></html>
```

---

# 🧪 Ghi chú chạy thử

1. `pio run -t upload` → mở Serial Monitor 115200.
2. Kết nối AP `QS-ESP32C3-XXXX` (pass `12345678`).
3. Vào `http://192.168.4.1/` → xem/Export/Import JSON (UI tối giản). Bạn có thể mở rộng UI tại `data/index.html`.
4. Chân mặc định ở `pins.h` — đổi theo phần cứng của bạn.

# ⚠️ Lưu ý an toàn

- Relay NC phải có diode cuộn hút; đường cắt IGN/INJ đi dây to; cách ly nguồn, lọc nhiễu.
- Backfire OFF mặc định; bật khi test ở tua > `backfire_min_rpm` và hiểu rủi ro với pô/catalyst.

