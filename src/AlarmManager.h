#ifndef ALARM_MANAGER_H
#define ALARM_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <time.h>
#include "MyPreferences.h"

class AlarmManager
{
public:
    static constexpr uint8_t MAX_ALARMS = 35;
    static constexpr uint8_t MAX_ALARMS_PER_DAY = 5;

    enum RepeatMode : uint8_t
    {
        REPEAT_DAILY = 0,
        REPEAT_WEEKDAYS = 1,
        REPEAT_WEEKEND = 2,
        REPEAT_WEEKLY = 3,
        REPEAT_CUSTOM = 4
    };

    enum RuntimeStatus : uint8_t
    {
        STATUS_OFF = 0,
        STATUS_ACTIVE = 1,
        STATUS_SNOOZE = 2
    };

    struct AlarmEntry
    {
        uint8_t id = 0;
        bool enabled = true;
        uint8_t hour = 7;
        uint8_t minute = 0;
        uint8_t streamIndex = 0;
        uint8_t volume = 12;
        RepeatMode mode = REPEAT_DAILY;
        uint8_t dayMask = 0x7F; // Bit0=Sun .. Bit6=Sat
        uint8_t snoozeMinutes = 10;
        int32_t lastTriggeredMinuteKey = -1;
    };

    explicit AlarmManager(MyPreferences &preferences);

    void begin();
    bool saveToPreferences();

    bool updateFromJson(uint8_t *data, size_t len, uint32_t streamCount, String &errorMessage);
    void appendAlarmsJson(JsonArray &array) const;

    bool poll(time_t now, AlarmEntry &triggeredAlarm, bool &fromSnooze);
    bool snoozeCurrentAlarm(time_t now, time_t &snoozeUntilOut);

    void stopRinging();
    bool isRinging() const;
    RuntimeStatus getRuntimeStatus() const;
    const char *getRuntimeStatusLabel() const;
    const char *getDisplayStatusLabel() const;

    uint8_t getCount() const;
    const AlarmEntry *getAlarms() const;

private:
    static const char *PREF_KEY;

    MyPreferences &prefs;
    AlarmEntry alarms[MAX_ALARMS];
    uint8_t alarmCount;

    bool alarmRinging;
    int currentAlarmId;
    bool snoozePending;
    time_t snoozeUntil;
    int snoozeAlarmId;

    bool loadFromPreferences();
    bool parseFromJsonDocument(JsonDocument &doc, uint32_t streamCount, String &errorMessage);

    static bool isDayActiveForAlarm(const AlarmEntry &alarm, uint8_t weekday);
    static int32_t buildMinuteKey(const tm &timeInfo);
    static RepeatMode modeFromString(const String &modeString);
    static const char *modeDebugLabel(RepeatMode mode);
    static String modeToString(RepeatMode mode);
    static uint8_t normalizeDayMask(RepeatMode mode, uint8_t dayMask);
    static bool validateAlarm(const AlarmEntry &alarm, uint32_t streamCount, String &errorMessage);
};

#endif