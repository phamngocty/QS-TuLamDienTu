#include "ota_update.h"
#include <Update.h>
#include <LittleFS.h>

static uint32_t s_reboot_delay_ms = 1200;

void OTAHTTP_setRebootDelayMs(uint32_t ms){ s_reboot_delay_ms = ms; }

// Gửi JSON ngắn gọn
static void sendJSON(AsyncWebServerRequest* req, int code, const String& msg){
  String out = "{\"ok\":"; out += (code==200?"true":"false");
  out += ",\"msg\":\""; out += msg; out += "\"}";
  req->send(code, "application/json", out);
}

// Trang OTA tối giản (phòng khi bạn chưa sửa index.html)
static const char* OTA_PAGE = R"HTML(
<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>OTA</title>
<style>body{font-family:sans-serif;margin:16px} fieldset{margin:12px 0} input[type=file]{width:100%}</style></head>
<body>
<h2>OTA Update</h2>
<fieldset><legend>Firmware (.bin)</legend>
<form id="f_fw" method="POST" action="/api/ota/firmware" enctype="multipart/form-data">
<input type="file" name="update" required><br><br><button type="submit">Upload Firmware</button>
</form></fieldset>
<fieldset><legend>Filesystem image LittleFS (.bin)</legend>
<form id="f_fs" method="POST" action="/api/ota/fsimage" enctype="multipart/form-data">
<input type="file" name="update" required><br><br><button type="submit">Upload FS Image</button>
</form></fieldset>
<fieldset><legend>Upload 1 file (ví dụ /index.html)</legend>
<form id="f_one" method="POST" action="/api/upload?path=/index.html" enctype="multipart/form-data">
<input type="file" name="file" required><br><br><button type="submit">Upload /index.html</button>
</form></fieldset>
<script>
// giữ AP không tắt trong khi đang mở trang
fetch('/api/wifi_hold?on=1').catch(()=>{});
window.addEventListener('beforeunload',()=>{navigator.sendBeacon('/api/wifi_hold?on=0')});
</script>
</body></html>
)HTML";

void OTAHTTP_registerRoutes(AsyncWebServer& server){
  // Trang OTA đơn giản
  server.on("/ota", HTTP_GET, [](AsyncWebServerRequest* req){
    req->send(200, "text/html", OTA_PAGE);
  });

  // ===== OTA FIRMWARE (.bin) =====
  server.on("/api/ota/firmware", HTTP_POST,
    // onRequest
    [](AsyncWebServerRequest* req){
      // Kết thúc: nếu không lỗi → reboot
      if (Update.hasError()){
        sendJSON(req, 500, "FW update failed");
      } else {
        sendJSON(req, 200, "FW update ok, rebooting...");
        req->client()->close(true);
        delay(s_reboot_delay_ms);
        ESP.restart();
      }
    },
    // onUpload
    [](AsyncWebServerRequest* req, const String& filename, size_t index,
       uint8_t *data, size_t len, bool final){
      if (!index){
        // Bắt đầu update firmware
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)){
          Update.printError(Serial);
        }
      }
      if (len){
        if (Update.write(data, len) != len){
          Update.printError(Serial);
        }
      }
      if (final){
        if (!Update.end(true)){
          Update.printError(Serial);
        }
      }
    });

  // ===== OTA FS IMAGE (LittleFS .bin) =====
  // Build từ PlatformIO: "Build Filesystem Image" -> .pio/build/<env>/littlefs.bin
  server.on("/api/ota/fsimage", HTTP_POST,
    [](AsyncWebServerRequest* req){
      if (Update.hasError()){
        sendJSON(req, 500, "FS image update failed");
      } else {
        sendJSON(req, 200, "FS image ok, rebooting...");
        req->client()->close(true);
        delay(s_reboot_delay_ms);
        ESP.restart();
      }
    },
    [](AsyncWebServerRequest* req, const String& filename, size_t index,
       uint8_t *data, size_t len, bool final){
      if (!index){
        // Với LittleFS trên ESP32, dùng U_SPIFFS cho phân vùng DATA (LittleFS)
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_SPIFFS)){
          Update.printError(Serial);
        }
      }
      if (len){
        if (Update.write(data, len) != len){
          Update.printError(Serial);
        }
      }
      if (final){
        if (!Update.end(true)){
          Update.printError(Serial);
        }
      }
    });

  // ===== Upload 1 file vào LittleFS (ví dụ /index.html) =====
  server.on("/api/upload", HTTP_POST,
    [](AsyncWebServerRequest* req){
      // nếu tới đây mà không có onUpload nghĩa là lỗi
      sendJSON(req, 200, "Upload done");
    },
    [](AsyncWebServerRequest* req, const String& filename, size_t index,
       uint8_t *data, size_t len, bool final){
      // đường dẫn đích qua query ?path=/index.html
      static File f;
      if (!index){
        if (!req->hasParam("path", true)){
          req->send(400, "application/json", "{\"ok\":false,\"msg\":\"missing path\"}");
          return;
        }
        String path = req->getParam("path", true)->value();
        if (!path.startsWith("/")) path = "/" + path;
        // tạo thư mục nếu cần (đơn giản hoá: chỉ cho file gốc)
        if (LittleFS.exists(path)) LittleFS.remove(path);
        f = LittleFS.open(path, "w");
        if (!f){
          req->send(500, "application/json", "{\"ok\":false,\"msg\":\"open fail\"}");
          return;
        }
      }
      if (len && f){
        f.write(data, len);
      }
      if (final && f){
        f.close();
      }
    });
}
