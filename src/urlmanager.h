#ifndef urlmanager_h
#define urlmanager_h

#include <Arduino.h>
#include <Preferences.h>
#include <vector>

class UrlManager {
public:
    UrlManager(const char *namespaceName);

    void addUrl(const char *url);
    void removeUrl(const char *url);
    void printUrls();
    const char *getUrlAtIndex(int index);

private:
    Preferences preferences;
    const char *namespaceName;
    std::vector<String> urls; // Use std::vector for dynamic URL management
};

#endif
