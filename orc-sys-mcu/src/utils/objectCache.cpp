#include "objectCache.h"
#include "logger.h"
#include "ipcManager.h"  // For transaction ID management

// Global instance
ObjectCache objectCache;

// External IPC instance
extern IPCProtocol ipc;

ObjectCache::ObjectCache() : _lastBulkRequest(0) {
    clear();
}

void ObjectCache::updateObject(const IPC_SensorData_t* data) {
    if (data == nullptr || data->index >= MAX_CACHED_OBJECTS) {
        return;
    }
    
    CachedObject* obj = &_cache[data->index];
    obj->index = data->index;
    obj->objectType = data->objectType;
    obj->value = data->value;
    strncpy(obj->unit, data->unit, sizeof(obj->unit) - 1);
    obj->unit[sizeof(obj->unit) - 1] = '\0';
    obj->flags = data->flags;
    strncpy(obj->message, data->message, sizeof(obj->message) - 1);
    obj->message[sizeof(obj->message) - 1] = '\0';
    obj->lastUpdate = millis();
    obj->valid = true;
    
    // Copy additional values if present
    obj->valueCount = data->valueCount;
    if (data->valueCount > 0 && data->valueCount <= 4) {
        for (uint8_t i = 0; i < data->valueCount; i++) {
            obj->additionalValues[i] = data->additionalValues[i];
            strncpy(obj->additionalUnits[i], data->additionalUnits[i], sizeof(obj->additionalUnits[i]) - 1);
            obj->additionalUnits[i][sizeof(obj->additionalUnits[i]) - 1] = '\0';
        }
    }
}

void ObjectCache::updateObjectName(uint8_t index, const char* name, uint8_t type) {
    if (index >= MAX_CACHED_OBJECTS || name == nullptr) {
        return;
    }
    
    CachedObject* obj = &_cache[index];
    obj->index = index;
    obj->objectType = type;
    strncpy(obj->name, name, sizeof(obj->name) - 1);
    obj->name[sizeof(obj->name) - 1] = '\0';
    
    // Don't mark as valid yet - wait for actual data
}

ObjectCache::CachedObject* ObjectCache::getObject(uint8_t index) {
    if (index >= MAX_CACHED_OBJECTS) {
        return nullptr;
    }
    
    return &_cache[index];
}

bool ObjectCache::isStale(uint8_t index, unsigned long maxAge) {
    if (index >= MAX_CACHED_OBJECTS || !_cache[index].valid) {
        return true;
    }
    
    unsigned long age = millis() - _cache[index].lastUpdate;
    return age > maxAge;
}

void ObjectCache::requestUpdate(uint8_t index) {
    if (index >= MAX_CACHED_OBJECTS) {
        return;
    }
    
    // Generate transaction ID and track request
    uint16_t txnId = generateTransactionId();
    
    IPC_SensorReadReq_t request;
    request.transactionId = txnId;
    request.index = index;
    
    if (ipc.sendPacket(IPC_MSG_SENSOR_READ_REQ, (uint8_t*)&request, sizeof(request))) {
        addPendingTransaction(txnId, IPC_MSG_SENSOR_READ_REQ, IPC_MSG_SENSOR_DATA, 1, index);
    }
    
    // log(LOG_DEBUG, false, "Cache: Requested update for object %d\n", index);
}

void ObjectCache::requestBulkUpdate(uint8_t startIndex, uint8_t count) {
    if (startIndex >= MAX_CACHED_OBJECTS || count == 0) {
        return;
    }
    
    // Clamp count to valid range
    if (startIndex + count > MAX_CACHED_OBJECTS) {
        count = MAX_CACHED_OBJECTS - startIndex;
    }
    
    // Generate transaction ID and track bulk request
    uint16_t txnId = generateTransactionId();
    
    // Use efficient bulk read request (single packet instead of N individual requests)
    IPC_SensorBulkReadReq_t request;
    request.transactionId = txnId;
    request.startIndex = startIndex;
    request.count = count;
    
    if (ipc.sendPacket(IPC_MSG_SENSOR_BULK_READ_REQ, (uint8_t*)&request, sizeof(request))) {
        // Track transaction - expecting 'count' SENSOR_DATA responses
        addPendingTransaction(txnId, IPC_MSG_SENSOR_BULK_READ_REQ, IPC_MSG_SENSOR_DATA, count, startIndex);
    }
    
    _lastBulkRequest = millis();
    
    // log(LOG_DEBUG, false, "Cache: Requested bulk update for %d objects starting at %d\n", 
    //     count, startIndex);
}

void ObjectCache::requestBulkUpdateSparse(uint8_t startIndex, uint8_t requestCount, uint8_t expectedResponses) {
    if (startIndex >= MAX_CACHED_OBJECTS || requestCount == 0) {
        return;
    }
    
    // Clamp request count to valid range
    if (startIndex + requestCount > MAX_CACHED_OBJECTS) {
        requestCount = MAX_CACHED_OBJECTS - startIndex;
    }
    
    // Generate transaction ID and track bulk request
    uint16_t txnId = generateTransactionId();
    
    // Send bulk read request for the full range
    IPC_SensorBulkReadReq_t request;
    request.transactionId = txnId;
    request.startIndex = startIndex;
    request.count = requestCount;
    
    if (ipc.sendPacket(IPC_MSG_SENSOR_BULK_READ_REQ, (uint8_t*)&request, sizeof(request))) {
        // Track transaction - expecting fewer responses than requested due to sparse objects
        addPendingTransaction(txnId, IPC_MSG_SENSOR_BULK_READ_REQ, IPC_MSG_SENSOR_DATA, expectedResponses, startIndex);
    }
    
    _lastBulkRequest = millis();
}

void ObjectCache::refreshStaleObjects(uint8_t startIndex, uint8_t count) {
    if (startIndex >= MAX_CACHED_OBJECTS || count == 0) {
        return;
    }
    
    // Clamp count to valid range
    if (startIndex + count > MAX_CACHED_OBJECTS) {
        count = MAX_CACHED_OBJECTS - startIndex;
    }
    
    uint8_t refreshCount = 0;
    
    for (uint8_t i = 0; i < count; i++) {
        uint8_t index = startIndex + i;
        if (isStale(index)) {
            requestUpdate(index);
            refreshCount++;
            delay(5);  // Small delay between requests
        }
    }
    
    if (refreshCount > 0) {
        // log(LOG_DEBUG, false, "Cache: Refreshed %d stale objects in range %d-%d\n",
        //     refreshCount, startIndex, startIndex + count - 1);
    }
}

bool ObjectCache::exists(uint8_t index) {
    if (index >= MAX_CACHED_OBJECTS) {
        return false;
    }
    return _cache[index].valid;
}

void ObjectCache::invalidate(uint8_t index) {
    if (index >= MAX_CACHED_OBJECTS) {
        return;
    }
    
    _cache[index].valid = false;
    _cache[index].lastUpdate = 0;
    // Keep other fields intact for potential debugging
}

void ObjectCache::invalidateRange(uint8_t startIndex, uint8_t count) {
    if (startIndex >= MAX_CACHED_OBJECTS || count == 0) {
        return;
    }
    
    // Clamp count to valid range
    if (startIndex + count > MAX_CACHED_OBJECTS) {
        count = MAX_CACHED_OBJECTS - startIndex;
    }
    
    for (uint8_t i = 0; i < count; i++) {
        invalidate(startIndex + i);
    }
}

void ObjectCache::clear() {
    for (int i = 0; i < MAX_CACHED_OBJECTS; i++) {
        memset(&_cache[i], 0, sizeof(CachedObject));
        _cache[i].valid = false;
    }
}

uint8_t ObjectCache::getValidCount() {
    uint8_t count = 0;
    for (uint8_t i = 0; i < MAX_CACHED_OBJECTS; i++) {
        if (_cache[i].valid) {
            count++;
        }
    }
    return count;
}

uint8_t ObjectCache::getValidCountInRange(uint8_t startIndex, uint8_t maxCount) {
    if (startIndex >= MAX_CACHED_OBJECTS) {
        return 0;
    }
    
    // Clamp to valid range
    uint8_t endIndex = startIndex + maxCount;
    if (endIndex > MAX_CACHED_OBJECTS) {
        endIndex = MAX_CACHED_OBJECTS;
    }
    
    // Count valid objects in range
    uint8_t count = 0;
    for (uint8_t i = startIndex; i < endIndex; i++) {
        if (_cache[i].valid) {
            count++;
        }
    }
    return count;
}
