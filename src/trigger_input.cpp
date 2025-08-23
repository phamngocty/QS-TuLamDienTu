#include "trigger_input.h"
#include "pins.h"
static uint8_t gpin; static uint16_t gdeb; static uint32_t last_ms=0; static bool last=false;
void TRIG::begin(uint8_t pin, uint16_t debounce_ms){ gpin=pin; gdeb=debounce_ms; pinMode(pin, INPUT_PULLUP); }
bool TRIG::pressed(){ bool v = (digitalRead(gpin)==LOW); uint32_t now=millis(); if (v!=last){ last=v; last_ms=now; }
  if (v && (now-last_ms)>=gdeb) return true; return false; }
static uint8_t sPin = 0;
static bool sLevel = false; // true = đang nhấn (tùy cách bạn định nghĩa)
bool TRIG::rawLevel(){ 
  // Đọc trực tiếp từ pin NPN (active-low)
  return digitalRead(gpin) == LOW; // true = đang nhấn (LOW)
}
// Active-low theo code hiện tại
static inline bool shiftPressedRaw() {
  return digitalRead(PIN_SHIFT_NPN) == LOW;
}