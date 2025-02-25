#include "fetchImage.h"
// Fetch a file from the URL given and save it in LittleFS
// Return 1 if a web fetch was needed or 0 if file already exists

#define FORMAT_LITTLEFS_IF_FAILED false

bool getFile(String url, String filename)
{
    const String folder = "/StreamLogos";

    // Controleer of LittleFS is gestart
    if (!LittleFS.begin())
    {
        Serial.println("[ERROR] LittleFS initialization failed!");
        return false;
    }

    // Controleer of de folder bestaat, anders maak deze aan
    if (!LittleFS.exists(folder))
    {
        if (!LittleFS.mkdir(folder))
        {
            Serial.println("[ERROR] Failed to create folder /StreamLogos");
            return false;
        }
    }

    // Volledig pad van het bestand
    // String filePath = folder + "/" + filename;

    // Controleer of het bestand al bestaat
    if (LittleFS.exists(filename)) {
        Serial.println("[INFO] File already exists: " + filename);
        return true; // Bestand bestaat al, downloaden overslaan
    }

    if (LittleFS.exists(filename))
    {
        Serial.println("[INFO] File already exists: removing " + filename);
        if (LittleFS.remove(filename)){
            Serial.println("[INFO] File removed: " + filename);
        };
    }

    listDir(LittleFS, folder.c_str(), 1);

    // Bestand bestaat niet, start download
    HTTPClient http;
    http.begin(url);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK)
    {
        // Download succesvol, haal payload op
        String payload = http.getString();

        // Open bestand voor schrijven
        File file = LittleFS.open(filename, "w");
        if (!file)
        {
            Serial.println("[ERROR] Failed to open file for writing: " + filename);
            http.end();
            return false;
        }

        // Schrijf data naar bestand
        file.print(payload);
        file.close();
        Serial.println("[INFO] File downloaded and saved: " + filename);

        http.end();
        return true;
    }
    else
    {
        // Fout bij downloaden
        Serial.printf("[ERROR] Failed to download file: %s, HTTP code: %d\n", url.c_str(), httpCode);
        http.end();
        return false;
    }
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
