#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>

void OTAHTTP_registerRoutes(AsyncWebServer& server);
void OTAHTTP_setRebootDelayMs(uint32_t ms);  // mặc định 1200ms
