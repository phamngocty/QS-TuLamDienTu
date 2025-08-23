#pragma once
#include <Arduino.h>

class BackfireController {
public:
  enum ModeFlags : uint8_t {
    BF_SHIFT   = 0x01,   // bắn ngay sau khi QS nhả cut
    BF_OVERRUN = 0x02    // bắn khi dRPM/dt âm lớn (đóng ga nhanh)
  };

  // Config lồng trong class -> tránh đụng tên với code cũ của bạn
  struct Config {
    bool     enabled                = true;      // bật/tắt backfire
    bool     ign_only               = true;      // chỉ cho phép khi đang chọn IGN
    uint8_t  mode                   = BF_SHIFT | BF_OVERRUN; // bitmask

    uint16_t rpm_min                = 4000;      // chỉ kích trong khoảng
    uint16_t rpm_max                = 9000;
    uint16_t warmup_s               = 120;       // trễ khởi động (s)

    uint16_t decel_thresh_rpm_s     = 3000;      // dRPM/dt <= -ngưỡng thì kích OVERRUN
    uint16_t window_after_shift_ms  = 250;       // cửa sổ sau khi QS nhả

    uint8_t  burst_count            = 3;         // số nhịp trong 1 chuỗi
    uint16_t burst_on_ms            = 25;        // thời gian cắt mỗi nhịp
    uint16_t burst_off_ms           = 75;        // thời gian nhả giữa hai nhịp
    uint16_t refractory_ms          = 1500;      // khoá chống spam giữa 2 chuỗi
  };

  // Callbacks bạn gắn vào:
  using GetRpmFn        = uint16_t (*)();          // trả RPM tức thời
  using IsCutBusyFn     = bool     (*)();          // đang có cut nào đang chạy?
  using RequestIgnCutFn = void     (*)(uint16_t);  // yêu cầu cắt IGN ms
  using IsIgnModeFn     = bool     (*)();          // output hiện tại là IGN?

  void begin(const Config& cfg,
             GetRpmFn getRpm,
             IsCutBusyFn isBusy,
             RequestIgnCutFn requestIgnCut,
             IsIgnModeFn isIgnMode)
  {
    _cfg = cfg;
    _getRpm        = getRpm;
    _isBusy        = isBusy;
    _requestIgnCut = requestIgnCut;
    _isIgnMode     = isIgnMode;

    _lastRpm = 0;
    _lastTs  = millis();
    _startedAt = _lastTs;

    _active = false;
    _burstsLeft = 0;
    _nextPulseAt = _patternEnd = _lastFireAt = _shiftWindowUntil = 0;
    _drpm_per_s = 0;
  }

  // có thể đổi config khi đang chạy
  void setConfig(const Config& cfg) { _cfg = cfg; }

  // gọi một lần sau begin để đánh dấu thời điểm bắt đầu chạy
  void markStarted(uint32_t now_ms) { _startedAt = now_ms; }

  // gọi NGAY khi quickshift vừa NHẢ cắt
  void onShiftCutCompleted(uint32_t now_ms) {
    _shiftWindowUntil = now_ms + _cfg.window_after_shift_ms;
  }

  // tick ~ mỗi 1–5ms
  void tick(uint32_t now_ms) {
    if (!_cfg.enabled) return;
    if (!_getRpm || !_isBusy || !_requestIgnCut || !_isIgnMode) return;
    if (_cfg.ign_only && !_isIgnMode()) return;
    if ((now_ms - _startedAt) < (uint32_t)_cfg.warmup_s * 1000UL) return;

    // cập nhật dRPM/dt (đủ 20ms mới cập nhật để bớt nhiễu)
    const uint16_t rpm = _getRpm();
    const uint32_t dt  = now_ms - _lastTs;
    if (dt >= 20) {
      const int32_t drpm = (int32_t)rpm - (int32_t)_lastRpm;
      _drpm_per_s = (int32_t)((int64_t)drpm * 1000 / (int32_t)dt);
      _lastRpm = rpm;
      _lastTs  = now_ms;
    }

    // kết thúc chuỗi hiện tại?
    if (_active && now_ms >= _patternEnd) {
      _active = false;
    }

    // rảnh → xét trigger mới
    if (!_active && !_isBusy() && (now_ms - _lastFireAt) >= _cfg.refractory_ms) {
      const bool rpm_ok     = (rpm >= _cfg.rpm_min && rpm <= _cfg.rpm_max);
      const bool shift_ok   = ((_cfg.mode & BF_SHIFT)   && (now_ms <= _shiftWindowUntil));
      const bool overrun_ok = ((_cfg.mode & BF_OVERRUN) && (_drpm_per_s <= -(int32_t)_cfg.decel_thresh_rpm_s));

      if (rpm_ok && (shift_ok || overrun_ok)) {
        startPattern(now_ms);
      }
    }

    // đang active → bắn nhịp nếu đến lượt & hệ đang không bận
    if (_active && !_isBusy() && (now_ms >= _nextPulseAt) && _burstsLeft) {
      _requestIgnCut(_cfg.burst_on_ms); // ON
      _nextPulseAt = now_ms + (uint32_t)_cfg.burst_on_ms + (uint32_t)_cfg.burst_off_ms; // OFF
      if (--_burstsLeft == 0) {
        _patternEnd = _nextPulseAt; // kết thúc sau OFF cuối
      }
    }
  }

private:
  void startPattern(uint32_t now_ms) {
    // clamp an toàn cho rơ-le cơ
    uint16_t on_ms  = _cfg.burst_on_ms;  if (on_ms  < 20) on_ms  = 20;
    uint16_t off_ms = _cfg.burst_off_ms; if (off_ms < 40) off_ms = 40;

    _active      = true;
    _burstsLeft  = _cfg.burst_count ? _cfg.burst_count : 1;
    _nextPulseAt = now_ms;  // bắn ngay nhịp đầu
    _patternEnd  = now_ms + (uint32_t)(on_ms + off_ms) * (uint32_t)_burstsLeft;
    _lastFireAt  = now_ms;
    _shiftWindowUntil = 0;  // dùng 1 lần sau SHIFT
  }

  // ===== data =====
  Config _cfg{};

  GetRpmFn        _getRpm        = nullptr;
  IsCutBusyFn     _isBusy        = nullptr;
  RequestIgnCutFn _requestIgnCut = nullptr;
  IsIgnModeFn     _isIgnMode     = nullptr;

  uint16_t _lastRpm = 0;
  uint32_t _lastTs  = 0;
  int32_t  _drpm_per_s = 0;

  bool     _active = false;
  uint8_t  _burstsLeft = 0;
  uint32_t _nextPulseAt = 0;
  uint32_t _patternEnd  = 0;
  uint32_t _lastFireAt  = 0;
  uint32_t _shiftWindowUntil = 0;
  uint32_t _startedAt = 0;
};
