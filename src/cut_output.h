#pragma once
#include <Arduino.h>

enum class CutLine { IGN=0, INJ=1 };

namespace CUT {
  void begin(uint8_t pinIgn, uint8_t pinInj);
  void set(CutLine line, bool cutting); // true = open (cut), false = closed (run)
  bool isActive();                         // đang có line nào bị cắt?
void pulse(CutLine line, uint16_t ms);   // cắt không chặn trong ms
void tick();                             // gọi mỗi vòng loop để nhả đúng hẹn

}
