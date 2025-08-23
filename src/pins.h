#pragma once
#include <Arduino.h>

// ===== Centralized GPIO mapping (change here) =====
// RMT-capable pin recommended for RPM_IN if possible on your C3 module
static constexpr uint8_t PIN_RPM_IN      = 2;  // RMT input from coil/injector via opto/schmitt
static constexpr uint8_t PIN_SHIFT_NPN   = 4;  // NPN proximity sensor (active LOW via pull-up/opto)
static constexpr uint8_t PIN_CUT_IGN     = 6;  // Relay NC for ignition cut (driver)
static constexpr uint8_t PIN_CUT_INJ     = 7;  // Relay NC for injector cut (driver)
static constexpr uint8_t PIN_PWM_TEST    = 8;  // PWM out to simulate RPM
static constexpr uint8_t PIN_STATUS_LED  = 3;  // Optional LED
