// web_ui.cpp
#include "web_ui.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "config_store.h"
#include "log_ring.h"
#include "pwm_test.h"
#include "lock_guard.h"

#include <Arduino.h>
#include "FS.h"
#include "LittleFS.h"
#include "ota_update.h"

// ...


// ===================== Serial log helpers =====================
static bool serOn = true; // có thể thêm /api/serial để bật/tắt nếu muốn
#define SLOGln(x)  do{ if(serOn) Serial.println(x); }while(0)
#define SLOGf(...) do{ if(serOn) Serial.printf(__VA_ARGS__); }while(0)

// ===================== Globals =====================
static AsyncWebServer server(80);
static DNSServer dns;
static uint32_t lastHit   = 0;
static bool     running   = false; // portal is running
static bool     holdPortal= false; // keep AP on while UI is open

// ===================== FS debug – danh sách file =====================
static void listFS() {
  SLOGln("[FS] Listing LittleFS:");
  File root = LittleFS.open("/");
  if (!root) { SLOGln("  (cannot open /)"); return; }
  File f = root.openNextFile();
  while (f) {
    SLOGf("  - %s (%u bytes)\n", f.name(), (unsigned)f.size());
    f = root.openNextFile();
  }
  SLOGf("[FS] total=%u used=%u\n",
        (unsigned)LittleFS.totalBytes(), (unsigned)LittleFS.usedBytes());
}
// Helper: lấy param từ query trước, nếu không có thì lấy từ POST body
auto getParam = [&](AsyncWebServerRequest* req, const char* key, const char* def = nullptr) -> String {
  if (req->hasParam(key))                    return req->getParam(key)->value();         // query string
  if (req->hasParam(key, true))              return req->getParam(key, true)->value();   // POST body
  return def ? String(def) : String();
};

// ===================== REST API =====================
static void handleAPI() {
  // --------- Config get/set ----------
  server.on("/api/get", HTTP_GET, [](AsyncWebServerRequest* req) {
    SLOGln("[API] GET /api/get");
    String js; CFG::exportJSON(js, false);
    AsyncWebServerResponse *response = req->beginResponse(200, "application/json", js);
    response->addHeader("Cache-Control", "no-store, no-cache, must-revalidate");
    req->send(response);
    lastHit = millis();
  });

  server.on("/api/set", HTTP_POST, [](AsyncWebServerRequest* req) {
    SLOGln("[API] POST /api/set");
  }, NULL, [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
    String body((char*)data, len);
    bool ok = CFG::importJSON(body);
    SLOGf("[API] /api/set → %s\n", ok ? "OK" : "BAD");
    req->send(ok ? 200 : 400, "text/plain", ok ? "OK" : "BAD");
    lastHit = millis();
  });

  // --------- Logs ----------
  server.on("/api/log", HTTP_GET, [](AsyncWebServerRequest* req) {
    SLOGln("[API] GET /api/log");
    String js; LOGR::readAllToJson(js);
    req->send(200, "application/json", js);
    lastHit = millis();
  });

  server.on("/api/clearlog", HTTP_POST, [](AsyncWebServerRequest* req) {
    SLOGln("[API] POST /api/clearlog");
    LOGR::clear();
    req->send(200, "text/plain", "OK");
    lastHit = millis();
  });

  // --------- Test output (cut 50ms) ----------
  server.on("/api/testcut", HTTP_POST, [](AsyncWebServerRequest* req) {
  String out = getParam(req, "out");
  SLOGf("[API] POST /api/testcut out=%s\n", out.c_str());
  if (out != "ign" && out != "inj") { req->send(400, "text/plain", "out? ign|inj"); return; }
  extern void CUT_testPulse(bool useIgn, uint16_t ms);
  CUT_testPulse(out == "ign", 50);
  req->send(200, "text/plain", "OK");
  lastHit = millis();
});


  // --------- Test RPM ----------
 // /api/testrpm?en=0|1&rpm=5000&ppr=1

  // --------- Config API aliases (không phá route cũ) ----------
  server.on("/api/config/get", HTTP_GET, [](AsyncWebServerRequest* req) {
    SLOGln("[API] GET /api/config/get (alias)");
    String js; CFG::exportJSON(js, false);
    AsyncWebServerResponse *response = req->beginResponse(200, "application/json", js);
    response->addHeader("Cache-Control", "no-store, no-cache, must-revalidate");
    req->send(response);
    lastHit = millis();
  });

  server.on("/api/config/set", HTTP_POST, [](AsyncWebServerRequest* req) {
    SLOGln("[API] POST /api/config/set (alias)");
  }, NULL, [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
    String body((char*)data, len);
    bool ok = CFG::importJSON(body);
    SLOGf("[API] /api/config/set → %s\n", ok ? "OK" : "BAD");
    req->send(ok ? 200 : 400, "text/plain", ok ? "OK" : "BAD");
    lastHit = millis();
  });

  server.on("/api/json/export", HTTP_GET, [](AsyncWebServerRequest* req) {
    SLOGln("[API] GET /api/json/export (alias)");
    
    // Kiểm tra include_secret parameter
    bool includeSecret = req->getParam("include_secret", true) ? 
                        req->getParam("include_secret", true)->value().toInt() : false;
    
    String js; 
    CFG::exportJSON(js, includeSecret);
    AsyncWebServerResponse *response = req->beginResponse(200, "application/json", js);
    response->addHeader("Cache-Control", "no-store, no-cache, must-revalidate");
    req->send(response);
    lastHit = millis();
  });

  server.on("/api/json/import", HTTP_POST, [](AsyncWebServerRequest* req) {
    SLOGln("[API] POST /api/json/import (alias)");
  }, NULL, [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
    String body((char*)data, len);
    bool ok = CFG::importJSON(body);
    SLOGf("[API] /api/json/import → %s\n", ok ? "OK" : "BAD");
    req->send(ok ? 200 : 400, "text/plain", ok ? "OK" : "BAD");
    lastHit = millis();
  });

  // --------- Logs với pagination và verbose ----------
  server.on("/api/logs", HTTP_GET, [](AsyncWebServerRequest* req) {
    SLOGln("[API] GET /api/logs");
    
    // Parse query parameters
    int offset = req->getParam("offset", true) ? req->getParam("offset", true)->value().toInt() : 0;
    int limit = req->getParam("limit", true) ? req->getParam("limit", true)->value().toInt() : 100;
    bool verbose = req->getParam("verbose", true) ? req->getParam("verbose", true)->value().toInt() : 0;
    
    // Clamp limit
    if (limit > 200) limit = 200;
    if (limit < 1) limit = 1;
    
        String js; 
    LOGR::readAllToJson(js);
    
    // Tạo response với next_offset
    JsonDocument doc;
    doc["items"] = js;
    doc["next_offset"] = -1;
    
    String response;
    serializeJson(doc, response);
    
    AsyncWebServerResponse *resp = req->beginResponse(200, "application/json", response);
    resp->addHeader("Cache-Control", "no-store, no-cache, must-revalidate");
    req->send(resp);
    lastHit = millis();
  });

  // --------- Wi-Fi AP Status & Control ----------
  server.on("/api/wifi/status", HTTP_GET, [](AsyncWebServerRequest* req) {
    SLOGln("[API] GET /api/wifi/status");
    
    const auto c = CFG::get();
    JsonDocument doc;
    doc["running"] = running;
    doc["ssid"] = c.ap_ssid;
    doc["password"] = "***"; // Không trả password
    doc["timeout_s"] = c.ap_timeout_s;
    doc["hold"] = holdPortal;
    doc["last_hit"] = lastHit;
    doc["uptime_ms"] = millis();
    
    String out;
    serializeJson(doc, out);
    
    AsyncWebServerResponse *response = req->beginResponse(200, "application/json", out);
    response->addHeader("Cache-Control", "no-store, no-cache, must-revalidate");
    req->send(response);
    lastHit = millis();
  });

  server.on("/api/wifi/config", HTTP_POST, [](AsyncWebServerRequest* req) {
    SLOGln("[API] POST /api/wifi/config");
  }, nullptr, [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
    String body((char*)data, len);
    JsonDocument d;
    DeserializationError e = deserializeJson(d, body);
    
    if (e) {
      SLOGln("[API] /api/wifi/config - BAD JSON");
      req->send(400, "application/json", "{\"ok\":false,\"msg\":\"Invalid JSON\"}");
      return;
    }
    
    // Chỉ cho phép đổi password và timeout, không cho đổi SSID
    if (d.containsKey("ssid")) {
      SLOGln("[WEB] /api/wifi/config: SSID change attempted, ignoring");
      // Bỏ qua để tương thích
    }
    
    String newPass = d["pass"] | "";
    uint16_t newTimeout = d["timeout_s"] | 120;
    
    // Validation password
    if (newPass.length() > 0 && (newPass.length() < 8 || newPass.length() > 63)) {
      req->send(400, "application/json", "{\"ok\":false,\"msg\":\"Password must be 8-63 characters or empty for open AP\"}");
      return;
    }
    
    // Cập nhật config (giữ nguyên SSID)
    QSConfig cfg = CFG::get();
    
    if (newPass.length() > 0) {
      strncpy(cfg.ap_pass, newPass.c_str(), sizeof(cfg.ap_pass) - 1);
      cfg.ap_pass[sizeof(cfg.ap_pass) - 1] = '\0';
    } else {
      // AP mở (không có password)
      cfg.ap_pass[0] = '\0';
    }
    
    cfg.ap_timeout_s = newTimeout;
    CFG::set(cfg);
    
    // Restart AP với cấu hình mới (SSID giữ nguyên)
    if (running) {
      WiFi.softAPdisconnect(true);
      if (newPass.length() > 0) {
        WiFi.softAP(cfg.ap_ssid, cfg.ap_pass);
      } else {
        WiFi.softAP(cfg.ap_ssid); // AP mở
      }
      SLOGf("[WEB] AP restarted with SSID: %s (unchanged), pass updated\n", cfg.ap_ssid);
    }
    
    req->send(200, "application/json", "{\"ok\":true,\"msg\":\"Wi-Fi password updated\"}");
    lastHit = millis();
  });
server.on("/api/testrpm", HTTP_POST, [](AsyncWebServerRequest* req) {
  int   en  = getParam(req, "en",  "0").toInt();
  float rpm = getParam(req, "rpm", "0").toFloat();
  float ppr = getParam(req, "ppr", "1").toFloat();
  SLOGf("[API] POST /api/testrpm en=%d rpm=%.1f ppr=%.2f\n", en, rpm, ppr);
  PWMTEST::enable(en != 0);
  PWMTEST::setSim(rpm, ppr <= 0 ? 1 : ppr);
  req->send(200, "text/plain", "OK test r");
  lastHit = millis();
});

  // --------- Calibrate RPM ----------
  // /api/calib?true_rpm=XXXX
  server.on("/api/calib", HTTP_POST, [](AsyncWebServerRequest* req) {
    auto p = req->getParam("true_rpm", true);
    SLOGf("[API] POST /api/calib true_rpm=%s\n", p ? p->value().c_str() : "?");
    if (!p) { req->send(400, "text/plain", "true_rpm?"); return; }
    uint16_t true_rpm = p->value().toInt();
    uint32_t t0 = millis(), n = 0, sum = 0;
    while (millis() - t0 < 1000) { extern uint16_t RPM_get(); sum += RPM_get(); n++; delay(5); }
    float meas = (n ? (float)sum / n : 1.0f);
    auto cfg = CFG::get();
    cfg.rpm_scale = (meas > 0 ? (float)true_rpm / meas : 1.0f);
    CFG::set(cfg);
    req->send(200, "text/plain", "OK");
    lastHit = millis();
  });
  

  // --------- Wi-Fi hold / off ----------
  server.on("/api/wifi_hold", HTTP_POST, [](AsyncWebServerRequest* req) {
    int on = req->getParam("on", true) ? req->getParam("on", true)->value().toInt() : 1;
    SLOGf("[API] POST /api/wifi_hold on=%d\n", on);
    holdPortal = (on != 0);
    lastHit = millis();
    req->send(200, "text/plain", holdPortal ? "HOLD" : "RELEASE");
  });

  server.on("/api/wifi_off", HTTP_POST, [](AsyncWebServerRequest* req) {
    SLOGln("[API] POST /api/wifi_off");
    req->send(200, "text/plain", "OFF");
    SLOGln("[WEB] Wi-Fi AP OFF by user");
    server.end(); dns.stop(); WiFi.softAPdisconnect(true);
    running = false; holdPortal = false;
  });

  // --------- FS LIST (JSON) ----------
  server.on("/api/fslist", HTTP_GET, [](AsyncWebServerRequest* req){
    String js = "[";
    File root = LittleFS.open("/");
    if (!root) { req->send(500, "application/json", "[]"); return; }
    File f = root.openNextFile();
    bool first = true;
    while (f) {
      if (!first) js += ",";
      first = false;
      js += "{\"name\":\"" + String(f.name()) + "\",\"size\":" + String((unsigned)f.size()) + "}";
      f = root.openNextFile();
    }
    js += "]";
    req->send(200, "application/json", js);
    lastHit = millis();
  });

  // --------- Root UI (LittleFS + fallback) ----------
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req){
    SLOGf("[WEB] GET / from %s\n", req->client()->remoteIP().toString().c_str());
    if (!LittleFS.exists("/index.html")) {
      const char* fb =
        "<!doctype html><meta charset=utf-8>"
        "<style>body{font-family:system-ui;padding:16px}pre{background:#eee;padding:8px;border-radius:8px}</style>"
        "<h3>index.html chưa có trên LittleFS</h3>"
        "<p>Vào VSCode → <b>PlatformIO: Upload File System Image</b></p>"
        "<pre>pio run --target uploadfs</pre>"
        "<p>Hoặc đặt file vào thư mục <code>data/index.html</code> rồi uploadfs.</p>";
      req->send(200, "text/html", fb);
      return;
    }
    req->send(LittleFS, "/index.html", "text/html");
    lastHit = millis(); holdPortal = true;
  });

  // (Tuỳ chọn) phục vụ thêm file tĩnh khác nếu bạn có (css/js…)
  server.serveStatic("/", LittleFS, "/");

  // /api/rpm  → trả rpm hiện tại (JSON)
server.on("/api/rpm", HTTP_GET, [](AsyncWebServerRequest* req){
  extern uint16_t RPM_get();          // đã dùng trong /api/calib
  float rpm_raw = (float)RPM_get();
  auto cfg = CFG::get();
  float rpm = rpm_raw * (cfg.rpm_scale > 0 ? cfg.rpm_scale : 1.0f);

  // (tuỳ chọn) clamp để tránh giá trị lố
  if (rpm < 0) rpm = 0;

  // trả JSON rất nhẹ để poll nhanh
  String js = String("{\"rpm\":") + String((int)rpm) + "}";
  req->send(200, "application/json", js);
  lastHit = millis();
});
// === LOCK API ===
server.on("/api/lock_state", HTTP_GET, [](AsyncWebServerRequest* req) {
  const auto c = CFG::get();
  JsonDocument doc;
  doc["locked"]        = LOCK::isLocked();
  doc["lock_enabled"]  = c.lock_enabled;
  doc["lock_cut_sel"]  = (uint8_t)c.lock_cut_sel;
  doc["lock_code_len"] = (uint8_t)strlen(c.lock_code);
  String out; serializeJson(doc, out);
  req->send(200, "application/json", out);
  lastHit = millis();
});

// POST /api/lock_cmd  body: {"cmd":"lock"} | {"cmd":"unlock","pass":"0101"} | {"cmd":"enable"} | {"cmd":"disable"}
server.on("/api/lock_cmd", HTTP_POST, [](AsyncWebServerRequest* req){ 
  SLOGln("[API] POST /api/lock_cmd"); 
}, nullptr, [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t){
  String body((char*)data, len);
  JsonDocument d;
  DeserializationError e = deserializeJson(d, body);
  if (e) { 
    SLOGln("[API] /api/lock_cmd - BAD JSON");
    req->send(400, "application/json", "{\"ok\":false,\"msg\":\"Invalid JSON\"}"); 
    return; 
  }
  
  String cmd = d["cmd"] | "";
  SLOGf("[API] /api/lock_cmd - cmd: %s\n", cmd.c_str());
  
  if (cmd == "lock") {
    LOCK::forceLock();
    SLOGln("[API] /api/lock_cmd - Lock forced");
    req->send(200, "application/json", "{\"ok\":true,\"msg\":\"Locked\"}");
  } else if (cmd == "unlock") {
    String pass = d["pass"] | "";
    SLOGf("[API] /api/lock_cmd - Unlock attempt with pass: %s\n", pass.c_str());
    bool ok = LOCK::adminUnlock(pass);
    if (ok) {
      SLOGln("[API] /api/lock_cmd - Unlock successful");
      req->send(200, "application/json", "{\"ok\":true,\"msg\":\"Unlocked\"}");
    } else {
      SLOGln("[API] /api/lock_cmd - Unlock failed - wrong password");
      req->send(403, "application/json", "{\"ok\":false,\"msg\":\"Wrong password\"}");
    }
  } else if (cmd == "disable") {
    SLOGln("[API] /api/lock_cmd - Disable lock requested");
    
    // Tắt lock_enabled trong config
    auto c = CFG::get();
    c.lock_enabled = false;
    CFG::set(c);
    
    // Tắt lock system (KHÔNG đổi trạng thái cắt)
    LOCK::disableNoOutputChange();
    
    SLOGln("[API] /api/lock_cmd - Lock disabled via API (no output change)");
    req->send(200, "application/json", "{\"ok\":true,\"msg\":\"lock disabled (no output change)\"}");
  } else if (cmd == "enable") {
    SLOGln("[API] /api/lock_cmd - Enable lock requested");
    bool ok = LOCK::enableLock();
    if (ok) {
      SLOGln("[API] /api/lock_cmd - Lock enabled successfully");
      req->send(200, "application/json", "{\"ok\":true,\"msg\":\"Lock enabled (pass mode ON)\"}");
    } else {
      SLOGln("[API] /api/lock_cmd - Lock enable failed");
      req->send(400, "application/json", "{\"ok\":false,\"msg\":\"Lock enable failed\"}");
    }
  } else {
    SLOGf("[API] /api/lock_cmd - Invalid command: %s\n", cmd.c_str());
    req->send(400, "application/json", "{\"ok\":false,\"msg\":\"Invalid command\"}");
  }
  lastHit = millis();
});


}

// ===================== Portal lifecycle =====================
void WEB::beginPortal() {
  if (running) return;
  running = true; lastHit = millis(); holdPortal = false;

  // Mount LittleFS
  if (!LittleFS.begin(true)) { // true = format if mount failed (cân nhắc đổi false nếu không muốn format)
    SLOGln("[FS] LittleFS mount FAILED");
  } else {
    SLOGln("[FS] LittleFS mounted");
    listFS(); // in danh sách file để chắc chắn có /index.html
  }

  WiFi.mode(WIFI_AP);
  const auto c = CFG::get();
  
  // Tạo SSID mặc định theo MAC (giữ công thức của user)
  String defaultSsid = String("QS-TuLamDienTu-") + String((uint32_t)ESP.getEfuseMac(), HEX).substring(4);
  
  // Sanitize SSID: cắt về ≤ 31 ký tự, đệm '0' nếu cần
  if (defaultSsid.length() > 31) {
    defaultSsid = defaultSsid.substring(0, 31);
  }
  
  // Sử dụng SSID từ config nếu có, không thì dùng default
  String ssidToUse = (c.ap_ssid[0] != '\0') ? String(c.ap_ssid) : defaultSsid;
  
  // Sử dụng SSID & Password từ config
  if (c.ap_pass[0] != '\0') {
    WiFi.softAP(ssidToUse.c_str(), c.ap_pass);
  } else {
    WiFi.softAP(ssidToUse.c_str()); // AP mở
  }
  
  SLOGln("=== WEB Portal start ===");
  SLOGf("[AP] SSID: %s (config: %s, default: %s)\n", ssidToUse.c_str(), c.ap_ssid, defaultSsid.c_str());
  SLOGf("[AP] IP  : %s\n", WiFi.softAPIP().toString().c_str());

  dns.start(53, "*", WiFi.softAPIP());
  handleAPI();
  server.onNotFound([](AsyncWebServerRequest* req) { lastHit = millis(); req->redirect("/"); });
  server.begin();
}

void WEB::loop() {
  if (!running) return;
  dns.processNextRequest();
  if (holdPortal) return; // Giữ AP khi người dùng đang mở UI

  uint16_t tout = CFG::get().ap_timeout_s; // timeout cấu hình trong Web
  if (tout > 0 && (millis() - lastHit) > (uint32_t)tout * 1000UL) {
    server.end(); dns.stop(); WiFi.softAPdisconnect(true);
    running = false;
    SLOGln("[WEB] AP timeout → stop portal");
  }
}
