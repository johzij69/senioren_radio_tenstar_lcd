#include "fetchImage.h"
// Fetch a file from the URL given and save it in LittleFS
// Return 1 if a web fetch was needed or 0 if file already exists

#define FORMAT_LITTLEFS_IF_FAILED false

bool getFile(String url, String filename) {
    const String folder = "/StreamLogos";

    // Controleer of LittleFS is gestart
    if (!LittleFS.begin()) {
        Serial.println("[ERROR] LittleFS initialization failed!");
        return false;
    }

    // Controleer of de folder bestaat, anders maak deze aan
    if (!LittleFS.exists(folder)) {
        if (!LittleFS.mkdir(folder)) {
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

    // Bestand bestaat niet, start download
    HTTPClient http;
    http.begin(url);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        // Download succesvol, haal payload op
        String payload = http.getString();

        // Open bestand voor schrijven
        File file = LittleFS.open(filename, "w");
        if (!file) {
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
    } else {
        // Fout bij downloaden
        Serial.printf("[ERROR] Failed to download file: %s, HTTP code: %d\n", url.c_str(), httpCode);
        http.end();
        return false;
    }
}




bool getFileOld(String url, String filename)
{

    if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED))
    {
        Serial.println("LittleFS Mount Failed");
        return false;
    }
    Serial.println("LittleFS Mounted Successfully");

    if (!LittleFS.exists("/StreamLogos"))
    {
        LittleFS.mkdir("/StreamLogos");
    }

    Serial.println("Fetching " + url + " to " + filename);

    // If it exists then no need to fetch it
    if (LittleFS.exists(filename) == true)
    {
        Serial.println("Found " + filename);
        return 0;
    }

    Serial.println("Downloading " + filename + " from " + url);

    Serial.print("[HTTP] begin...\n");

    HTTPClient http;
    // Configure server and url
    http.begin(url);

    Serial.print("[HTTP] GET...\n");
    // Start connection and send HTTP header
    int httpCode = http.GET();

    Serial.printf("[HTTP] GET... code: %d\n", httpCode);

    if (httpCode == HTTP_CODE_OK)
    {
       Serial.println("open file for writing: "+filename);
       fs::File f = LittleFS.open(filename, "w+");
        if (!f)
        {
            Serial.println("file open for writing failed");
            return 0;
        }
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTP] GET... code: %d\n", httpCode);

        // File found at server
        if (httpCode == HTTP_CODE_OK)
        {

            // Get length of document (is -1 when Server sends no Content-Length header)
            int total = http.getSize();
            int len = total;

            // Create buffer for read
            uint8_t buff[128] = {0};

            // Get tcp stream
            WiFiClient *stream = http.getStreamPtr();

            // Read all data from server
            while (http.connected() && (len > 0 || len == -1))
            {
                // Get available data size
                size_t size = stream->available();

                if (size)
                {
                    // Read up to 128 bytes
                    int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));

                    // Write it to file
                    f.write(buff, c);

                    // Calculate remaining bytes
                    if (len > 0)
                    {
                        len -= c;
                    }
                }
                yield();
            }
            Serial.println();
            Serial.print("[HTTP] connection closed or file end.\n");
        }
        f.close();
    }
    else
    {
        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();

    return 1; // File was fetched from web
}
