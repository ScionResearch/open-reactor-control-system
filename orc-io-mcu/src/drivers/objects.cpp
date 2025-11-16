#include "objects.h"

// Object index array
ObjectIndex_t objIndex[MAX_NUM_OBJECTS];
int numObjects = 0;

// ============================================================================
// OBJECT INDEX MANAGEMENT
// =============================================================================

/**
 * @brief Count all valid objects in the object index array
 * @return Number of valid objects
 */
int countValidObjects() {
    int count = 0;
    for (int i = 0; i < MAX_NUM_OBJECTS; i++) {
        if (objIndex[i].valid) {
            count++;
        }
    }
    return count;
}

/**
 * @brief Update the global numObjects counter by counting valid entries
 * @return The updated object count
 */
int updateObjectCount() {
    numObjects = countValidObjects();
    Serial.printf("[OBJ] Updated object count: %d valid objects found\n", numObjects);
    return numObjects;
}