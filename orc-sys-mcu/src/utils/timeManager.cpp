#include "timeManager.h"

MCP79410 rtc(Wire1);

// Global DateTime protection
SemaphoreHandle_t dateTimeMutex = NULL;
DateTime globalDateTime;

void init_timeManager(void) {
     // Create synchronization primitives
  dateTimeMutex = xSemaphoreCreateMutex();
  if (dateTimeMutex == NULL) {
    log(LOG_ERROR, false, "Failed to create dateTimeMutex!\n");
    while (1);
  }
  xTaskCreate(manageRTC, "RTC updt", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
}

DateTime epochToDateTime(time_t epochTime)
{
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

void manageRTC(void *param)
{
  (void)param;
  Wire1.setSDA(PIN_RTC_SDA);
  Wire1.setSCL(PIN_RTC_SCL);

  if (!rtc.begin())
  {
    log(LOG_ERROR, false, "RTC initialization failed!\n");
    return;
  }

  // Set initial time (24-hour format)
  DateTime now;
  rtc.getDateTime(&now);
  memcpy(&globalDateTime, &now, sizeof(DateTime)); // Initialize global DateTime directly
  log(LOG_INFO, false, "Current date and time is: %04d-%02d-%02d %02d:%02d:%02d\n",
                now.year, now.month, now.day, now.hour, now.minute, now.second);
                
  log(LOG_INFO, false, "RTC update task started\n");

  // Task loop
  while (1)
  {
    DateTime currentTime;
    if (rtc.getDateTime(&currentTime))
    {
      // Only update global time if it hasn't been modified recently
      if (xSemaphoreTake(dateTimeMutex, pdMS_TO_TICKS(100)) == pdTRUE)
      {
        memcpy(&globalDateTime, &currentTime, sizeof(DateTime));
        xSemaphoreGive(dateTimeMutex);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// Function to safely get the current DateTime
bool getGlobalDateTime(DateTime &dt)
{
  if (xSemaphoreTake(dateTimeMutex, pdMS_TO_TICKS(100)) == pdTRUE)
  {
    memcpy(&dt, &globalDateTime, sizeof(DateTime));
    xSemaphoreGive(dateTimeMutex);
    return true;
  }
  return false;
}

// Function to safely update the DateTime
bool updateGlobalDateTime(const DateTime &dt) {
    const int maxRetries = 3; // Maximum number of retries
    const int retryDelayMs = 100; // Delay between retries (milliseconds)

    if (xSemaphoreTake(dateTimeMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
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
              vTaskDelay(pdMS_TO_TICKS(retryDelayMs)); // Delay if retrying
            }
        }

        xSemaphoreGive(dateTimeMutex); // Release the mutex
        if(success) {
           log(LOG_INFO, true, "Time successfully set to: %04d-%02d-%02d %02d:%02d:%02d\n",
                          dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
           return true;
        } else {
            log(LOG_ERROR, true, "Failed to set RTC time after maximum retries.\n");
            return false;
        }

    } else {
        log(LOG_ERROR, true, "Failed to take dateTimeMutex in updateGlobalDateTime");
        return false;
    }
}
