#pragma once
#include <Arduino.h>
#include "config.h"

namespace CFG {
  void begin();
  QSConfig get();
  void set(const QSConfig& cfg);
  bool exportJSON(String &out, bool includeSecret = false);
  bool importJSON(const String& json);
}
