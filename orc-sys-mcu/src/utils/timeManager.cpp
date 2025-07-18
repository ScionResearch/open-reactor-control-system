#include "timeManager.h"

MCP79410 rtc(Wire1);

// Global DateTime protection
volatile bool dateTimeLocked = false;
volatile bool dateTimeWriteLocked = false;

DateTime globalDateTime;

uint32_t lastTimeUpdate = 0;

void init_timeManager(void) {
  Wire1.setSDA(PIN_RTC_SDA);
  Wire1.setSCL(PIN_RTC_SCL);

  if (!rtc.begin())
  {
    log(LOG_ERROR, false, "RTC initialization failed!\n");
    if (!statusLocked) {
      statusLocked = true;
      status.rtcOK = false;
      status.updated = true;
      statusLocked = false;
    }
    return;
  }

  // Set initial time (24-hour format)
  DateTime now;
  rtc.getDateTime(&now);
  memcpy(&globalDateTime, &now, sizeof(DateTime)); // Initialize global DateTime directly
  log(LOG_INFO, false, "Current date and time is: %04d-%02d-%02d %02d:%02d:%02d\n",
                now.year, now.month, now.day, now.hour, now.minute, now.second);
                
  log(LOG_INFO, false, "RTC update task started\n");
  lastTimeUpdate = millis();
  if (!statusLocked) {
    statusLocked = true;
    status.rtcOK = true;
    status.updated = true;
    statusLocked = false;
  }
}

void manageTime(void)
{
  if (millis() - lastTimeUpdate < TIME_UPDATE_INTERVAL) return;
  if (dateTimeLocked) return;
  dateTimeLocked = true;
  DateTime currentTime;
  if (rtc.getDateTime(&currentTime)) {
    memcpy(&globalDateTime, &currentTime, sizeof(DateTime));
    dateTimeLocked = false;
  } else {
    dateTimeLocked = false;
    log(LOG_ERROR, true, "Failed to read time from RTC\n");
    if (!statusLocked) {
      statusLocked = true;
      status.rtcOK = false;
      status.updated = true;
      statusLocked = false;
    }
  }
  if (!statusLocked) {
    statusLocked = true;
    status.rtcOK = true;
    status.updated = true;
    statusLocked = false;
  }
  lastTimeUpdate += TIME_UPDATE_INTERVAL;  
}

DateTime epochToDateTime(time_t epochTime) {
  struct tm *timeinfo = gmtime(&epochTime);
  DateTime dt = {
      .year = (uint16_t)(timeinfo->tm_year + 1900),
      .month = (uint8_t)(timeinfo->tm_mon + 1),
      .day = (uint8_t)timeinfo->tm_mday,
      .hour = (uint8_t)timeinfo->tm_hour,
      .minute = (uint8_t)timeinfo->tm_min,
      .second = (uint8_t)timeinfo->tm_sec};
  return dt;
}

bool getGlobalDateTime(DateTime &dt, uint32_t timeout) {
  uint32_t startTime = millis();
  while (dateTimeLocked) {
    if (millis() - startTime > timeout) return false;
  }
  dateTimeLocked = true;
  memcpy(&dt, &globalDateTime, sizeof(DateTime));
  dateTimeLocked = false;
  return true;
}


// Function to safely update the DateTime
bool updateGlobalDateTime(const DateTime &dt) {
  if (dateTimeWriteLocked) {
    log(LOG_ERROR, true, "Failed to update time: DateTime write lock is active - can't handle multiple simultaneous updates\n");
    return false;
  }
  dateTimeWriteLocked = true;
  const int maxRetries = 3; // Maximum number of retries
  const int retryDelayMs = 100; // Delay between retries (milliseconds)
  bool success = false;
  for (int retry = 0; retry < maxRetries; ++retry) {
      log(LOG_INFO, false, "Attempt %d: Setting RTC to: %04d-%02d-%02d %02d:%02d:%02d\n",
                    retry + 1, dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);

      rtc.setDateTime(dt); // Set RTC time

      // Verify the time was set by reading it back
      DateTime currentTime;
      if (rtc.getDateTime(&currentTime)) {
        // Check that time is correct before proceeding
        if (currentTime.year == dt.year &&
            currentTime.month == dt.month &&
            currentTime.day == dt.day &&
            currentTime.hour == dt.hour &&
            currentTime.minute == dt.minute &&
            currentTime.second == dt.second) {
              log(LOG_INFO, false, "RTC verification successful after %d retries.\n", retry);
              memcpy(&globalDateTime, &dt, sizeof(DateTime)); // Update global time after successful write
              success = true;
              break; // Exit retry loop on success
        } else {
          log(LOG_ERROR, true, "RTC verification failed, current time: %04d-%02d-%02d %02d:%02d:%02d, expected time: %04d-%02d-%02d %02d:%02d:%02d\n", 
                  currentTime.year, currentTime.month, currentTime.day,
                  currentTime.hour, currentTime.minute, currentTime.second,
                  dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
        }
      } else {
          log(LOG_ERROR, true, "Failed to read time from RTC during verification.\n");
      }
        
      if(retry < maxRetries - 1) {
        delay(retryDelayMs);
      }
  }
  if(success) {
      log(LOG_INFO, true, "Time successfully set to: %04d-%02d-%02d %02d:%02d:%02d\n",
                    dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
      dateTimeWriteLocked = false;
      return true;
  } else {
      log(LOG_ERROR, true, "Failed to set RTC time after maximum retries.\n");
      dateTimeWriteLocked = false;
      return false;
  }
}

/**
 * @brief Gets the current time as a thread-safe, ISO 8601 formatted string.
 * @param timeout Milliseconds to wait for the time lock.
 * @return A string in "YYYY-MM-DDTHH:MM:SSZ" format, or an empty string on timeout.
 */
String getISO8601Timestamp(uint32_t timeout) {
  DateTime now;
  if (getGlobalDateTime(now, timeout)) {
    char buf[21]; // YYYY-MM-DDTHH:MM:SSZ + null
    snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ",
         now.year, now.month, now.day, now.hour, now.minute, now.second);
    return String(buf);
  }
  return "";
}
