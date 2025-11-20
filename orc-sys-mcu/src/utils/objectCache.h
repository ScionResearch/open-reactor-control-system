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
     * @brief Request bulk sensor update with explicit expected response count
     * Used for sparse object ranges where request count != response count
     * @param startIndex First object index
     * @param requestCount Number of consecutive indices to request
     * @param expectedResponses Number of valid objects expected to respond
     */
    void requestBulkUpdateSparse(uint8_t startIndex, uint8_t requestCount, uint8_t expectedResponses);
    
    /**
     * @brief Request update for stale objects in range
     */
    void refreshStaleObjects(uint8_t startIndex, uint8_t count);
    
    /**
     * @brief Check if object exists (valid flag set)
     */
    bool exists(uint8_t index);
    
    /**
     * @brief Invalidate a single cached object
     * @param index Object index to invalidate
     */
    void invalidate(uint8_t index);
    
    /**
     * @brief Invalidate a range of cached objects
     * @param startIndex First object index to invalidate
     * @param count Number of objects to invalidate
     */
    void invalidateRange(uint8_t startIndex, uint8_t count);
    
    /**
     * @brief Clear all cached data
     */
    void clear();
    
    /**
     * @brief Get number of valid objects
     */
    uint8_t getValidCount();
    
    /**
     * @brief Get number of valid objects in a specific range
     * @param startIndex First index to check
     * @param maxCount Maximum count to check (actual range is startIndex to startIndex+maxCount-1)
     * @return Number of valid objects found in the range
     */
    uint8_t getValidCountInRange(uint8_t startIndex, uint8_t maxCount);
    
private:
    CachedObject _cache[MAX_CACHED_OBJECTS];
    unsigned long _lastBulkRequest;
};

extern ObjectCache objectCache;
