
#pragma once
#include <Arduino.h>

namespace RPM {
  void begin(uint8_t pin);
  void setPPR(float ppr);
  void setScale(float s);
  uint16_t get(); // filtered rpm (0 if timeout)
}
#pragma once
