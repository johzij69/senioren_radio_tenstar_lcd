#ifndef MY_PREFERENCES_H
#define MY_PREFERENCES_H

#include <Preferences.h>
#include <nvs_flash.h>

class MyPreferences {
public:
    MyPreferences(const char* namespaceName);
    ~MyPreferences();
    void begin();
    void writeValue(const char* key, int value);
    int readValue(const char* key, int defaultValue);
    void writeString(const char* key, const char* value);
    String readString(const char* key, const char *defaultValue);
    void putUInt(const char* key, uint32_t value);
    uint32_t getUInt(const char* key, uint32_t defaultValue);
    bool remove(const char* key);

private:
    Preferences preferences;
    String namespaceNameStr;
    bool initialized;
    bool isNVSInitialized();
    void initializeNVS() ;

};

#endif  // MY_PREFERENCES_H
