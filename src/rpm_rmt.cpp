#include "rpm_rmt.h"
#include "pins.h"

// Simple period-based mock (replace with real RMT if needed now).
// For skeleton: measure pulse intervals via interrupt on PIN_RPM_IN.

static volatile uint32_t last_us = 0;
static volatile uint32_t period_us = 0;
static float g_ppr = 1.0f; static float g_scale = 1.0f;

static void IRAM_ATTR isr(){
  uint32_t now = micros();
  uint32_t dt = now - last_us; last_us = now; if (dt>50 && dt<1000000) period_us = dt;
}

void RPM::begin(uint8_t pin){
  pinMode(pin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(pin), isr, RISING);
}

void RPM::setPPR(float ppr){ g_ppr = max(0.1f, ppr); }
void RPM::setScale(float s){ g_scale = (s<=0?1.0f:s); }

uint16_t RPM::get(){
  uint32_t p = period_us; if (p==0) return 0;
  // timeout if too old
  if ((micros() - last_us) > 500000) return 0; // 0.5s
  float rpm = 60.0f * 1e6f / (float(p) * g_ppr);
  rpm *= g_scale;
  // crude clamp
  if (rpm<0) rpm=0; if (rpm>20000) rpm=20000;
  return (uint16_t)rpm;
}
// thêm ở cuối file
uint16_t RPM_get(){ return RPM::get(); }