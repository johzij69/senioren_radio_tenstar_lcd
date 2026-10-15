#ifndef PrioWebServer_H
#define PrioWebServer_H

#include <ESPAsyncWebServer.h>
#include <atomic>
#include "UrlManager.h"
#include "favicon.h"
#include "ArduinoJson.h"
#include "PrioWebjavascript.h"
#include "PrioWebcss.h"
#include "globals.h" 
#include "AlarmManager.h"
#include "MyPreferences.h"
#include "LittleFS.h"
#include "fetchImage.h"

struct OtaStatusSnapshot
{
  String currentVersion;
  String latestVersion;
  String sourceBaseUrl;
  String lastError;
  String updateMessage;
  bool updateAvailable;
  bool checkInProgress;
  bool updateInProgress;
  uint32_t uptimeSeconds;
  uint32_t lastCheckAgeSeconds;
};


class PrioWebServer
{
public:
  PrioWebServer(UrlManager &urlManager, AlarmManager &alarmManager, MyPreferences &preferences, int port);

  void begin();
  void processOtaRequests();
  // Zet de listener aan/uit zonder de handlers opnieuw te registreren; de stand
  // wordt bewaard en bij begin() weer toegepast.
  void setEnabled(bool enabled);
  bool isRunning() const;
  String ip;
  

private:
  AsyncWebServer server;
  bool running = false;
  UrlManager &urlManager;
  AlarmManager &alarmManager;
  MyPreferences &preferences;

  String htmlPage;

  String PROGMEM bigString;

  String getHtmlStart();
  String getHtmlEnd();
  String getTopMenu();
  String setHtmlBody(String body, String Script);
  String createHtmlPage(String body);

  void handleRoot(AsyncWebServerRequest *request);
  void handleAddStream(AsyncWebServerRequest *request, uint8_t *data);
  void handleInputStream(AsyncWebServerRequest *request);
  void handleSettings(AsyncWebServerRequest *request, uint8_t *data);//, size_t len);
  void handleDeleteStream(AsyncWebServerRequest *request);
  void handleInstellingen(AsyncWebServerRequest *request);
  void handleNetworkPage(AsyncWebServerRequest *request);
  void handleImportExportPage(AsyncWebServerRequest *request);
  void handleContactPage(AsyncWebServerRequest *request);
  void handleSynctime(AsyncWebServerRequest *request);
  void handleAlarmPage(AsyncWebServerRequest *request);
  void handleApiAlarms(AsyncWebServerRequest *request);
  void handleApiAlarmStatus(AsyncWebServerRequest *request);
  void handleApiExportConfig(AsyncWebServerRequest *request);
  void handleApiImportConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len);
  void handleSaveAlarms(AsyncWebServerRequest *request, uint8_t *data, size_t len);
  void handleApiSettings(AsyncWebServerRequest *request);
  void handleAlarmAudioStatus(AsyncWebServerRequest *request);
  void handleAlarmAudioUpload(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
  void finishAlarmAudioUpload(AsyncWebServerRequest *request);
  void clearAlarmAudioUpload(AsyncWebServerRequest *request);
  void handleApiOtaStatus(AsyncWebServerRequest *request);
  void handleApiOtaCheck(AsyncWebServerRequest *request);
  void handleApiOtaUpdate(AsyncWebServerRequest *request, uint8_t *data, size_t len);
  void handleSaveSettings(AsyncWebServerRequest *request, uint8_t *data, size_t len);
  void handleUploadLogo(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
  void handleUploadLogoReplace(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
  void handleRefreshLogo(AsyncWebServerRequest *request, uint8_t *data, size_t len);
  void onBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);




  void handleApi(AsyncWebServerRequest *request);
  bool checkOtaVersion();
  bool startOtaCheckAsync();
  bool startOtaUpdateAsync(String &message);
  bool runOtaUpdate(String &message, bool &updated);
  OtaStatusSnapshot getOtaStatusSnapshot();
  void otaCheckTaskBody();
  void otaUpdateTaskBody();

  String otaLatestVersion;
  String otaSourceBaseUrl;
  String otaLastError;
  String otaLastUpdateMessage;
  bool otaUpdateAvailable = false;
  std::atomic<bool> otaCheckInProgress{false};
  std::atomic<bool> otaUpdateInProgress{false};
  std::atomic<bool> otaCheckRequested{false};
  std::atomic<bool> otaUpdateRequested{false};
  std::atomic<bool> alarmAudioUploadBusy{false};
  unsigned long otaLastCheckMs = 0;
};

#endif
