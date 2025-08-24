#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>

class OTA_MGR {
public:
  static void begin();
  static void markValid();
  static void markPending();  // Add method to set pending flag
  static void markRollbackAndReboot();
  static bool isPendingValidate();
  static bool createFSBackup();
  static bool restoreFSBackup();
  static String getBackupList();
  
private:
  static Preferences prefs;
  static const char* NVS_NAMESPACE;
  static const char* PENDING_KEY;
  static const char* BACKUP_COUNT_KEY;
  static const char* BACKUP_PREFIX;
};

#endif // OTA_MANAGER_H
