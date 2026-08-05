#include "fetchImage.h"
// Fetch a file from the URL given and save it in LittleFS
// Return 1 if a web fetch was needed or 0 if file already exists

// DISABLED: Logo download functionality conflicts with Audio library SSL
// Only logo upload via web interface is supported now

#define FORMAT_LITTLEFS_IF_FAILED false

static WiFiClient sHttpClient;
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

void testNetworkConnectivity() {
    Serial.println("\n========== NETWORK DIAGNOSTICS ==========");
    
    // 1. ESP32 Network Info
    Serial.printf("[INFO] ESP32 IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[INFO] Subnet Mask: %s\n", WiFi.subnetMask().toString().c_str());
    Serial.printf("[INFO] Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
    Serial.printf("[INFO] DNS: %s\n", WiFi.dnsIP().toString().c_str());
    
    // 2. Target Info
    String targetIP = "195.240.122.90";
    uint16_t targetPort = 444;
    Serial.printf("[INFO] Target: %s:%d\n", targetIP.c_str(), targetPort);
    
    // 3. Test Raw TCP Connection
    Serial.printf("[TEST] Testing TCP connection to %s:%d...\n", targetIP.c_str(), targetPort);
    WiFiClient tcpClient;
    
    unsigned long startTime = millis();
    bool connected = tcpClient.connect(targetIP.c_str(), targetPort);
    unsigned long elapsed = millis() - startTime;
    
    if (connected) {
        Serial.printf("[SUCCESS] TCP connection established in %lu ms\n", elapsed);
        Serial.printf("[INFO] Local port used: %d\n", tcpClient.localPort());
        tcpClient.stop();
    } else {
        Serial.printf("[FAILED] TCP connection failed after %lu ms\n", elapsed);
        Serial.println("[HINT] Possible causes:");
        Serial.println("  - ESP32 is on different VLAN/subnet");
        Serial.println("  - Firewall on target server blocks this IP");
        Serial.println("  - ARP resolution failed");
    }
    
    // 4. Test Alternative Port (80)
    Serial.printf("[TEST] Testing TCP connection to %s:80...\n", targetIP.c_str());
    WiFiClient tcpClient80;
    connected = tcpClient80.connect(targetIP.c_str(), 80);
    if (connected) {
        Serial.println("[SUCCESS] Port 80 is reachable");
        tcpClient80.stop();
    } else {
        Serial.println("[FAILED] Port 80 is also unreachable");
    }
    
    Serial.println("==========================================\n");
}

struct DownloadTaskParams {
    String url;
    String filename;
    int* httpCodeOut;
    bool* successOut;
    SemaphoreHandle_t doneSemaphore;
};

void downloadTask(void* pvParameters) {
    // DISABLED - Logo download conflicts with Audio SSL
    DownloadTaskParams* params = (DownloadTaskParams*)pvParameters;
    *(params->httpCodeOut) = 0;
    *(params->successOut) = false;
    xSemaphoreGive(params->doneSemaphore);
    vTaskDelete(NULL);
}

static bool downloadUrlToFile(const String &url, const String &filename, int &httpCodeOut)
{
    // Run network diagnostics before download
    testNetworkConnectivity();
    
    bool success = false;
    SemaphoreHandle_t doneSemaphore = xSemaphoreCreateBinary();
    if (doneSemaphore == NULL) return false;

    DownloadTaskParams params;
    params.url = url;
    params.filename = filename;
    params.httpCodeOut = &httpCodeOut;
    params.successOut = &success;
    params.doneSemaphore = doneSemaphore;

    BaseType_t taskCreated = xTaskCreate(
        downloadTask, "DownloadTask", 40960, &params, tskIDLE_PRIORITY + 1, NULL
    );

    if (taskCreated != pdPASS) {
        vSemaphoreDelete(doneSemaphore);
        return false;
    }

    xSemaphoreTake(doneSemaphore, portMAX_DELAY);
    vSemaphoreDelete(doneSemaphore);
    return success;
}

/* end */
bool getFile(String url, String filename)
{
    // DISABLED: Logo download functionality conflicts with Audio library SSL
    // Use web interface to upload logos instead
    Serial.println("[INFO] Logo download is disabled - use web upload instead");
    return false;
}

void listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
    Serial.printf("Listing directory: %s\n", dirname);

    File root = fs.open(dirname);
    if (!root) {
        Serial.println("Failed to open directory");
        return;
    }
    if (!root.isDirectory()) {
        Serial.println("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while (file) {
        if (file.isDirectory()) {
            Serial.printf("  DIR : %s\n", file.name());
            if (levels) {
                listDir(fs, file.name(), levels - 1);
            }
        } else {
            Serial.printf("  FILE: %s  SIZE: %d\n", file.name(), file.size());
        }
        file = root.openNextFile();
    }
}
