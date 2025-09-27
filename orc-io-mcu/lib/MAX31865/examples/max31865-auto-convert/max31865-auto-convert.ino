/*************************************************** 
  This is a library for the Adafruit PT100/P1000 RTD Sensor w/MAX31865

  Designed specifically to work with the Adafruit RTD Sensor
  ----> https://www.adafruit.com/products/3328

  This sensor uses SPI to communicate, 4 pins are required to  
  interface
  Adafruit invests time and resources providing this open source code, 
  please support Adafruit and open-source hardware by purchasing 
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.  
  BSD license, all text above must be included in any redistribution
 ****************************************************/

 /***************************************************
  Modified library for non-blocking applications - why you ask?
  The standard 1-shot version of this library ties up the processor
  for ~75ms per read, fine for many applications and not fine for
  some! This version of the library removes dependance on the BusIO
  library, and provides new methods to grab data much faster. At
  1MHz SPI clock read speed is as low as 96µs, and at 4MHz as low
  as 54µs. As per the datasheet, the sensor can be read at up to
  60Hz, or 50Hz in 50Hz filter mode. Check for the state of the
  DRDY pin if you need super fast refresh.

  Self heating - bear in mind auto convert requires bias current to
  be on while in auto convert mode. This will heat the PT100, how
  much will depend on thermal conductivity (expressed as °C/mW).
  For example, a small PT100 with themal conductivity of 0.5 °C/mW
  will result in about 0.25 °C self heating error at room temperature.
  This effect will reduce as the temperature rises, and vis-versa.
 ****************************************************/

#include <MAX31865.h>

// use hardware SPI, just pass in the CS pin
MAX31865 thermo = MAX31865(10);

// The value of the Rref resistor. Use 430.0 for PT100 and 4300.0 for PT1000
#define RREF      430.0
// The 'nominal' 0-degrees-C resistance of the sensor
// 100.0 for PT100, 1000.0 for PT1000
#define RNOMINAL  100.0

void setup() {
  Serial.begin(115200);
  Serial.println("Adafruit MAX31865 PT100 Sensor Test - Auto-convert");

  thermo.begin(MAX31865_3WIRE);  // set to 2WIRE or 4WIRE as necessary

  // Enable auto convert
  thermo.autoConvert(true);
}


void loop() {
  uint16_t rtd = thermo.readRTDauto();

  Serial.print("RTD value: "); Serial.println(rtd);
  float ratio = rtd;
  ratio /= 32768;
  Serial.print("Ratio = "); Serial.println(ratio,8);
  Serial.print("Resistance = "); Serial.println(RREF*ratio,8);
  Serial.print("Temperature = "); Serial.println(thermo.temperature(RNOMINAL, RREF));

  // Check and print any faults
  uint8_t fault = thermo.readFault();
  if (fault) {
    Serial.print("Fault 0x"); Serial.println(fault, HEX);
    if (fault & MAX31865_FAULT_HIGHTHRESH) {
      Serial.println("RTD High Threshold"); 
    }
    if (fault & MAX31865_FAULT_LOWTHRESH) {
      Serial.println("RTD Low Threshold"); 
    }
    if (fault & MAX31865_FAULT_REFINLOW) {
      Serial.println("REFIN- > 0.85 x Bias"); 
    }
    if (fault & MAX31865_FAULT_REFINHIGH) {
      Serial.println("REFIN- < 0.85 x Bias - FORCE- open"); 
    }
    if (fault & MAX31865_FAULT_RTDINLOW) {
      Serial.println("RTDIN- < 0.85 x Bias - FORCE- open"); 
    }
    if (fault & MAX31865_FAULT_OVUV) {
      Serial.println("Under/Over voltage"); 
    }
    thermo.clearFault();
  }
  Serial.println();
  delay(1000);
}
