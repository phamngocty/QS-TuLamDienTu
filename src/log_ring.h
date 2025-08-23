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
