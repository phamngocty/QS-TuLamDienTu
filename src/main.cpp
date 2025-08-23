#include <Arduino.h>
#include "pins.h"
#include "config_store.h"   // để dùng CFG::get()
#include "log_ring.h"
#include "rpm_rmt.h"
#include "trigger_input.h"
#include "cut_output.h"
#include "control_sm.h"
#include "web_ui.h"
#include "pwm_test.h"
#include "lock_guard.h"  // dùng LOCK từ lock_guard.cpp
#include "Backfire.h"

// 1) Tạo instance:
BackfireController backfire;
// Callbacks cho BackfireController
static uint16_t QS_GetRPM()            { return RPM::get(); }
static bool     QS_IsCutBusy()         { return CUT::isActive(); } // đủ để tránh chồng xung
static void     QS_RequestIgnCut(uint16_t ms) { 
  if (LOCK::isLocked()) return;        // khi khóa: chặn mọi cắt
  CUT::pulse(CutLine::IGN, ms);        // không chặn loop
}
static bool     QS_IsIgnMode()         { return CFG::get().cut_output == CutOutputSel::IGN; }

/*

// 2) Viết 4 callback theo code của chính bạn:
static uint16_t QS_GetRPM() {
  // TODO: trả về RPM hiện tại từ module RPM của bạn
  // ví dụ: return rpm_current();
  return 0;
}
static bool QS_IsCutBusy() {
  // TODO: true nếu đang có cut (QS/backfire) đang chạy
  // ví dụ: return cut_is_active();
  return false;
}
static void QS_RequestIgnCut(uint16_t ms) {
  // TODO: yêu cầu CẮT LỬA (IGN) trong ms mili-giây
  // ví dụ: cut_ignition_ms(ms);
}
static bool QS_IsIgnMode() {
  // TODO: true nếu cấu hình hiện tại đang chọn IGN
  // ví dụ: return (config.output_mode == OUTPUT_MODE_IGN);
  return true;
}
*/
void setup(){
  Serial.begin(115200); delay(200);
  Serial.println("=== Quickshifter ESP32-C3 started ===");

  pinMode(PIN_STATUS_LED, OUTPUT); 
  digitalWrite(PIN_STATUS_LED, LOW);

  CFG::begin();
  LOGR::begin();
  RPM::begin(PIN_RPM_IN);
  TRIG::begin(PIN_SHIFT_NPN, 10);
  CUT::begin(PIN_CUT_IGN, PIN_CUT_INJ);
  PWMTEST::begin(PIN_PWM_TEST);
  CTRL::begin();
  WEB::beginPortal();     // AP at boot; tự tắt theo ap_timeout_s
  LOCK::begin();          // bật cơ chế khóa theo config

  
  const auto& c = CFG::get();

BackfireController::Config bf{};
bf.enabled               = (c.bf_enable != 0);
bf.ign_only              = (c.bf_ign_only != 0);
bf.mode                  = (uint8_t)(c.bf_mode & (BackfireController::BF_SHIFT | BackfireController::BF_OVERRUN));
bf.rpm_min               = c.bf_rpm_min;
bf.rpm_max               = c.bf_rpm_max;
bf.warmup_s              = c.bf_warmup_s;
bf.decel_thresh_rpm_s    = c.bf_decel_thresh;
bf.window_after_shift_ms = c.bf_window_ms;
bf.burst_count           = c.bf_burst_count;
bf.burst_on_ms           = c.bf_burst_on;
bf.burst_off_ms          = c.bf_burst_off;
bf.refractory_ms         = c.bf_refractory_ms;

backfire.begin(bf, QS_GetRPM, QS_IsCutBusy, QS_RequestIgnCut, QS_IsIgnMode);

  backfire.markStarted(millis());

}

void loop(){
  // Ưu tiên xử lý khóa
  LOCK::tick();
  if (LOCK::isLocked()){
    WEB::loop(); // vẫn cho cấu hình khi đang khóa
    // heartbeat
    static uint32_t t0=0; 
    if (millis()-t0>500){ 
      t0=millis(); 
      digitalWrite(PIN_STATUS_LED, !digitalRead(PIN_STATUS_LED)); 
    }
    return; // chặn QS khi đang khóa
  }

  if (LOCK::justUnlocked()){
    // (Tùy chọn) mở portal 1 thời gian để tinh chỉnh
    // WEB::beginPortal();
  }

  // QS bình thường
  CTRL::tick();
  WEB::loop();

  // heartbeat
  static uint32_t t0=0; 
  if (millis()-t0>500){ 
    t0=millis(); 
    digitalWrite(PIN_STATUS_LED, !digitalRead(PIN_STATUS_LED)); 

     uint32_t now = millis();
  // ... code khác
  backfire.tick(now);
  }
  CUT::tick();
backfire.tick(millis());



}
// GỌI HÀM NÀY Ở CHỖ VỪA NHẢ QUICKSHIFT CUT
// (ngay sau khi bạn tắt rơ-le QS)
static inline void onQuickshiftCutReleased_hook(uint32_t now) {
  backfire.onShiftCutCompleted(now);
}

