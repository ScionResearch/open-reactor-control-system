#include "objectCache.h"
#include "logger.h"

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
    
    log(LOG_DEBUG, false, "Cache: Updated object %d (type %d) = %.2f %s\n", 
        data->index, data->objectType, data->value, data->unit);
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
    
    IPC_SensorReadReq_t request;
    request.index = index;
    ipc.sendPacket(IPC_MSG_SENSOR_READ_REQ, (uint8_t*)&request, sizeof(request));
    
    log(LOG_DEBUG, false, "Cache: Requested update for object %d\n", index);
}

void ObjectCache::requestBulkUpdate(uint8_t startIndex, uint8_t count) {
    if (startIndex >= MAX_CACHED_OBJECTS || count == 0) {
        return;
    }
    
    // Clamp count to valid range
    if (startIndex + count > MAX_CACHED_OBJECTS) {
        count = MAX_CACHED_OBJECTS - startIndex;
    }
    
    // Use efficient bulk read request (single packet instead of 21 individual requests)
    IPC_SensorBulkReadReq_t request;
    request.startIndex = startIndex;
    request.count = count;
    ipc.sendPacket(IPC_MSG_SENSOR_BULK_READ_REQ, (uint8_t*)&request, sizeof(request));
    
    _lastBulkRequest = millis();
    
    log(LOG_DEBUG, false, "Cache: Requested bulk update for %d objects starting at %d\n", 
        count, startIndex);
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
        log(LOG_DEBUG, false, "Cache: Refreshed %d stale objects in range %d-%d\n",
            refreshCount, startIndex, startIndex + count - 1);
    }
}

bool ObjectCache::exists(uint8_t index) {
    if (index >= MAX_CACHED_OBJECTS) {
        return false;
    }
    return _cache[index].valid;
}

void ObjectCache::clear() {
    for (int i = 0; i < MAX_CACHED_OBJECTS; i++) {
        memset(&_cache[i], 0, sizeof(CachedObject));
        _cache[i].valid = false;
    }
}

uint8_t ObjectCache::getValidCount() {
    uint8_t count = 0;
    for (int i = 0; i < MAX_CACHED_OBJECTS; i++) {
        if (_cache[i].valid) {
            count++;
        }
    }
    return count;
}
