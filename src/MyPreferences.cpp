#include "MyPreferences.h"

MyPreferences::MyPreferences(const char* namespaceName) {
    preferences.begin(namespaceName, false);
}

void MyPreferences::writeValue(const char* key, int value) {
    preferences.putInt(key, value);
}

int MyPreferences::readValue(const char* key, int defaultValue) {
    return preferences.getInt(key, defaultValue);
}

void MyPreferences::writeString(const char* key, const char* value) {
    preferences.putString(key, value);
}

String MyPreferences::readString(const char* key, const char* defaultValue) {
    return preferences.getString(key, defaultValue);
}

void MyPreferences::putUInt(const char* key, uint32_t value) {
    preferences.putUInt(key, value);
}

uint32_t MyPreferences::getUInt(const char* key, uint32_t defaultValue) {
    return preferences.getUInt(key, defaultValue);
}
