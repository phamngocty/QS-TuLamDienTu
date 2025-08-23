#pragma once
#include <Arduino.h>
#include "config.h"

enum class State { IDLE=0, ARMED, CUT, RECOVER };
namespace CTRL {
  void begin();
  void tick(); // call in loop
}
