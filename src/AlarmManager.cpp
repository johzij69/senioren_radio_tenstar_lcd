#include "AlarmManager.h"

constexpr uint8_t AlarmManager::MAX_ALARMS;
constexpr uint8_t AlarmManager::MAX_ALARMS_PER_DAY;

const char *AlarmManager::PREF_KEY = "alarms_json";

AlarmManager::AlarmManager(MyPreferences &preferences)
    : prefs(preferences), alarmCount(0), alarmRinging(false), currentAlarmId(-1), snoozePending(false), snoozeUntil(0), snoozeAlarmId(-1)
{
}

void AlarmManager::begin()
{
    loadFromPreferences();
}

bool AlarmManager::loadFromPreferences()
{
    String stored = prefs.readString(PREF_KEY, "");
    if (stored.length() == 0)
    {
        alarmCount = 0;
        return true;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, stored);
    if (err)
    {
        Serial.println("AlarmManager: kon alarms JSON niet laden, reset naar leeg");
        alarmCount = 0;
        return false;
    }

    String error;
    return parseFromJsonDocument(doc, 255, error);
}

bool AlarmManager::saveToPreferences()
{
    JsonDocument doc;
    JsonArray alarmArray = doc["alarms"].to<JsonArray>();
    appendAlarmsJson(alarmArray);

    String output;
    serializeJson(doc, output);
    prefs.writeString(PREF_KEY, output.c_str());
    return true;
}

bool AlarmManager::updateFromJson(uint8_t *data, size_t len, uint32_t streamCount, String &errorMessage)
{
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, data, len);
    if (err)
    {
        errorMessage = "Invalid JSON";
        return false;
    }

    if (!parseFromJsonDocument(doc, streamCount, errorMessage))
    {
        return false;
    }

    saveToPreferences();

    Serial.println(String("AlarmManager: alarms opgeslagen, count=") + alarmCount);
    for (uint8_t i = 0; i < alarmCount; i++)
    {
        const AlarmEntry &a = alarms[i];
        Serial.println(
            String("  [") + i + "] id=" + a.id +
            " enabled=" + (a.enabled ? "1" : "0") +
            " time=" + (a.hour < 10 ? "0" : "") + String(a.hour) + ":" + (a.minute < 10 ? "0" : "") + String(a.minute) +
            " mode=" + modeDebugLabel(a.mode) +
            " dayMask=0x" + String(a.dayMask, HEX) +
            " stream=" + a.streamIndex +
            " vol=" + a.volume +
            " snooze=" + a.snoozeMinutes);
    }

    return true;
}

bool AlarmManager::parseFromJsonDocument(JsonDocument &doc, uint32_t streamCount, String &errorMessage)
{
    if (!doc["alarms"].is<JsonArray>())
    {
        errorMessage = "JSON moet een alarms array bevatten";
        return false;
    }

    JsonArray incoming = doc["alarms"].as<JsonArray>();
    if (incoming.size() > MAX_ALARMS)
    {
        errorMessage = "Te veel alarmen";
        return false;
    }

    AlarmEntry parsed[MAX_ALARMS];
    uint8_t parsedCount = 0;

    for (JsonVariant value : incoming)
    {
        if (!value.is<JsonObject>())
        {
            errorMessage = "Alarm item heeft ongeldig formaat";
            return false;
        }

        JsonObject obj = value.as<JsonObject>();

        AlarmEntry alarm;
        alarm.id = obj["id"].is<uint8_t>() ? obj["id"].as<uint8_t>() : parsedCount;
        alarm.enabled = obj["enabled"].is<bool>() ? obj["enabled"].as<bool>() : true;
        alarm.hour = obj["hour"].is<uint8_t>() ? obj["hour"].as<uint8_t>() : 7;
        alarm.minute = obj["minute"].is<uint8_t>() ? obj["minute"].as<uint8_t>() : 0;
        alarm.streamIndex = obj["streamIndex"].is<uint8_t>() ? obj["streamIndex"].as<uint8_t>() : 0;
        alarm.volume = obj["volume"].is<uint8_t>() ? obj["volume"].as<uint8_t>() : 12;
        alarm.snoozeMinutes = obj["snoozeMinutes"].is<uint8_t>() ? obj["snoozeMinutes"].as<uint8_t>() : 10;

        String modeString = obj["mode"].is<const char *>() ? String((const char *)obj["mode"]) : String("daily");
        alarm.mode = modeFromString(modeString);
        alarm.dayMask = obj["dayMask"].is<uint8_t>() ? obj["dayMask"].as<uint8_t>() : 0x7F;
        alarm.dayMask = normalizeDayMask(alarm.mode, alarm.dayMask);
        alarm.lastTriggeredMinuteKey = -1;

        if (!validateAlarm(alarm, streamCount, errorMessage))
        {
            return false;
        }

        parsed[parsedCount++] = alarm;
    }

    uint8_t perDayCounter[7] = {0, 0, 0, 0, 0, 0, 0};
    for (uint8_t i = 0; i < parsedCount; i++)
    {
        if (!parsed[i].enabled)
        {
            continue;
        }

        for (uint8_t day = 0; day < 7; day++)
        {
            if (isDayActiveForAlarm(parsed[i], day))
            {
                perDayCounter[day]++;
                if (perDayCounter[day] > MAX_ALARMS_PER_DAY)
                {
                    errorMessage = "Maximaal 5 alarmen per dag toegestaan";
                    return false;
                }
            }
        }
    }

    alarmCount = parsedCount;
    for (uint8_t i = 0; i < alarmCount; i++)
    {
        alarms[i] = parsed[i];
    }

    stopRinging();
    return true;
}

void AlarmManager::appendAlarmsJson(JsonArray &array) const
{
    for (uint8_t i = 0; i < alarmCount; i++)
    {
        const AlarmEntry &alarm = alarms[i];
        JsonObject item = array.add<JsonObject>();
        item["id"] = alarm.id;
        item["enabled"] = alarm.enabled;
        item["hour"] = alarm.hour;
        item["minute"] = alarm.minute;
        item["streamIndex"] = alarm.streamIndex;
        item["volume"] = alarm.volume;
        item["mode"] = modeToString(alarm.mode);
        item["dayMask"] = alarm.dayMask;
        item["snoozeMinutes"] = alarm.snoozeMinutes;
    }
}

bool AlarmManager::poll(time_t now, AlarmEntry &triggeredAlarm, bool &fromSnooze)
{
    fromSnooze = false;

    if (now <= 100000)
    {
        return false;
    }

    tm timeInfo;
    localtime_r(&now, &timeInfo);
    int32_t minuteKey = buildMinuteKey(timeInfo);

    static int32_t lastPollLogMinuteKey = -1;
    if (lastPollLogMinuteKey != minuteKey)
    {
        lastPollLogMinuteKey = minuteKey;

        Serial.println(
            String("Alarm poll: ") +
            (timeInfo.tm_year + 1900) + "-" +
            (timeInfo.tm_mon + 1 < 10 ? "0" : "") + String(timeInfo.tm_mon + 1) + "-" +
            (timeInfo.tm_mday < 10 ? "0" : "") + String(timeInfo.tm_mday) + " " +
            (timeInfo.tm_hour < 10 ? "0" : "") + String(timeInfo.tm_hour) + ":" +
            (timeInfo.tm_min < 10 ? "0" : "") + String(timeInfo.tm_min) +
            " wday=" + String(timeInfo.tm_wday) +
            " count=" + String(alarmCount) +
            " ringing=" + (alarmRinging ? "1" : "0") +
            " snoozePending=" + (snoozePending ? "1" : "0"));

        for (uint8_t i = 0; i < alarmCount; i++)
        {
            const AlarmEntry &a = alarms[i];
            bool timeMatch = (a.hour == timeInfo.tm_hour && a.minute == timeInfo.tm_min);
            bool dayActive = isDayActiveForAlarm(a, static_cast<uint8_t>(timeInfo.tm_wday));
            bool alreadyTriggeredThisMinute = (a.lastTriggeredMinuteKey == minuteKey);

            Serial.println(
                String("  alarm id=") + a.id +
                " en=" + (a.enabled ? "1" : "0") +
                " t=" + (a.hour < 10 ? "0" : "") + String(a.hour) + ":" + (a.minute < 10 ? "0" : "") + String(a.minute) +
                " mode=" + modeDebugLabel(a.mode) +
                " mask=0x" + String(a.dayMask, HEX) +
                " timeMatch=" + (timeMatch ? "1" : "0") +
                " dayActive=" + (dayActive ? "1" : "0") +
                " already=" + (alreadyTriggeredThisMinute ? "1" : "0") +
                " stream=" + a.streamIndex);
        }
    }

    if (snoozePending && now >= snoozeUntil)
    {
        for (uint8_t i = 0; i < alarmCount; i++)
        {
            if (alarms[i].id == snoozeAlarmId)
            {
                if (alarms[i].lastTriggeredMinuteKey != minuteKey)
                {
                    Serial.println(String("Alarm trigger (snooze): id=") + alarms[i].id + " minuteKey=" + minuteKey);
                    alarms[i].lastTriggeredMinuteKey = minuteKey;
                    alarmRinging = true;
                    currentAlarmId = alarms[i].id;
                    snoozePending = false;
                    snoozeAlarmId = -1;
                    triggeredAlarm = alarms[i];
                    fromSnooze = true;
                    return true;
                }
            }
        }

        snoozePending = false;
        snoozeAlarmId = -1;
    }

    uint8_t weekday = static_cast<uint8_t>(timeInfo.tm_wday);

    for (uint8_t i = 0; i < alarmCount; i++)
    {
        AlarmEntry &alarm = alarms[i];
        if (!alarm.enabled)
        {
            continue;
        }

        if (alarm.hour != timeInfo.tm_hour || alarm.minute != timeInfo.tm_min)
        {
            continue;
        }

        if (!isDayActiveForAlarm(alarm, weekday))
        {
            continue;
        }

        if (alarm.lastTriggeredMinuteKey == minuteKey)
        {
            continue;
        }

        Serial.println(String("Alarm trigger: id=") + alarm.id + " stream=" + alarm.streamIndex + " volume=" + alarm.volume + " minuteKey=" + minuteKey);
        alarm.lastTriggeredMinuteKey = minuteKey;
        alarmRinging = true;
        currentAlarmId = alarm.id;
        snoozePending = false;
        snoozeAlarmId = -1;
        triggeredAlarm = alarm;
        return true;
    }

    return false;
}

bool AlarmManager::snoozeCurrentAlarm(time_t now, time_t &snoozeUntilOut)
{
    if (!alarmRinging || currentAlarmId < 0)
    {
        return false;
    }

    for (uint8_t i = 0; i < alarmCount; i++)
    {
        if (alarms[i].id == currentAlarmId)
        {
            uint8_t snoozeMinutes = alarms[i].snoozeMinutes;
            if (snoozeMinutes == 0)
            {
                snoozeMinutes = 10;
            }
            snoozeUntil = now + (snoozeMinutes * 60);
            snoozeUntilOut = snoozeUntil;
            snoozePending = true;
            snoozeAlarmId = currentAlarmId;
            alarmRinging = false;
            currentAlarmId = -1;
            return true;
        }
    }

    return false;
}

void AlarmManager::stopRinging()
{
    alarmRinging = false;
    currentAlarmId = -1;
    snoozePending = false;
    snoozeAlarmId = -1;
    snoozeUntil = 0;
}

bool AlarmManager::isRinging() const
{
    return alarmRinging;
}

AlarmManager::RuntimeStatus AlarmManager::getRuntimeStatus() const
{
    if (alarmRinging)
    {
        return STATUS_ACTIVE;
    }

    if (snoozePending)
    {
        return STATUS_SNOOZE;
    }

    return STATUS_OFF;
}

const char *AlarmManager::getRuntimeStatusLabel() const
{
    RuntimeStatus status = getRuntimeStatus();
    if (status == STATUS_ACTIVE)
    {
        return "actief";
    }
    if (status == STATUS_SNOOZE)
    {
        return "snooze";
    }
    return "uit";
}

const char *AlarmManager::getDisplayStatusLabel() const
{
    if (alarmRinging)
    {
        return "Alarm actief";
    }

    if (snoozePending)
    {
        return "Snooze wacht";
    }

    for (uint8_t i = 0; i < alarmCount; i++)
    {
        if (alarms[i].enabled)
        {
            return "Ingesteld";
        }
    }

    return "Geen alarm";
}

uint8_t AlarmManager::getCount() const
{
    return alarmCount;
}

const AlarmManager::AlarmEntry *AlarmManager::getAlarms() const
{
    return alarms;
}

bool AlarmManager::isDayActiveForAlarm(const AlarmEntry &alarm, uint8_t weekday)
{
    if (weekday > 6)
    {
        return false;
    }

    uint8_t normalizedMask = normalizeDayMask(alarm.mode, alarm.dayMask);
    return (normalizedMask & (1 << weekday)) != 0;
}

int32_t AlarmManager::buildMinuteKey(const tm &timeInfo)
{
    int32_t dayKey = (timeInfo.tm_year * 1000) + timeInfo.tm_yday;
    return (dayKey * 1440) + (timeInfo.tm_hour * 60) + timeInfo.tm_min;
}

AlarmManager::RepeatMode AlarmManager::modeFromString(const String &modeString)
{
    if (modeString == "weekdays")
    {
        return REPEAT_WEEKDAYS;
    }
    if (modeString == "weekend")
    {
        return REPEAT_WEEKEND;
    }
    if (modeString == "weekly")
    {
        return REPEAT_WEEKLY;
    }
    if (modeString == "custom")
    {
        return REPEAT_CUSTOM;
    }
    return REPEAT_DAILY;
}

const char *AlarmManager::modeDebugLabel(RepeatMode mode)
{
    switch (mode)
    {
    case REPEAT_WEEKDAYS:
        return "weekdays";
    case REPEAT_WEEKEND:
        return "weekend";
    case REPEAT_WEEKLY:
        return "weekly";
    case REPEAT_CUSTOM:
        return "custom";
    case REPEAT_DAILY:
    default:
        return "daily";
    }
}

String AlarmManager::modeToString(RepeatMode mode)
{
    switch (mode)
    {
    case REPEAT_WEEKDAYS:
        return "weekdays";
    case REPEAT_WEEKEND:
        return "weekend";
    case REPEAT_WEEKLY:
        return "weekly";
    case REPEAT_CUSTOM:
        return "custom";
    case REPEAT_DAILY:
    default:
        return "daily";
    }
}

uint8_t AlarmManager::normalizeDayMask(RepeatMode mode, uint8_t dayMask)
{
    switch (mode)
    {
    case REPEAT_DAILY:
        return 0x7F;
    case REPEAT_WEEKDAYS:
        return 0x3E; // Mon..Fri
    case REPEAT_WEEKEND:
        return 0x41; // Sun + Sat
    case REPEAT_WEEKLY:
    {
        if ((dayMask & 0x7F) == 0)
        {
            return 0x02; // Monday default
        }

        for (uint8_t bit = 0; bit < 7; bit++)
        {
            if ((dayMask & (1 << bit)) != 0)
            {
                return (1 << bit);
            }
        }
        return 0x02;
    }
    case REPEAT_CUSTOM:
    default:
        return dayMask & 0x7F;
    }
}

bool AlarmManager::validateAlarm(const AlarmEntry &alarm, uint32_t streamCount, String &errorMessage)
{
    if (alarm.hour > 23)
    {
        errorMessage = "Uur moet tussen 0 en 23 liggen";
        return false;
    }
    if (alarm.minute > 59)
    {
        errorMessage = "Minuut moet tussen 0 en 59 liggen";
        return false;
    }
    if (alarm.volume > 30)
    {
        errorMessage = "Volume moet tussen 0 en 30 liggen";
        return false;
    }
    if (streamCount > 0 && alarm.streamIndex >= streamCount)
    {
        errorMessage = "Alarm verwijst naar een onbekende stream";
        return false;
    }
    if (alarm.mode == REPEAT_CUSTOM && (alarm.dayMask & 0x7F) == 0)
    {
        errorMessage = "Custom alarm moet minimaal 1 dag hebben";
        return false;
    }
    if (alarm.snoozeMinutes > 120)
    {
        errorMessage = "Snooze moet tussen 0 en 120 minuten liggen";
        return false;
    }
    return true;
}