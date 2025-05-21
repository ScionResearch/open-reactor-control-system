#pragma once

#include <Arduino.h>
#include <Wire.h>

// Device base address
#define INA260_BASE_ADDRESS       0x40

// Device IDs
#define INA260_MANUFACTURER_ID    0x5449
#define INA260_DEVICE_ID          0x2270

// LSB values
#define INA260_LSB_VOLTAGE_mV        1.25
#define INA260_LSB_CURRENT_mA        1.25
#define INA260_LSB_POWER_mW          10

// Register address definitions
#define INA260_REG_CONFIG         0x00  // Configuration register
#define INA260_REG_CURRENT        0x01  // Shunt Current register 1.25mA/LSB
#define INA260_REG_VOLTAGE        0x02  // Bus Voltage register 1.25mV/LSB
#define INA260_REG_POWER          0x03  // Calculated power register 10mW/LSB

#define INA260_REG_MASK_ENABLE    0x06  // Mask/Enable register
#define INA260_REG_ALERT_LIMIT    0x07  // Alert Limit register

#define INA260_REG_MANUFACTURER_ID   0xFE  // Manufacturer ID register
#define INA260_REG_DEVICE_ID      0xFF  // Device ID register

// Config register bit positions
#define INA260_RESET_bp           15
#define INA260_AVG_bp             9   // AVG[2:0] -> bits 11–9
#define INA260_VBUSCT_bp          6   // VBUSCT[2:0] -> bits 8–6
#define INA260_ISHCT_bp           3   // ISHCT[2:0] -> bits 5–3
#define INA260_MODE_bp            0   // MODE[2:0]  -> bits 2–0

// Config register bit masks
#define INA260_RESET_bm           (1 << INA260_RESET_bp)

// AVG[2:0] – Averaging Mode
#define INA260_AVG_MASK           (0x7 << INA260_AVG_bp)
#define INA260_AVG_1_bm           (0x0 << INA260_AVG_bp) // Default
#define INA260_AVG_4_bm           (0x1 << INA260_AVG_bp)
#define INA260_AVG_16_bm          (0x2 << INA260_AVG_bp)
#define INA260_AVG_64_bm          (0x3 << INA260_AVG_bp)
#define INA260_AVG_128_bm         (0x4 << INA260_AVG_bp)
#define INA260_AVG_256_bm         (0x5 << INA260_AVG_bp)
#define INA260_AVG_512_bm         (0x6 << INA260_AVG_bp)
#define INA260_AVG_1024_bm        (0x7 << INA260_AVG_bp)

// VBUSCT[2:0] – Bus Voltage Conversion Time
#define INA260_VBUSCT_MASK        (0x7 << INA260_VBUSCT_bp)
#define INA260_VBUSCT_140US_bm    (0x0 << INA260_VBUSCT_bp)
#define INA260_VBUSCT_204US_bm    (0x1 << INA260_VBUSCT_bp)
#define INA260_VBUSCT_332US_bm    (0x2 << INA260_VBUSCT_bp)
#define INA260_VBUSCT_588US_bm    (0x3 << INA260_VBUSCT_bp)
#define INA260_VBUSCT_1100US_bm   (0x4 << INA260_VBUSCT_bp) // Default
#define INA260_VBUSCT_2116US_bm   (0x5 << INA260_VBUSCT_bp)
#define INA260_VBUSCT_4156US_bm   (0x6 << INA260_VBUSCT_bp)
#define INA260_VBUSCT_8244US_bm   (0x7 << INA260_VBUSCT_bp)

// ISHCT[2:0] – Shunt Current Conversion Time
#define INA260_ISHCT_MASK         (0x7 << INA260_ISHCT_bp)
#define INA260_ISHCT_140US_bm     (0x0 << INA260_ISHCT_bp)
#define INA260_ISHCT_204US_bm     (0x1 << INA260_ISHCT_bp)
#define INA260_ISHCT_332US_bm     (0x2 << INA260_ISHCT_bp)
#define INA260_ISHCT_588US_bm     (0x3 << INA260_ISHCT_bp)
#define INA260_ISHCT_1100US_bm    (0x4 << INA260_ISHCT_bp) // Default
#define INA260_ISHCT_2116US_bm    (0x5 << INA260_ISHCT_bp)
#define INA260_ISHCT_4156US_bm    (0x6 << INA260_ISHCT_bp)
#define INA260_ISHCT_8244US_bm    (0x7 << INA260_ISHCT_bp)

// MODE[2:0] – Operating Mode
#define INA260_MODE_MASK                     (0x7 << INA260_MODE_bp)
#define INA260_MODE_POWER_DOWN_bm            (0x0 << INA260_MODE_bp)
#define INA260_MODE_SHUNT_TRIGGERED_bm       (0x1 << INA260_MODE_bp)
#define INA260_MODE_BUS_TRIGGERED_bm         (0x2 << INA260_MODE_bp)
#define INA260_MODE_SHUNT_BUS_TRIGGERED_bm   (0x3 << INA260_MODE_bp)
#define INA260_MODE_SHUNT_CONTINUOUS_bm      (0x5 << INA260_MODE_bp)
#define INA260_MODE_BUS_CONTINUOUS_bm        (0x6 << INA260_MODE_bp)
#define INA260_MODE_SHUNT_BUS_CONTINUOUS_bm  (0x7 << INA260_MODE_bp) // Default

// Mask enable register bit positions
#define INA260_OCL_bp             15
#define INA260_UCL_bp             14
#define INA260_BOL_bp             13
#define INA260_BUL_bp             12
#define INA260_POL_bp             11
#define INA260_CNVR_bp            10
#define INA260_AFF_bp             4
#define INA260_CVRF_bp            3

// Mask enable register bit masks
#define INA260_OCL_bm             (1 << INA260_OCL_bp)  // Over Current Limit
#define INA260_UCL_bm             (1 << INA260_UCL_bp)  // Under Current Limit
#define INA260_BOL_bm             (1 << INA260_BOL_bp)  // Bus Voltage Over-Limit
#define INA260_BUL_bm             (1 << INA260_BUL_bp)  // Bus Voltage Under-Limit
#define INA260_POL_bm             (1 << INA260_POL_bp)  // Power Over-Limit
#define INA260_CNVR_bm            (1 << INA260_CNVR_bp) // Conversion Ready
#define INA260_AFF_bm             (1 << INA260_AFF_bp)  // Alert Function Flag (read-only)
#define INA260_CVRF_bm            (1 << INA260_CVRF_bp) // Conversion Ready Flag (read-only)

enum class INA260_MODE {
    INA260_MODE_POWER_DOWN = INA260_MODE_POWER_DOWN_bm,
    INA260_MODE_SHUNT_TRIGGERED = INA260_MODE_SHUNT_TRIGGERED_bm,
    INA260_MODE_BUS_TRIGGERED = INA260_MODE_BUS_TRIGGERED_bm,
    INA260_MODE_SHUNT_BUS_TRIGGERED = INA260_MODE_SHUNT_BUS_TRIGGERED_bm,
    INA260_MODE_SHUNT_CONTINUOUS = INA260_MODE_SHUNT_CONTINUOUS_bm,
    INA260_MODE_BUS_CONTINUOUS = INA260_MODE_BUS_CONTINUOUS_bm,
    INA260_MODE_SHUNT_BUS_CONTINUOUS = INA260_MODE_SHUNT_BUS_CONTINUOUS_bm
};

enum class INA260_AVERAGE {
    INA260_AVERAGE_1 = INA260_AVG_1_bm,
    INA260_AVERAGE_4 = INA260_AVG_4_bm,
    INA260_AVERAGE_16 = INA260_AVG_16_bm,
    INA260_AVERAGE_64 = INA260_AVG_64_bm,
    INA260_AVERAGE_128 = INA260_AVG_128_bm,
    INA260_AVERAGE_256 = INA260_AVG_256_bm,
    INA260_AVERAGE_512 = INA260_AVG_512_bm,
    INA260_AVERAGE_1024 = INA260_AVG_1024_bm
};

enum class INA260_V_CONV_TIME {
    INA260_VBUSCT_140US = INA260_VBUSCT_140US_bm,
    INA260_VBUSCT_204US = INA260_VBUSCT_204US_bm,
    INA260_VBUSCT_332US = INA260_VBUSCT_332US_bm,
    INA260_VBUSCT_588US = INA260_VBUSCT_588US_bm,
    INA260_VBUSCT_1100US = INA260_VBUSCT_1100US_bm,
    INA260_VBUSCT_2116US = INA260_VBUSCT_2116US_bm,
    INA260_VBUSCT_4156US = INA260_VBUSCT_4156US_bm,
    INA260_VBUSCT_8244US = INA260_VBUSCT_8244US_bm
};

enum class INA260_I_CONV_TIME {
    INA260_ISHCT_140US = INA260_ISHCT_140US_bm,
    INA260_ISHCT_204US = INA260_ISHCT_204US_bm,
    INA260_ISHCT_332US = INA260_ISHCT_332US_bm,
    INA260_ISHCT_588US = INA260_ISHCT_588US_bm,
    INA260_ISHCT_1100US = INA260_ISHCT_1100US_bm,
    INA260_ISHCT_2116US = INA260_ISHCT_2116US_bm,
    INA260_ISHCT_4156US = INA260_ISHCT_4156US_bm,
    INA260_ISHCT_8244US = INA260_ISHCT_8244US_bm
};

// Class definition
class INA260 {
    public:
        INA260(uint8_t I2C_dev_address = INA260_BASE_ADDRESS, TwoWire *wire = &Wire);
        INA260(uint8_t I2C_dev_address, TwoWire *wire, int irqPin);

        bool begin();
        bool reset();
        void set_irq_cb(void (*cb)());
        bool setMode(INA260_MODE mode);
        bool setAverage(INA260_AVERAGE avg);
        bool setVoltageConversionTime(INA260_V_CONV_TIME busTime);
        bool setCurrentConversionTime(INA260_I_CONV_TIME shuntTime);

        // Alert limits - only one may be active at a time, setting will overwrite previous settings
        bool setOverCurrentLimit(uint16_t mA);
        bool setUnderCurrentLimit(uint16_t mA);
        bool setOverVoltLimit(uint16_t mV);
        bool setUnderVoltLimit(uint16_t mV);
        bool setOverPowerLimit(uint16_t mW);
        bool setConversionReadyFlag();

        float amps(void);
        float volts(void);
        float watts(void);

        float milliamps(void);
        float millivolts(void);
        float milliwatts(void);

    private:
        bool _write_16(uint8_t reg_addr, uint16_t data);
        uint16_t _read_16(uint8_t reg_addr, uint16_t timeout_ms);

        uint8_t _I2C_dev_address;
        TwoWire *_wire;
        int _irqPin;
        bool _initialised;
        void (*_irq_cb)();
        bool _irq_cb_set = false;

        bool _ina260_debug = true;
};