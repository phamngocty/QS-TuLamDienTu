#pragma once
#include <Arduino.h>

// ===== Defaults & Limits =====
// Modes
enum class Mode : uint8_t { MANUAL = 0, AUTO = 1 };
enum class RpmSource : uint8_t { COIL = 0, INJECTOR = 1 };
enum class CutOutputSel : uint8_t { IGN = 0, INJ = 1 };

struct AutoBand { uint16_t rpm_lo; uint16_t rpm_hi; uint16_t cut_ms; };
struct BackfireCfg {
  bool     enabled        = false;   // đã có
  uint16_t rpm_min        = 5000;    // đã có
  uint16_t extra_ms       = 15;      // đã có
  bool     force_ign      = true;    // MỚI: ép cắt IGN khi backfire
  uint8_t  skip_sparks    = 0;       // MỚI: số tia lửa bỏ qua sau cắt (0..2)
};

struct QSConfig {
  Mode mode = Mode::AUTO;
  RpmSource rpm_source = RpmSource::COIL;
  float ppr = 1.0f;                 // 0.5 / 1 / 2 selectable
  uint16_t rpm_min = 2500;          // Below this no cut
  uint16_t manual_kill_ms = 65;     // Manual cut time (fallback)
  uint16_t debounce_shift_ms = 25;  // Shift sensor debounce
  uint16_t holdoff_ms = 180;        // Lockout after cut
  CutOutputSel cut_output = CutOutputSel::IGN; // default output
  // Backfire (relay-simple): OFF by default per user request
  bool backfire_enabled = false;
  uint16_t backfire_extra_ms = 15;  // Extra cut when backfire enabled
  uint16_t backfire_min_rpm = 4500; // Only above this rpm
  
  // Calibration
  float rpm_scale = 1.0f;           // rpm_display = rpm_raw * rpm_scale
  // Auto map (up to 6 bands typical) - Preset cho Winner X 150
  AutoBand map[5] = {
    {3000, 5000, 80},   // Tua thấp: cut dài hơn
    {5001, 7000, 70},   // Tua trung bình
    {7001, 9000, 62},   // Tua cao
    {9001, 11000, 55},  // Tua rất cao
    {11001, 12500, 50}  // Redline
  };
  uint8_t map_count = 5;
    // ===== Vehicle Lock =====
  bool     lock_enabled    = false;
  CutOutputSel lock_cut_sel = CutOutputSel::IGN; // cắt IGN khi đang khóa (hoặc INJ)
  char     lock_code[9]    = "1001"; // chuỗi 0/1, tối đa 8 ký tự + null
  uint16_t lock_short_ms_max = 300;  // < 300ms = nhấn "ngắn" (bit 0)
  uint16_t lock_long_ms_min  = 600;  // >= 600ms = nhấn "dài" (bit 1)
  uint16_t lock_gap_ms       = 400;  // khoảng nghỉ tối đa giữa hai nhịp
  uint16_t lock_timeout_s    = 30;   // thời gian cho phép nhập, hết giờ -> giữ khóa
  uint8_t  lock_max_retries  = 5;    // số lần sai tối đa, vượt -> giữ khóa (có thể cần tắt mở lại)

  // ==== Wi-Fi AP Configuration ====
  char     ap_ssid[32]     = "";       // SSID tối đa 31 ký tự + null (sẽ được set theo MAC)
  char     ap_pass[64]     = "12345678"; // Password 8-63 ký tự (WPA2-PSK)
  uint16_t ap_timeout_s    = 120;      // 0 = never auto close

  // ==== Auto Cut Configuration ====
  uint16_t auto_cut_min    = 20;       // min cut time in auto mode
  uint16_t auto_cut_max    = 150;      // max cut time in auto mode

  // ==== Lock Status (runtime) ====
  bool vehicle_locked = false;          // Trạng thái khóa hiện tại
  bool has_password = false;            // Đã đặt mật khẩu hay chưa

};

// Limits / safety
static constexpr uint16_t CUT_MS_MAX = 150; // hard cap
static constexpr uint16_t CUT_MS_MIN = 20;
