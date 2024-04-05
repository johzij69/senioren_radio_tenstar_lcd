#ifndef URLMANAGER_H
#define URLMANAGER_H

#include "UrlManager.h"
#include "MyPreferences.h"
#include "ArduinoJson.h"
#include <vector>

class UrlManager
{
public:
    UrlManager(MyPreferences &prefs);
    void readAndPrintValue(const char *key);
    void addUrl(const char *url);
    void addUrl(const char *url, const char *logo_url);
    void printAllUrls();
    const char *getUrlAtIndex(int index);
    String CreateDivUrls();
    void begin();
    void loadUrls();
    void PrinturlsFromPrev();
    void deleteUrl(int index);
    void saveUrls();
    void loadStreams();
    void saveStreams(uint8_t *data);
    void addStream(uint8_t *data);
    void deleteStream(int index);
    void saveToPreferences();


    uint32_t urlCount;
    uint32_t streamCount;
    
    std::vector<String> urls;
    std::vector<String> logo_urls;
    String default_logo;

    struct StreamItem
    {
        int id;
        String name;
        String url;
        String logo;
    } Streams[40]; // max 40 streams lijkt mij voldoende, kan het altijd nog dynamisch maken

private:
    MyPreferences &myPreferences;
};

#endif // ANOTHER_CLASS_H
