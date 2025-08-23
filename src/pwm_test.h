#pragma once
#include <Arduino.h>
namespace PWMTEST {
  void begin(uint8_t pin);
  void enable(bool en);
  void setSim(float rpm, float ppr);
  void tick();            // <-- THÊM DÒNG NÀY
}

