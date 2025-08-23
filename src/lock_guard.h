#pragma once
#include <Arduino.h>
#include "config.h"

namespace LOCK {
  void begin();
  void reset();                 // xóa buffer đang nhập
  void tick();                  // gọi mỗi vòng lặp, đọc SHIFT_NPN
  bool isLocked();              // true nếu đang khóa
  bool justUnlocked();          // true duy nhất 1 lần ngay sau khi mở
  void forceLock();             // ép về trạng thái khóa (nếu cần)
  void applyCutWhileLocked();   // áp dụng cut theo lock_cut_sel
  bool adminUnlock(const String& pass); // so pass với CFG::get().lock_code, mở khóa
  bool disableLock();                   // tắt lock system (không cần pass)
  bool enableLock();                    // bật lock system (không auto-lock)
  void disableNoOutputChange();         // tắt pass-mode + lock, KHÔNG đổi trạng thái cắt
}
