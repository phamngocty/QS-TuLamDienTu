#include "config_store.h"
#include <Preferences.h>
#include <ArduinoJson.h>

namespace CFG {
  Preferences prefs;
  QSConfig g_cfg;

  void begin() {
    prefs.begin("qs", false);
    
    // Load existing config
    g_cfg.lock_enabled = prefs.getBool("lock_en", true);
    g_cfg.lock_code[0] = '\0';
    prefs.getString("lock_code", g_cfg.lock_code, sizeof(g_cfg.lock_code));
    g_cfg.lock_cut_sel = (CutOutputSel)prefs.getUChar("lock_cut", 0);
    g_cfg.lock_short_ms_max = prefs.getUShort("lock_short", 300);
    g_cfg.lock_long_ms_min = prefs.getUShort("lock_long", 600);
    g_cfg.lock_gap_ms = prefs.getUShort("lock_gap", 400);
    g_cfg.lock_timeout_s = prefs.getUShort("lock_timeout", 30);
    g_cfg.lock_max_retries = prefs.getUChar("lock_retries", 5);
    
    g_cfg.auto_cut_min = prefs.getUShort("auto_min", 20);
    g_cfg.auto_cut_max = prefs.getUShort("auto_max", 150);
    g_cfg.ppr = prefs.getFloat("ppr", 1.0f);
    g_cfg.rpm_source = (RpmSource)prefs.getUChar("rpm_src", 0);
    g_cfg.rpm_min = prefs.getUShort("rpm_min", 1000);
    g_cfg.manual_kill_ms = prefs.getUShort("mkill", 50);
    g_cfg.debounce_shift_ms = prefs.getUShort("deb", 20);
    g_cfg.holdoff_ms = prefs.getUShort("hold", 200);
    g_cfg.cut_output = (CutOutputSel)prefs.getUChar("cut_out", 0);
    
    g_cfg.backfire_enabled = prefs.getBool("bf_en", false);
    g_cfg.bf_burst_on = prefs.getUShort("bf_on", 25);
    g_cfg.bf_burst_off = prefs.getUShort("bf_off", 75);
    g_cfg.bf_refractory_ms = prefs.getUShort("bf_ref", 1500);
    
    // Load Auto Map
    int map_count = prefs.getInt("map_count", 0);
    g_cfg.map_count = (map_count > 4) ? 4 : map_count;
    for (int i = 0; i < g_cfg.map_count; i++) {
      String key = "map_" + String(i);
      g_cfg.map[i].rpm_lo = prefs.getUShort((key + "_lo").c_str(), 0);
      g_cfg.map[i].rpm_hi = prefs.getUShort((key + "_hi").c_str(), 0);
      g_cfg.map[i].cut_ms = prefs.getUShort((key + "_t").c_str(), 50);
    }
    
    // Load Wi-Fi AP settings
    g_cfg.ap_ssid[0] = '\0';
    prefs.getString("ap_ssid", g_cfg.ap_ssid, sizeof(g_cfg.ap_ssid));
    g_cfg.ap_pass[0] = '\0';
    prefs.getString("ap_pass", g_cfg.ap_pass, sizeof(g_cfg.ap_pass));
    g_cfg.ap_timeout_s = prefs.getUShort("ap_timeout", 120);
    
    // Tạo SSID mặc định theo MAC nếu chưa có
    if (g_cfg.ap_ssid[0] == '\0') {
      String defaultSsid = String("QS-TuLamDienTu-") + String((uint32_t)ESP.getEfuseMac(), HEX).substring(4);
      strncpy(g_cfg.ap_ssid, defaultSsid.c_str(), sizeof(g_cfg.ap_ssid) - 1);
      g_cfg.ap_ssid[sizeof(g_cfg.ap_ssid) - 1] = '\0';
      // Lưu SSID mặc định vào prefs
      prefs.putString("ap_ssid", g_cfg.ap_ssid);
    }
  }

  void set(const QSConfig& cfg) {
    g_cfg = cfg;
    
    prefs.putBool("lock_en", cfg.lock_enabled);
    prefs.putString("lock_code", cfg.lock_code);
    prefs.putUChar("lock_cut", (uint8_t)cfg.lock_cut_sel);
    prefs.putUShort("lock_short", cfg.lock_short_ms_max);
    prefs.putUShort("lock_long", cfg.lock_long_ms_min);
    prefs.putUShort("lock_gap", cfg.lock_gap_ms);
    prefs.putUShort("lock_timeout", cfg.lock_timeout_s);
    prefs.putUChar("lock_retries", cfg.lock_max_retries);
    
    prefs.putUShort("auto_min", cfg.auto_cut_min);
    prefs.putUShort("auto_max", cfg.auto_cut_max);
    prefs.putFloat("ppr", cfg.ppr);
    prefs.putUChar("rpm_src", (uint8_t)cfg.rpm_source);
    prefs.putUShort("rpm_min", cfg.rpm_min);
    prefs.putUShort("mkill", cfg.manual_kill_ms);
    prefs.putUShort("deb", cfg.debounce_shift_ms);
    prefs.putUShort("hold", cfg.holdoff_ms);
    prefs.putUChar("cut_out", (uint8_t)cfg.cut_output);
    
    prefs.putBool("bf_en", cfg.backfire_enabled);
    prefs.putUShort("bf_on", cfg.bf_burst_on);
    prefs.putUShort("bf_off", cfg.bf_burst_off);
    prefs.putUShort("bf_ref", cfg.bf_refractory_ms);
    
    // Save Auto Map
    prefs.putInt("map_count", cfg.map_count);
    for (int i = 0; i < cfg.map_count; i++) {
      String key = "map_" + String(i);
      prefs.putUShort((key + "_lo").c_str(), cfg.map[i].rpm_lo);
      prefs.putUShort((key + "_hi").c_str(), cfg.map[i].rpm_hi);
      prefs.putUShort((key + "_t").c_str(), cfg.map[i].cut_ms);
    }
    
    // Save Wi-Fi AP settings
    prefs.putString("ap_ssid", cfg.ap_ssid);
    prefs.putString("ap_pass", cfg.ap_pass);
    prefs.putUShort("ap_timeout", cfg.ap_timeout_s);
  }

  QSConfig get() {
    return g_cfg;
  }

  bool exportJSON(String &out, bool includeSecret) {
    JsonDocument d;
    
    d["lock_enabled"]        = g_cfg.lock_enabled;
    d["lock_code"]           = includeSecret ? g_cfg.lock_code : "***"; // Chỉ trả password khi includeSecret = true
    d["lock_cut_sel"]        = (uint8_t)g_cfg.lock_cut_sel;
    d["lock_short_ms_max"]   = g_cfg.lock_short_ms_max;
    d["lock_long_ms_min"]    = g_cfg.lock_long_ms_min;
    d["lock_gap_ms"]         = g_cfg.lock_gap_ms;
    d["lock_timeout_s"]      = g_cfg.lock_timeout_s;
    d["lock_max_retries"]    = g_cfg.lock_max_retries;
    
    d["auto_cut_min"]        = g_cfg.auto_cut_min;
    d["auto_cut_max"]        = g_cfg.auto_cut_max;
    d["ppr"]                 = g_cfg.ppr;
    d["rpm_source"]          = (uint8_t)g_cfg.rpm_source;
    d["rpm_min"]             = g_cfg.rpm_min;
    d["manual_kill_ms"]      = g_cfg.manual_kill_ms;
    d["debounce_shift_ms"]   = g_cfg.debounce_shift_ms;
    d["holdoff_ms"]          = g_cfg.holdoff_ms;
    d["cut_output"]          = (uint8_t)g_cfg.cut_output;
    
    d["backfire_enabled"]    = g_cfg.backfire_enabled;
    d["bf_burst_on"]         = g_cfg.bf_burst_on;
    d["bf_burst_off"]        = g_cfg.bf_burst_off;
    d["bf_refractory_ms"]    = g_cfg.bf_refractory_ms;
    
    // Auto Map
    JsonArray mapArray = d["map"].to<JsonArray>();
    for (int i = 0; i < g_cfg.map_count; i++) {
      JsonObject mapItem = mapArray.add<JsonObject>();
      mapItem["lo"] = g_cfg.map[i].rpm_lo;
      mapItem["hi"] = g_cfg.map[i].rpm_hi;
      mapItem["t"] = g_cfg.map[i].cut_ms;
    }
    
    // Wi-Fi AP settings
    d["ap_ssid"]             = g_cfg.ap_ssid;
    d["ap_pass"]             = includeSecret ? g_cfg.ap_pass : "***"; // Chỉ trả password khi includeSecret = true
    d["ap_timeout_s"]        = g_cfg.ap_timeout_s;
    
    return serializeJson(d, out) > 0;
  }

  bool importJSON(const String& json) {
    JsonDocument d;
    DeserializationError e = deserializeJson(d, json);
    if (e) return false;
    
    QSConfig c = g_cfg; // Copy current config
    
    if (d["lock_enabled"].is<bool>()) c.lock_enabled = d["lock_enabled"];
    if (d["lock_code"].is<String>()) {
      String code = d["lock_code"].as<String>();
      // Sanitize: chỉ cho phép 0/1, tối đa 8 ký tự
      String sanitized = "";
      for (unsigned int i = 0; i < code.length() && i < 8; i++) {
        char ch = code.charAt(i);
        if (ch == '0' || ch == '1') {
          sanitized += ch;
        }
      }
      strncpy(c.lock_code, sanitized.c_str(), sizeof(c.lock_code) - 1);
      c.lock_code[sizeof(c.lock_code) - 1] = '\0';
    }
    if (d["lock_cut_sel"].is<uint8_t>()) c.lock_cut_sel = (CutOutputSel)d["lock_cut_sel"].as<uint8_t>();
    if (d["lock_short_ms_max"].is<uint16_t>()) c.lock_short_ms_max = d["lock_short_ms_max"];
    if (d["lock_long_ms_min"].is<uint16_t>()) c.lock_long_ms_min = d["lock_long_ms_min"];
    if (d["lock_gap_ms"].is<uint16_t>()) c.lock_gap_ms = d["lock_gap_ms"];
    if (d["lock_timeout_s"].is<uint16_t>()) c.lock_timeout_s = d["lock_timeout_s"];
    if (d["lock_max_retries"].is<uint8_t>()) c.lock_max_retries = d["lock_max_retries"];
    
    if (d["auto_cut_min"].is<uint16_t>()) c.auto_cut_min = d["auto_cut_min"];
    if (d["auto_cut_max"].is<uint16_t>()) c.auto_cut_max = d["auto_cut_max"];
    if (d["ppr"].is<float>()) c.ppr = d["ppr"];
    if (d["rpm_source"].is<uint8_t>()) c.rpm_source = (RpmSource)d["rpm_source"].as<uint8_t>();
    if (d["rpm_min"].is<uint16_t>()) c.rpm_min = d["rpm_min"];
    if (d["manual_kill_ms"].is<uint16_t>()) c.manual_kill_ms = d["manual_kill_ms"];
    if (d["debounce_shift_ms"].is<uint16_t>()) c.debounce_shift_ms = d["debounce_shift_ms"];
    if (d["holdoff_ms"].is<uint16_t>()) c.holdoff_ms = d["holdoff_ms"];
    if (d["cut_output"].is<uint8_t>()) c.cut_output = (CutOutputSel)d["cut_output"].as<uint8_t>();
    
    if (d["backfire_enabled"].is<bool>()) c.backfire_enabled = d["backfire_enabled"];
    if (d["bf_burst_on"].is<uint16_t>()) c.bf_burst_on = d["bf_burst_on"];
    if (d["bf_burst_off"].is<uint16_t>()) c.bf_burst_off = d["bf_burst_off"];
    if (d["bf_refractory_ms"].is<uint16_t>()) c.bf_refractory_ms = d["bf_refractory_ms"];
    
    // Auto Map
    if (d["map"].is<JsonArray>()) {
      JsonArray mapArray = d["map"];
      c.map_count = 0;
      int index = 0;
      for (JsonObject mapItem : mapArray) {
        if (index >= 4) break; // Max 4 items
        if (mapItem["lo"].is<uint16_t>() && mapItem["hi"].is<uint16_t>() && mapItem["t"].is<uint16_t>()) {
          c.map[index].rpm_lo = mapItem["lo"];
          c.map[index].rpm_hi = mapItem["hi"];
          c.map[index].cut_ms = mapItem["t"];
          index++;
          c.map_count++;
        }
      }
    }
    
    // Wi-Fi AP settings
    if (d["ap_ssid"].is<String>()) {
      String ssid = d["ap_ssid"].as<String>();
      // Sanitize: cắt về ≤ 31 ký tự, loại bỏ ký tự điều khiển
      ssid = ssid.substring(0, min(ssid.length(), (unsigned int)31));
      for (unsigned int i = 0; i < ssid.length(); i++) {
        if (ssid.charAt(i) < 32) {
          ssid.setCharAt(i, '_');
        }
      }
      strncpy(c.ap_ssid, ssid.c_str(), sizeof(c.ap_ssid) - 1);
      c.ap_ssid[sizeof(c.ap_ssid) - 1] = '\0';
    }
    if (d["ap_pass"].is<String>()) {
      String pass = d["ap_pass"].as<String>();
      // Validation: 8-63 ký tự hoặc rỗng
      if (pass.length() == 0 || (pass.length() >= 8 && pass.length() <= 63)) {
        strncpy(c.ap_pass, pass.c_str(), sizeof(c.ap_pass) - 1);
        c.ap_pass[sizeof(c.ap_pass) - 1] = '\0';
      }
      // Nếu không hợp lệ, giữ password cũ
    }
    if (d["ap_timeout_s"].is<uint16_t>()) c.ap_timeout_s = d["ap_timeout_s"];
    
    set(c);
    return true;
  }
}
