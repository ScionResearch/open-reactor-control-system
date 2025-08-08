#ifndef IPC_DATA_STRUCTS_H
#define IPC_DATA_STRUCTS_H
#include <stdint.h>
struct TemperatureSensor {
    float celcius;
    bool online;
};
struct PHSensor {
    float pH;
    bool online;
};
struct PARSensor {
    float par;
    bool online;
};
struct LevelSensor {
    float level;
    bool online;
};
enum MessageTypes {
    MSG_TEMPERATURE_SENSOR = 2,
    MSG_PH_SENSOR = 3,
    MSG_PAR_SENSOR = 10,
    MSG_LEVEL_SENSOR = 11
};
#endif /* IPC_DATA_STRUCTS_H */
