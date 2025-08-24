#include "ota_update.h"
#include <Update.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <esp_ota_ops.h>
#include "ota_manager.h"

// Serial log helper
#define SLOGln(x)  do{ Serial.println(x); }while(0)

static uint32_t s_reboot_delay_ms = 1200;
static uint32_t s_upload_start_time = 0;
static uint32_t s_upload_total_size = 0;
static uint32_t s_upload_loaded = 0;

void OTAHTTP_setRebootDelayMs(uint32_t ms){ s_reboot_delay_ms = ms; }

// Gửi JSON ngắn gọn
static void sendJSON(AsyncWebServerRequest* req, int code, const String& msg){
  String out = "{\"ok\":"; out += (code==200?"true":"false");
  out += ",\"msg\":\""; out += msg; out += "\"}";
  req->send(code, "application/json", out);
}

// Gửi JSON với progress
static void sendProgressJSON(AsyncWebServerRequest* req, int code, const String& msg, 
                           uint32_t loaded, uint32_t total, uint32_t speed = 0){
  String out = "{\"ok\":"; out += (code==200?"true":"false");
  out += ",\"msg\":\""; out += msg; out += "\"";
  out += ",\"loaded\":"; out += loaded;
  out += ",\"total\":"; out += total;
  if (speed > 0) {
    out += ",\"speed\":"; out += speed;
  }
  out += "}";
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

  // ===== OTA STATE =====
  server.on("/api/ota/state", HTTP_GET, [](AsyncWebServerRequest* req){
    JsonDocument doc;
    doc["app_label"] = esp_ota_get_running_partition()->label;
    doc["pending_validate"] = OTA_MGR::isPendingValidate();
    doc["backup_list"] = OTA_MGR::getBackupList();
    doc["version"] = "1.0.0"; // TODO: Add version info
    
    String response;
    serializeJson(doc, response);
    req->send(200, "application/json", response);
  });

  // ===== OTA MARK VALID =====
  server.on("/api/ota/mark_valid", HTTP_POST, [](AsyncWebServerRequest* req){
    OTA_MGR::markValid();
    sendJSON(req, 200, "Firmware marked as valid");
  });

  // ===== OTA ROLLBACK FIRMWARE =====
  server.on("/api/ota/rollback_fw", HTTP_POST, [](AsyncWebServerRequest* req){
    OTA_MGR::markRollbackAndReboot();
    sendJSON(req, 200, "Rolling back firmware...");
  });

  // ===== OTA BACKUP FS =====
  server.on("/api/ota/backup_fs", HTTP_POST, [](AsyncWebServerRequest* req){
    if (OTA_MGR::createFSBackup()) {
      sendJSON(req, 200, "FS backup created successfully");
    } else {
      sendJSON(req, 500, "FS backup failed");
    }
  });

  // ===== OTA ROLLBACK FS =====
  server.on("/api/ota/rollback_fs", HTTP_POST, [](AsyncWebServerRequest* req){
    if (OTA_MGR::restoreFSBackup()) {
      sendJSON(req, 200, "FS restored, rebooting...");
      req->client()->close(true);
      delay(s_reboot_delay_ms);
      ESP.restart();
    } else {
      sendJSON(req, 500, "FS rollback failed");
    }
  });

  // ===== OTA FIRMWARE (.bin) =====
  server.on("/api/ota/firmware", HTTP_POST,
    // onRequest
    [](AsyncWebServerRequest* req){
      // Kết thúc: nếu không lỗi → reboot
      if (Update.hasError()){
        String errorMsg = "FW update failed: ";
        const char* errorStr = Update.errorString();
        if (errorStr && strlen(errorStr) > 0) {
          errorMsg += errorStr;
        } else {
          errorMsg += "Unknown error";
        }
        sendJSON(req, 500, errorMsg);
      } else {
        // Set pending validation
        OTA_MGR::markPending();
        sendJSON(req, 200, "FW ok, rebooting...");
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
        s_upload_start_time = millis();
        s_upload_loaded = 0;
        s_upload_total_size = Update.size();
        
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)){
          Update.printError(Serial);
          req->send(500, "application/json", "{\"ok\":false,\"msg\":\"Failed to start firmware update\"}");
          return;
        }
      }
      if (len){
        if (Update.write(data, len) != len){
          Update.printError(Serial);
          req->send(500, "application/json", "{\"ok\":false,\"msg\":\"Failed to write firmware data\"}");
          return;
        }
        s_upload_loaded += len;
      }
      if (final){
        if (!Update.end(true)){
          Update.printError(Serial);
          req->send(500, "application/json", "{\"ok\":false,\"msg\":\"Failed to finalize firmware update\"}");
          return;
        }
      }
    });

  // ===== OTA FS IMAGE (LittleFS .bin) =====
  // Build từ PlatformIO: "Build Filesystem Image" -> .pio/build/<env>/littlefs.bin
  server.on("/api/ota/fsimage", HTTP_POST,
    [](AsyncWebServerRequest* req){
      if (Update.hasError()){
        String errorMsg = "FS image update failed: ";
        const char* errorStr = Update.errorString();
        if (errorStr && strlen(errorStr) > 0) {
          errorMsg += errorStr;
        } else {
          errorMsg += "Unknown error";
        }
        sendJSON(req, 500, errorMsg);
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
        // Tạo backup trước khi update
        if (!OTA_MGR::createFSBackup()) {
          SLOGln("[OTA] Warning: Failed to create FS backup");
        }
        
        // Với LittleFS trên ESP32, dùng U_SPIFFS cho phân vùng DATA (LittleFS)
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_SPIFFS)){
          Update.printError(Serial);
          req->send(500, "application/json", "{\"ok\":false,\"msg\":\"Failed to start FS update\"}");
          return;
        }
      }
      if (len){
        if (Update.write(data, len) != len){
          Update.printError(Serial);
          req->send(500, "application/json", "{\"ok\":false,\"msg\":\"Failed to write FS data\"}");
          return;
        }
      }
      if (final){
        if (!Update.end(true)){
          Update.printError(Serial);
          req->send(500, "application/json", "{\"ok\":false,\"msg\":\"Failed to finalize FS update\"}");
          return;
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
        
        // Tạo backup file cũ nếu có
        if (LittleFS.exists(path)) {
          String backupPath = "/backup/files/";
          if (!LittleFS.exists(backupPath)) {
            LittleFS.mkdir(backupPath);
          }
          
          String timestamp = String(millis());
          String backupFile = backupPath + timestamp + "_" + path.substring(path.lastIndexOf("/") + 1);
          
          File src = LittleFS.open(path, "r");
          File dst = LittleFS.open(backupFile, "w");
          if (src && dst) {
            while (src.available()) {
              dst.write(src.read());
            }
            src.close();
            dst.close();
          }
        }
        
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
        // Trả về {ok:true} khi upload thành công
        req->send(200, "application/json", "{\"ok\":true}");
      }
    });
}
