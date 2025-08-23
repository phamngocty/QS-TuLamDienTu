#include "pwm_test.h"

static uint8_t gpin;
static bool gen = false;
static float grpm = 0, gppr = 1;
static uint32_t lastToggle = 0;
static bool lvl = false;

static inline uint32_t periodUs(){
  if(!gen || grpm <= 0) return 0;
  float hz = (grpm * gppr) / 60.0f;
  if(hz < 1) hz = 1;
  if(hz > 2000) hz = 2000;
  return (uint32_t)(1000000.0f / hz);
}

void PWMTEST::begin(uint8_t pin){
  gpin = pin;
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
}

void PWMTEST::enable(bool en){ gen = en; }

void PWMTEST::setSim(float rpm, float ppr){
  grpm = rpm;
  gppr = (ppr <= 0 ? 1.0f : ppr);
}

void PWMTEST::tick(){                  // <- KHÔNG dùng void pwmTestTick()
  uint32_t p = periodUs();
  if(p == 0) return;
  uint32_t now = micros();
  if(now - lastToggle >= (p / 2)){
    lastToggle = now;
    lvl = !lvl;
    digitalWrite(gpin, lvl);
  }
}
