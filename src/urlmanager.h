#ifndef URLMANAGER_H
#define URLMANAGER_H

#include "UrlManager.h"
#include "MyPreferences.h"
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

    uint32_t urlCount;
    std::vector<String> urls;
    std::vector<String> logo_urls;
    String default_logo;

private:
    MyPreferences &myPreferences;
  
};

#endif // ANOTHER_CLASS_H
