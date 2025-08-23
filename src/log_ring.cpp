#include "log_ring.h"
#include <ArduinoJson.h>

// Dùng uint16_t để tránh xung đột với size_t khi dùng min/so sánh
static constexpr uint16_t RING_SZ = 64;
static LogItem ring[RING_SZ];
static volatile uint16_t head = 0; // next write index

void LOGR::begin(){ head = 0; }

void LOGR::push(const LogItem &it){
  // Snapshot biến volatile để tính toán nhất quán và tránh mismatch kiểu
  uint16_t h = head;
  ring[h % RING_SZ] = it;
  head = h + 1;
}

size_t LOGR::readAllToJson(String &out){
  StaticJsonDocument<4096> d; JsonArray a = d.to<JsonArray>();
  // Snapshot head một lần
  uint16_t h = head;
  // Thay min<> bằng toán tử 3 ngôi để tránh lỗi chọn overload
  uint16_t cnt = (h < RING_SZ) ? h : RING_SZ;
  for (uint16_t i = 0; i < cnt; i++){
    const LogItem &it = ring[(uint16_t)((h - cnt + i) % RING_SZ)];
    JsonObject o = a.createNestedObject();
    o["t"]=it.ts_ms; o["rpm"]=it.rpm; o["cut"]=it.cut_ms; o["auto"]=it.auto_mode; o["bf"]=it.backfire; o["out"]=it.out; o["why"]=it.reason;
  }
  serializeJson(d, out);
  return cnt;
}

void LOGR::clear(){ head = 0; }