#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>

#include <FlashStorage_SAMD.hpp>

#include "hardware/pins.h"

#include "drivers/objects.h"
#include "drivers/drv_adc.h"
#include "drivers/drv_dac.h"
#include "drivers/drv_rtd.h"
#include "drivers/drv_stepper.h"
#include "drivers/drv_bdc_motor.h"
#include <ModbusRTUMaster.h> // Include Modbus library

// Declare global Modbus master instance for RS485 bus 1
extern ModbusRTUMaster* modbusMaster1;

// Function Prototypes
bool modbus_init(HardwareSerial* port, long baud, int8_t rtsPin = -1);