#include "cut_output.h"
#include "pins.h"

// ---- state ----
static uint8_t pIgn, pInj;
static bool s_ign=false, s_inj=false;
static bool s_pulsing=false;
static uint32_t s_pulse_end=0;
static CutLine s_pulse_line=CutLine::IGN;

// ---- impl ----
void CUT::begin(uint8_t pinIgn, uint8_t pinInj){
  pIgn=pinIgn; pInj=pinInj;
  pinMode(pIgn, OUTPUT); pinMode(pInj, OUTPUT);
  digitalWrite(pIgn, LOW); digitalWrite(pInj, LOW);
  s_ign = s_inj = s_pulsing = false;
}

void CUT::set(CutLine line, bool cutting){
  const uint8_t p = (line==CutLine::IGN? pIgn : pInj);
  digitalWrite(p, cutting? HIGH:LOW);
  if (line==CutLine::IGN) s_ign = cutting; else s_inj = cutting;
}

bool CUT::isActive(){ return s_ign || s_inj; }

// Pulse không chặn (non-blocking), gọi CUT::tick() trong loop
void CUT::pulse(CutLine line, uint16_t ms){
  CUT::set(line, true);
  s_pulsing = true; s_pulse_line = line;
  s_pulse_end = millis() + (uint32_t)ms;
}

void CUT::tick(){
  if (s_pulsing && (millis() >= s_pulse_end)){
    CUT::set(s_pulse_line, false);
    s_pulsing = false;
  }
}

// Test blocking (chỉ dùng cho /api/testcut)
void CUT_testPulse(bool useIgn, uint16_t ms){
  CUT::set(useIgn? CutLine::IGN : CutLine::INJ, true);
  delay(ms);
  CUT::set(useIgn? CutLine::IGN : CutLine::INJ, false);
}
