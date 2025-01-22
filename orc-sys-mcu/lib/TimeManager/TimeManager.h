#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <Arduino.h>
#include <Timezone.h>
#include <NTPClient.h>
#include <WiFiUDP.h>

class TimeManager {
public:
    TimeManager();
    void begin(const char* ntpServer = "pool.ntp.org");
    void setTimezone(const char* tzOffset);
    String getCurrentTime();
    time_t getLocalTime();
    void update();
    bool isDST();
    
private:
    WiFiUDP ntpUDP;
    NTPClient* ntpClient;
    Timezone* tz;
    TimeChangeRule* dstRule;
    TimeChangeRule* stdRule;
    
    void createTimezoneRules(int offset);
    void setupDSTRules(int offset);
};

#endif // TIME_MANAGER_H
