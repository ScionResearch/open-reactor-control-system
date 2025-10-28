#pragma once

// General library include
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <FlashStorage_SAMD.hpp>

// Task scheduler
#include "tasks/taskManager.h"

// Hardware specific
#include "hardware/pins.h"

// Drivers
#include "drivers/objects.h"
#include "drivers/onboard/drv_adc.h"
#include "drivers/onboard/drv_dac.h"
#include "drivers/onboard/drv_rtd.h"
#include "drivers/onboard/drv_gpio.h"
#include "drivers/onboard/drv_output.h"
#include "drivers/onboard/drv_stepper.h"
#include "drivers/onboard/drv_bdc_motor.h"
#include "drivers/onboard/drv_pwr_sensor.h"
#include "drivers/onboard/drv_modbus.h"
#include "drivers/peripheral/drv_modbus_hamilton_arc_common.h"
#include "drivers/peripheral/drv_modbus_hamilton_ph.h"
#include "drivers/peripheral/drv_modbus_hamilton_arc_do.h"
#include "drivers/peripheral/drv_modbus_hamilton_arc_od.h"
#include "drivers/peripheral/drv_modbus_alicat_mfc.h"
#include "drivers/peripheral/drv_analogue_pressure.h"
#include "drivers/ipc/drv_ipc.h"

// Device Manager (must be after peripheral drivers to avoid circular deps)
#include "drivers/device_manager.h"

// Utility
#include "utility/calibrate.h"
