#pragma once
#include <Arduino.h>
namespace TRIG { void begin(uint8_t pin, uint16_t debounce_ms); bool pressed();   void tick();
  bool fell();   // cạnh
  bool rose();   // cạnh
  bool rawLevel(); // <-- thêm}
}

