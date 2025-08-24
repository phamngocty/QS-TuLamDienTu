#pragma once
#include <Arduino.h>
#include "config.h"

enum class State { IDLE=0, ARMED, CUT, RECOVER };
namespace CTRL {
  void begin();
  void tick(); // call in loop
  
  // Debug functions
  uint16_t getCurrentRPM();
  const char* getCurrentState();
  uint16_t getLastCutMs();
  uint16_t getHoldoffRemainMs();
  bool canCutNow();
  const char* getCutReason();
}
