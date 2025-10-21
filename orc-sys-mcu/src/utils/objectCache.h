#pragma once

#include <Arduino.h>
#include "../lib/IPCprotocol/IPCProtocol.h"

#define MAX_CACHED_OBJECTS 80
#define CACHE_STALE_TIME_MS 5000  // 5 seconds

/**
 * @brief Object cache for storing sensor/output data from IO MCU
 * 
 * Maintains a local cache of object states to reduce IPC traffic and
 * provide quick responses to web UI requests. Automatically marks
 * stale data and can trigger IPC refresh requests.
 */
class ObjectCache {
public:
    struct CachedObject {
        uint8_t index;                  // Object index (0-79)
        uint8_t objectType;             // OBJ_T_xxx enum
        float value;                    // Primary value
        char unit[8];                   // Unit string (for primary value)
        char name[40];                  // Object name
        uint8_t flags;                  // IPC_SENSOR_FLAG_xxx
        char message[100];              // Fault/status message
        unsigned long lastUpdate;       // millis() of last update
        bool valid;                     // Has been initialized
        
        // Multi-value extension
        uint8_t valueCount;             // Number of additional values (0 = only primary)
        float additionalValues[4];      // Additional values
        char additionalUnits[4][8];     // Units for additional values
    };
    
    ObjectCache();
    
    /**
     * @brief Update cache with sensor data from IPC
     */
    void updateObject(const IPC_SensorData_t* data);
    
    /**
     * @brief Update cache with object name from index sync
     */
    void updateObjectName(uint8_t index, const char* name, uint8_t type);
    
    /**
     * @brief Get cached object by index
     */
    CachedObject* getObject(uint8_t index);
    
    /**
     * @brief Check if object data is stale
     */
    bool isStale(uint8_t index, unsigned long maxAge = CACHE_STALE_TIME_MS);
    
    /**
     * @brief Request single sensor update via IPC
     */
    void requestUpdate(uint8_t index);
    
    /**
     * @brief Request bulk sensor update via IPC
     */
    void requestBulkUpdate(uint8_t startIndex, uint8_t count);
    
    /**
     * @brief Request update for stale objects in range
     */
    void refreshStaleObjects(uint8_t startIndex, uint8_t count);
    
    /**
     * @brief Check if object exists (valid flag set)
     */
    bool exists(uint8_t index);
    
    /**
     * @brief Clear all cached data
     */
    void clear();
    
    /**
     * @brief Get number of valid objects
     */
    uint8_t getValidCount();
    
private:
    CachedObject _cache[MAX_CACHED_OBJECTS];
    unsigned long _lastBulkRequest;
};

extern ObjectCache objectCache;
