#pragma once

#include <Arduino.h>
#include <SPI.h>

// SPI speed 10MHz
#define MCP48FEBxx_SPI_SPEED              10000000

// EEPROM write access wait time
#define MCP48FEBxx_EEPROM_MAX_WAIT_ms     100

// ==============================
// Register Addresses (Volatile)
// ==============================
#define MCP48FEBxx_REG_DAC0               0x00  // Volatile DAC0 Output Register
#define MCP48FEBxx_REG_DAC1               0x01  // Volatile DAC1 Output Register
#define MCP48FEBxx_REG_VREF               0x08  // Reference Voltage Control
#define MCP48FEBxx_REG_POWERDOWN          0x09  // Power-down Mode Control
#define MCP48FEBxx_REG_GAIN_STATUS        0x0A  // Gain and Status Register
#define MCP48FEBxx_REG_WIPER_LOCK         0x0B  // Wiper Lock Status

// ==============================
// Register Addresses (EEPROM)
// ==============================
#define MCP48FEBxx_REG_NV_DAC0            0x10  // Non-volatile DAC0 Output Register
#define MCP48FEBxx_REG_NV_DAC1            0x11  // Non-volatile DAC1 Output Register
#define MCP48FEBxx_REG_NV_VREF            0x18  // Non-volatile Reference Voltage
#define MCP48FEBxx_REG_NV_POWERDOWN       0x19  // Non-volatile Power-down Mode
#define MCP48FEBxx_REG_NV_GAIN            0x1A  // Non-volatile Gain Setting

// ==============================
// Bit Masks and Bit Positions
// ==============================

// Command bit position
#define MCP48FEBxx_CMD_bp                 1     // Command shifted left 1 bit in control byte
// Register bit position
#define MCP48FEBxx_REG_ADDRESS_bp         3     // Registers shifted left 3 bits in control byte

// SPI Command Bits
#define MCP48FEBxx_CMD_WRITE              0x00  // Write Command (C1:C0 = 00)
#define MCP48FEBxx_CMD_READ               0x03  // Read Command (C1:C0 = 11)
#define MCP48FEBxx_CMD_ENABLE_CONFIG      0x02  // Enable Configuration Bits (C1:C0 = 10)
#define MCP48FEBxx_CMD_DISABLE_CONFIG     0x01  // Disable Configuration Bits (C1:C0 = 01)

// DAC Data Bit Mask (12-bit DAC)
#define MCP48FEBxx_DAC_MASK               0x0FFF  // 12-bit DAC data mask

// VREF Register Bit Masks
#define MCP48FEBxx_VREF_VDD               0x0000  // Use VDD as reference
#define MCP48FEBxx_VREF_BANDGAP           0x0001  // Use Internal Band Gap (1.22V)
#define MCP48FEBxx_VREF_EXTERNAL          0x0002  // Use External VREF Pin
#define MCP48FEBxx_VREF_EXT_BUFFERED      0x0003  // Use Internal 1.25V

// VREF bit positions
#define MCP48FEBxx_VREF_0_bp              0       // Ch 0 VREF config bit position
#define MCP48FEBxx_VREF_1_bp              2       // Ch 1 VREF config bit position

// Power-Down Control Bits
#define MCP48FEBxx_PD_NORMAL              0x0000  // Normal operation
#define MCP48FEBxx_PD_1K_PULLDOWN         0x0001  // 1kΩ pull-down
#define MCP48FEBxx_PD_100K_PULLDOWN       0x0002  // 100kΩ pull-down
#define MCP48FEBxx_PD_HIGH_IMPEDANCE      0x0003  // High-Z output

// Power-Down bit positions
#define MCP48FEBxx_PD_0_bp                0       // Ch 0 power down config bit position
#define MCP48FEBxx_PD_1_bp                2       // Ch 1 power down config bit position

// Gain Control Bits
#define MCP48FEBxx_GAIN_1X                0x0000  // Gain = 1x
#define MCP48FEBxx_GAIN_2X                0x0001  // Gain = 2x

// Gain control bit positions
#define MCP48FEBxx_GAIN_0_bp              8       // Ch 0 power down config bit position
#define MCP48FEBxx_GAIN_1_bp              9       // Ch 1 power down config bit position

// Status bit positions (Gain and Status register)
#define MCP48FEBxx_POR_bp                 7       // Power On Reset status bit position (Non-volitile only)
#define MCP48FEBxx_EEWA_bp                6       // EEPROM Write Access status bit position (Non-volitile only)

// Wiper Lock status bits
#define MCP48FEBxx_WL_UNLOCKED            0x0000  // DAC wiper and DAC Configuration are unlocked (DLx = CLx = Disabled).
#define MCP48FEBxx_WL_WC_VM_LOCKED        0x0001  // DAC wiper (nonvolatile) and DAC Configuration (nonvolatile registers) are locked (DLx = Disabled; CLx = Enabled)
#define MCP48FEBxx_WL_W_C_VM_LOCKED       0x0002  // DAC wiper (volatile and nonvolatile) and DAC Configuration (nonvolatile registers) are locked (DLx = Enabled; CLx = Disabled).
#define MCP48FEBxx_WL_ALL_LOCKED          0x0003  // DAC wiper and DAC Configuration (volatile and nonvolatile registers) are locked (DLx = CLx = Enabled)

// Wiper Lock bit positions
#define MCP48FEBxx_WL_0_bp                0       // Ch 0 Wiper Lock config bit position
#define MCP48FEBxx_WL_1_bp                2       // Ch 1 Wiper Lock config bit position

// ==============================
// Enumerated Types
// ==============================
enum MCP48FEBxx_VREF {  VREF_VDD = MCP48FEBxx_VREF_VDD,
    VREF_BANDGAP = MCP48FEBxx_VREF_BANDGAP,
    VREF_EXTERNAL = MCP48FEBxx_VREF_EXTERNAL,
    VREF_EXT_BUFFERED = MCP48FEBxx_VREF_EXT_BUFFERED };

enum MCP48FEBxx_PD {  PD_NORMAL = MCP48FEBxx_PD_NORMAL,
    PD_1K_PULLDOWN = MCP48FEBxx_PD_1K_PULLDOWN,
    PD_100K_PULLDOWN = MCP48FEBxx_PD_100K_PULLDOWN,
    PD_HIGH_IMPEDANCE = MCP48FEBxx_PD_HIGH_IMPEDANCE };

enum MCP48FEBxx_GAIN {  GAIN_1X = MCP48FEBxx_GAIN_1X,
    GAIN_2X = MCP48FEBxx_GAIN_2X };

// ==============================
// Class MCP48FEB22
// ==============================
class MCP48FEBxx
{
    public:
        // Constructor
        MCP48FEBxx(int cs_pin);
        MCP48FEBxx(int cs_pin, int lat_pin, SPIClass *spi);

        // Initialisation
        bool begin(void);

        // Configuration functions
        bool setVREF(uint8_t channel, MCP48FEBxx_VREF vref);
        bool setPD(uint8_t channel, MCP48FEBxx_PD pd);
        bool setGain(uint8_t channel, MCP48FEBxx_GAIN gain);

        // Status functions
        bool getPORStatus(void);
        bool getEEWAStatus(void);

        // DAC functions
        bool writeDAC(uint8_t channel, uint16_t value);
        uint16_t readDAC(uint8_t channel);

        // Non-volatile config functions
        bool setVREF_EEPROM(uint8_t channel, MCP48FEBxx_VREF vref);
        bool setPD_EEPROM(uint8_t channel, MCP48FEBxx_PD pd);
        bool setGain_EEROM(uint8_t channel, MCP48FEBxx_GAIN gain);
        bool writeDAC_EEPROM(uint8_t channel, uint16_t value);

        int saveRegistersToEEPROM(void);     // Copy all non-volatile register values to the EEPROM - including DAC values

        // Read write functions
        bool readRegister(uint8_t reg, uint16_t *data);
        bool writeRegister(uint8_t reg, uint16_t data);

    private:
        int _dac_cs_pin;
        int _dac_lat_pin;
        SPIClass *_dac_spi;
        bool _initialised;

        // Safe wait for EEPROM write to complete (wait max of MCP48FEBxx_EEPROM_MAX_WAIT_ms)
        bool waitForEEWA(void);
};