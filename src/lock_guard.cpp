#include "lock_guard.h"
#include "config_store.h"
#include "cut_output.h"
#include "trigger_input.h"
#include <Arduino.h>

namespace LOCK {
  enum class Stage { IDLE, PRESSING, GAP };
  
  static bool locked = false;
  static bool unlocked_pulse = false;
  static Stage st = Stage::IDLE;
  static String seq = "";
  static uint32_t t_press_start = 0;
  static uint8_t  retries = 0;
  static uint32_t t_start_window = 0;

  QSConfig cfg() { return CFG::get(); }

  void begin() {
    reset();
    
    // Chỉ kích hoạt lock nếu được bật trong config
    const auto c = CFG::get();
    if (c.lock_enabled) {
      locked = true;
      applyCutWhileLocked();
      Serial.println("[LOCK] Lock enabled at boot - system LOCKED");
    } else {
      locked = false;
      Serial.println("[LOCK] Lock disabled at boot - system UNLOCKED");
    }
  }

  void reset() {
    st = Stage::IDLE;
    seq = "";
    t_press_start = 0;
    t_start_window = millis();
    retries = 0;
  }

  bool isLocked() { return locked; }
  bool justUnlocked() { 
    if (unlocked_pulse) {
      unlocked_pulse = false;
      return true;
    }
    return false;
  }

  void forceLock() {
    locked = true;
    unlocked_pulse = false;
    reset();
    applyCutWhileLocked();
  }

  void applyCutWhileLocked() {
    if (!locked) return;
    
    auto c = cfg();
    switch (c.lock_cut_sel) {
      case CutOutputSel::IGN:  CUT::set(CutLine::IGN, true); break;
      case CutOutputSel::INJ:  CUT::set(CutLine::INJ, true); break;
      default: break;
    }
  }

  void releaseCut() {
    CUT::set(CutLine::IGN, false);
    CUT::set(CutLine::INJ, false);
  }

  bool adminUnlock(const String& pass) {
    if (strcmp(pass.c_str(), cfg().lock_code) == 0) {
      locked = false;
      unlocked_pulse = true;
      releaseCut();
      reset();
      
      // Đảm bảo thoát pass-mode ngay lập tức
      // Không đọc NPN cho pass nữa
      st = Stage::IDLE;
      seq = "";
      t_press_start = 0;
      retries = 0;
      
      Serial.println("[LOCK] Admin unlock successful - pass mode disabled, system unlocked");
      return true;
    }
    return false;
  }

  bool disableLock() {
    auto c = CFG::get();
    if (c.lock_enabled) {
      // Cập nhật config
      QSConfig newCfg = c;
      newCfg.lock_enabled = false;
      CFG::set(newCfg);
      
      // Nhả cắt và reset state NGAY LẬP TỨC
      locked = false;
      unlocked_pulse = false;
      releaseCut(); // Nhả cắt IGN/INJ = LOW ngay
      
      // Reset toàn bộ state machine
      reset();
      
      Serial.println("[LOCK] Lock system disabled - cuts released immediately");
      return true;
    }
    return false;
  }

  bool enableLock() {
    auto c = CFG::get();
    if (!c.lock_enabled) {
      // Cập nhật config
      QSConfig newCfg = c;
      newCfg.lock_enabled = true;
      CFG::set(newCfg);
      
      // KHÔNG auto-lock, chỉ bật chế độ pass
      locked = false;
      unlocked_pulse = false;
      releaseCut(); // Đảm bảo không cắt
      
      // Reset state machine
      reset();
      
      Serial.println("[LOCK] Lock system enabled - pass mode ON, not locked");
      return true;
    }
    return false;
  }

  void disableNoOutputChange() {
    // Tắt pass-mode + lock, KHÔNG đổi trạng thái cắt hiện tại
    locked = false;
    unlocked_pulse = false;
    
    // Reset state machine
    reset();
    
    Serial.println("[LOCK] Lock disabled (no output change)");
  }

  void tick() {
    auto c = cfg();
    
    // EARLY RETURN: nếu lock không được bật, KHÔNG can thiệp gì
    if (!c.lock_enabled) { 
      // Khi không enabled, đảm bảo cắt được nhả
      if (locked) {
        locked = false;
        releaseCut();
      }
      return; 
    }
    
    // Nếu không locked, không đọc NPN cho pass
    if (!locked) { 
      return; 
    }

    // Timeout & retry limit
    if (c.lock_timeout_s > 0 && (millis() - t_start_window) > (uint32_t)c.lock_timeout_s * 1000UL) {
      // Hết thời gian -> vẫn locked, giữ cut
      return; // Không cần gọi applyCutWhileLocked() vì đã có ở cuối
    }
    if (c.lock_max_retries > 0 && retries >= c.lock_max_retries) {
      return; // Không cần gọi applyCutWhileLocked() vì đã có ở cuối
    }

    // Đọc cảm biến nhịp (sử dụng API trong trigger_input)
    // SHIFT_NPN active-low: LOW = đang nhấn, HIGH = không nhấn
    // SỬ DỤNG rawLevel() để không bị debounce - dành riêng cho Lock
    bool pressed = TRIG::rawLevel(); // true = đang nhấn (LOW)

    uint32_t now = millis();

    switch (st) {
      case Stage::IDLE:
        if (pressed) {
          st = Stage::PRESSING;
          t_press_start = now;
        }
        break;

      case Stage::PRESSING:
        if (!pressed) {
          // Kết thúc nhấn
          uint32_t duration = now - t_press_start;
          
          if (duration <= c.lock_short_ms_max) {
            seq += "0"; // Short pulse = 0
          } else if (duration >= c.lock_long_ms_min) {
            seq += "1"; // Long pulse = 1
          }
          // Ignore pulses that are too short or too long
          
          st = Stage::GAP;
          t_press_start = now;
        }
        break;

      case Stage::GAP:
        if (pressed) {
          // Tiếp tục input
          st = Stage::PRESSING;
          t_press_start = now;
        } else if ((now - t_press_start) >= c.lock_gap_ms) {
          // Hết gap -> kiểm tra sequence
          if (seq.length() > 0) {
            if (strcmp(seq.c_str(), c.lock_code) == 0) {
              // Unlock thành công
              locked = false;
              unlocked_pulse = true;
              releaseCut();
              reset();
              
              // Đảm bảo thoát pass-mode ngay lập tức
              st = Stage::IDLE;
              seq = "";
              t_press_start = 0;
              retries = 0;
              
              Serial.println("[LOCK] Unlock successful, system unlocked");
            } else {
              retries++;
              seq = "";
              st = Stage::IDLE;
              t_press_start = 0;
              Serial.printf("[LOCK] Unlock failed, retries: %d/%d\n", retries, cfg().lock_max_retries);
            }
          }
        }
        break;
    }

    // Khi đang khóa, đảm bảo cắt
    if (locked) {
      applyCutWhileLocked();
    }
  }
}