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
    void printAllUrls();
    const char *getUrlAtIndex(int index);
        String CreateDivUrls();

private:
    MyPreferences &myPreferences;
    std::vector<String> urls;
};

#endif // ANOTHER_CLASS_H
