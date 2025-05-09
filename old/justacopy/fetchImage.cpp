#include "fetchImage.h"
// Fetch a file from the URL given and save it in LittleFS
// Return 1 if a web fetch was needed or 0 if file already exists


#define FORMAT_LITTLEFS_IF_FAILED false

bool getFile(String url, String filename)
{

    if(!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)){
        Serial.println("LittleFS Mount Failed");
        return false;
    }
    
    // // If it exists then no need to fetch it
    // if (LittleFS.exists(filename) == true)
    // {
    //     Serial.println("Found " + filename);
    //     return 0;
    // }

    Serial.println("Downloading " + filename + " from " + url);


        Serial.print("[HTTP] begin...\n");

        HTTPClient http;
        // Configure server and url
        http.begin(url);

        Serial.print("[HTTP] GET...\n");
        // Start connection and send HTTP header
        int httpCode = http.GET();
        if (httpCode == 200)
        {
            fs::File f = LittleFS.open(filename, "w+");
            if (!f)
            {
                Serial.println("file open failed");
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

