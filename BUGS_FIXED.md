# Các lỗi đã sửa trong ESP32-C3 Quickshifter

## Lỗi đã phát hiện và sửa:

### 1. Lỗi khai báo hàm trong pwm_test.h
- **Vấn đề**: Hàm `pwmTestTick()` được gọi trong `control_sm.cpp` nhưng không được khai báo trong header
- **Sửa**: Thêm khai báo `void pwmTestTick();` vào `pwm_test.h`

### 2. Thiếu include cho các hàm C++ standard
- **Vấn đề**: Sử dụng `min()`, `max()` mà không include `<algorithm>`
- **Sửa**: Thêm `#include <algorithm>` vào các file cần thiết

### 3. Thiếu include cho các hàm C standard
- **Vấn đề**: Sử dụng `strncpy()` mà không include `<cstring>`
- **Sửa**: Thêm `#include <cstring>` vào `control_sm.cpp`

### 4. Thiếu include cho snprintf
- **Vấn đề**: Sử dụng `snprintf()` mà không include `<cstdio>`
- **Sửa**: Thêm `#include <cstdio>` vào `config_store.cpp`

### 5. Vấn đề với F() macro
- **Vấn đề**: Macro `F()` có thể không được hỗ trợ đầy đủ trên ESP32-C3
- **Sửa**: Thay thế `F()` bằng `String()` constructor

### 6. Cảnh báo về delay() blocking
- **Vấn đề**: Sử dụng `delay()` trong state machine có thể block toàn bộ hệ thống
- **Sửa**: Thêm comment cảnh báo về việc sử dụng non-blocking timing

## Các vấn đề tiềm ẩn khác cần lưu ý:

1. **PlatformIO không được cài đặt**: Cần cài đặt PlatformIO Core để build dự án
2. **Dependencies**: Đảm bảo các thư viện sau được cài đặt:
   - esp32async/ESPAsyncWebServer@^3.7.10
   - esp32async/ESPAsyncTCP@^2.0.0
   - bblanchon/ArduinoJson@^7.4.2

## Hướng dẫn build:

1. Cài đặt PlatformIO Core:
   ```bash
   pip install platformio
   ```

2. Build dự án:
   ```bash
   platformio run
   ```

3. Upload lên ESP32-C3:
   ```bash
   platformio run --target upload
   ```

## Cấu trúc dự án:
- `main.cpp`: Entry point và setup/loop chính
- `config.h`: Định nghĩa cấu trúc dữ liệu và enum
- `config_store.cpp/h`: Quản lý lưu trữ cấu hình
- `control_sm.cpp/h`: State machine điều khiển quickshifter
- `rpm_rmt.cpp/h`: Đọc RPM qua RMT
- `trigger_input.cpp/h`: Đọc tín hiệu shift
- `cut_output.cpp/h`: Điều khiển relay cut
- `web_ui.cpp/h`: Web interface
- `pwm_test.cpp/h`: PWM test generator
- `log_ring.cpp/h`: Logging system
