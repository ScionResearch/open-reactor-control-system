#include "TimeManager.h"

TimeManager::TimeManager() {
    ntpClient = new NTPClient(ntpUDP);
    dstRule = new TimeChangeRule();
    stdRule = new TimeChangeRule();
}

void TimeManager::begin(const char* ntpServer) {
    ntpClient->begin();
    ntpClient->setPoolServerName(ntpServer);
    ntpClient->setUpdateInterval(3600000); // Update every hour
}

void TimeManager::setTimezone(const char* tzOffset) {
    // Parse timezone offset (format: "+HH:MM" or "-HH:MM")
    int hours = atoi(tzOffset);
    int minutes = abs(atoi(tzOffset + 4));
    int totalOffset = hours * 60 + (hours < 0 ? -minutes : minutes);
    
    createTimezoneRules(totalOffset);
}

void TimeManager::createTimezoneRules(int offsetMinutes) {
    // Convert offset to minutes and set up rules
    int hours = offsetMinutes / 60;
    int minutes = abs(offsetMinutes % 60);

    // Standard time rule
    stdRule->week = 0;
    stdRule->dow = 0;
    stdRule->month = 0;
    stdRule->hour = 0;
    stdRule->offset = hours * 60 + (hours < 0 ? -minutes : minutes);

    // DST rule - automatically detect based on major zones
    setupDSTRules(offsetMinutes);

    // Create new Timezone object
    if (tz != nullptr) {
        delete tz;
    }
    tz = new Timezone(*dstRule, *stdRule);
}

void TimeManager::setupDSTRules(int offsetMinutes) {
    // Set up DST rules based on common timezone practices
    dstRule->week = 0;
    dstRule->dow = 0;
    dstRule->month = 0;
    dstRule->hour = 0;
    dstRule->offset = offsetMinutes;

    // Example: US rules
    if (offsetMinutes == -300) { // EST
        dstRule->week = 2;
        dstRule->dow = 1;
        dstRule->month = 3;
        dstRule->hour = 2;
        dstRule->offset = -240; // EDT
    }
    // Add more DST rules for other zones as needed
    // New Zealand rules
    else if (offsetMinutes == 720) { // NZST
        dstRule->week = 1;
        dstRule->dow = 0;
        dstRule->month = 9;
        dstRule->hour = 2;
        dstRule->offset = 780; // NZDT
    }
}

String TimeManager::getCurrentTime() {
    time_t localTime = getLocalTime();
    char timeStr[25];
    sprintf(timeStr, "%04d-%02d-%02dT%02d:%02d:%02d",
            year(localTime), month(localTime), day(localTime),
            hour(localTime), minute(localTime), second(localTime));
    return String(timeStr);
}

time_t TimeManager::getLocalTime() {
    if (ntpClient->update()) {
        time_t utc = ntpClient->getEpochTime();
        return tz->toLocal(utc);
    }
    return 0;
}

void TimeManager::update() {
    ntpClient->update();
}

bool TimeManager::isDST() {
    time_t utc = ntpClient->getEpochTime();
    return tz->locIsDST(utc);
}
