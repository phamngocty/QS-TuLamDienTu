#include "ota_manager.h"
#include <LittleFS.h>
#include <esp_ota_ops.h>
#include "config_store.h"

Preferences OTA_MGR::prefs;
const char* OTA_MGR::NVS_NAMESPACE = "ota";
const char* OTA_MGR::PENDING_KEY = "pending";
const char* OTA_MGR::BACKUP_COUNT_KEY = "backup_count";
const char* OTA_MGR::BACKUP_PREFIX = "/backup";

void OTA_MGR::begin() {
  prefs.begin(NVS_NAMESPACE, false);
  
  // Kiểm tra nếu có pending validate
  if (isPendingValidate()) {
    Serial.println("[OTA_MGR] Pending validation detected - starting watchdog");
    // TODO: Implement watchdog timer for validation
    // Nếu không gọi markValid() trong 30s, sẽ tự động rollback
  }
  
  Serial.println("[OTA_MGR] OTA Manager initialized");
}

void OTA_MGR::markValid() {
  prefs.putBool(PENDING_KEY, false);
  Serial.println("[OTA_MGR] Firmware marked as valid - rollback cancelled");
}

void OTA_MGR::markPending() {
  prefs.putBool(PENDING_KEY, true);
  Serial.println("[OTA_MGR] Firmware marked as pending validation");
}

void OTA_MGR::markRollbackAndReboot() {
  Serial.println("[OTA_MGR] Rolling back firmware and rebooting...");
  delay(1000);
  
  // Gọi ESP-IDF API để rollback
  esp_ota_mark_app_invalid_rollback_and_reboot();
}

bool OTA_MGR::isPendingValidate() {
  return prefs.getBool(PENDING_KEY, false);
}

bool OTA_MGR::createFSBackup() {
  if (!LittleFS.begin()) {
    Serial.println("[OTA_MGR] Failed to mount LittleFS for backup");
    return false;
  }
  
  // Tạo thư mục backup nếu chưa có
  if (!LittleFS.exists("/backup")) {
    LittleFS.mkdir("/backup");
  }
  
  // Tạo timestamp cho backup
  String timestamp = String(millis());
  String backupPath = String(BACKUP_PREFIX) + "/snap_" + timestamp + ".lfs";
  
  // Tạo file backup
  File backupFile = LittleFS.open(backupPath, "w");
  if (!backupFile) {
    Serial.println("[OTA_MGR] Failed to create backup file");
    return false;
  }
  
  // Ghi header
  backupFile.println("LFS_BACKUP_v1");
  backupFile.println(timestamp);
  
  // Duyệt và backup các file quan trọng
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  
  int fileCount = 0;
  while (file) {
    if (!file.isDirectory()) {
      String fileName = file.name();
      if (fileName.startsWith("/")) fileName = fileName.substring(1);
      
      // Backup file
      backupFile.println("FILE:" + fileName + ":" + file.size());
      
      // Copy nội dung file
      file.seek(0);
      while (file.available()) {
        backupFile.write(file.read());
      }
      backupFile.println(); // Separator
      fileCount++;
    }
    file = root.openNextFile();
  }
  
  backupFile.close();
  
  // Cập nhật backup count
  int currentCount = prefs.getInt(BACKUP_COUNT_KEY, 0);
  prefs.putInt(BACKUP_COUNT_KEY, currentCount + 1);
  
  Serial.printf("[OTA_MGR] FS backup created: %s (%d files)\n", backupPath.c_str(), fileCount);
  return true;
}

bool OTA_MGR::restoreFSBackup() {
  if (!LittleFS.begin()) {
    Serial.println("[OTA_MGR] Failed to mount LittleFS for restore");
    return false;
  }
  
  // Tìm backup mới nhất
  String latestBackup = "";
  File root = LittleFS.open("/backup");
  File file = root.openNextFile();
  
  while (file) {
    String fileName = String(file.name());
    if (!file.isDirectory() && fileName.startsWith("/backup/snap_")) {
      if (latestBackup == "" || fileName > latestBackup) {
        latestBackup = fileName;
      }
    }
    file = root.openNextFile();
  }
  
  if (latestBackup == "") {
    Serial.println("[OTA_MGR] No backup found");
    return false;
  }
  
  Serial.printf("[OTA_MGR] Restoring from backup: %s\n", latestBackup.c_str());
  
  // TODO: Implement restore logic
  // Đọc backup file và khôi phục các file
  
  return true;
}

String OTA_MGR::getBackupList() {
  String result = "[";
  bool first = true;
  
  if (LittleFS.begin()) {
    File root = LittleFS.open("/backup");
    File file = root.openNextFile();
    
    while (file) {
      String fileName = String(file.name());
      if (!file.isDirectory() && fileName.startsWith("/backup/snap_")) {
        if (!first) result += ",";
        result += "\"" + fileName + "\"";
        first = false;
      }
      file = root.openNextFile();
    }
  }
  
  result += "]";
  return result;
}
