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
#include "drivers/drv_adc.h"
#include "drivers/drv_dac.h"
#include "drivers/drv_rtd.h"
#include "drivers/drv_gpio.h"
#include "drivers/drv_output.h"
#include "drivers/drv_stepper.h"
#include "drivers/drv_bdc_motor.h"
#include "drivers/drv_pwr_sensor.h"
#include "drivers/drv_modbus.h"
#include "drivers/drv_modbus_hamilton_ph.h"
#include "drivers/drv_modbus_alicat_mfc.h"

// Utility
#include "utility/calibrate.h"
