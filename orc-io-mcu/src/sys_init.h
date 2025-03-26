#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>

#include <Adafruit_MAX31865.h>
#include <FlashStorage_SAMD.hpp>

#include "MCP48FEB.h"
#include "MCP346x.h"

#include "hardware/pins.h"

#include "drivers/objects.h"

#include "drivers/drv_adc.h"
#include "drivers/drv_dac.h"
#include "drivers/drv_rtd.h"

#include "utility/calibrate.h"