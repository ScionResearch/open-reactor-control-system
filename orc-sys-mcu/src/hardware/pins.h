#pragma once

// Hardware pin definitions

// Ethernet pins
#define PIN_ETH_MISO 0
#define PIN_ETH_CS 1
#define PIN_ETH_SCK 2
#define PIN_ETH_MOSI 3
#define PIN_ETH_RST 4
#define PIN_ETH_IRQ 5

// RTC pins
#define PIN_RTC_SDA 6
#define PIN_RTC_SCL 7

// External I2C interface pins
#define PIN_I2C_EXT_SDA 8
#define PIN_I2C_EXT_SCL 9

// SD card pins
#define PIN_SDIO_CLK 10
#define PIN_SDIO_CMD 11
#define PIN_SDIO_D0 12
#define PIN_SD_SCK 10
#define PIN_SD_MOSI 11
#define PIN_SD_MISO 12
#define PIN_SD_D1 13
#define PIN_SD_D2 14
#define PIN_SD_CS 15
#define PIN_SD_CD 18

// Inter-processor communication pins
#define PIN_SI_TX 16
#define PIN_SI_RX 17

// WS2812b LED pin
#define PIN_LED_DAT 19

// Power supply voltage feedback pins
#define PIN_PS_24V_FB 26
#define PIN_PS_20V_FB 27
#define PIN_PS_5V_FB 28

// Spare GPIO pins
#define PIN_SP_IO_0 20
#define PIN_SP_IO_1 21
#define PIN_SP_IO_2 22
#define PIN_SP_IO_3 23
#define PIN_SP_IO_4 24
#define PIN_SP_IO_5 25
#define PIN_SP_IO_6 29