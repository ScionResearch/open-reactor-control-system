#pragma once

#include <Arduino.h>
#include <SPI.h>
#include "TMC5130_reg.h"

// SPI default speed 4MHz
#define TMC5130_SPI_SPEED 4000000

// NOTE - all constants below assume internal clock speed at 12.4MHz, and native microstepping (256)
// This library is designed to use the TMC5130A internal ramp generator for motion rather than external step/dir pins
// Therefore native microstepping is used.

// Velocity constant (value of VMAX for 1 step/second)
#define TMC5130_V_STEP          346.3683303         // (fclk / 2 / 2^23 / µsteps)^-1 where fclk = 12.4MHz, µsteps = 256.
                                                    // For 1 rev/sec with 200 step motor:VMAX = TMC5130_V_STEP * 200 = 69274
                                                    // VMAX = TMC5130_V_STEP * steps_per_rev * (rpm / 60)

#define TMC5130_A_STEP          0.2731326276        // (fclk^2 / (512 x 256) / 2^24 / µsteps) where fclk = 12.4MHz, µsteps = 256.
                                                    // AMAX = (rpm/s^2 / 60) / (TMC5130_A_STEP / steps_per_rev)

#define TMC5130_CLK_PERIOD_s    0.00000008064       // 1/fclk = 1/12.4MHz = 0.00000008064 s
#define TMC5130_TSTEP_PERIOD_s  0.00002064516       // Clock period x microsteps = clk_period x 256 = 0.00002064516 s
                                                    // Calculate time for 1 rev = tstep value xtstep_period x steps_per_rev
                                                    // Calculate RPM = 60 / rev_time

#define TMC5130_mA_PER_BIT_LOW_SENSITIVITY  62      // mA per LSB for Irun and Ihold with vsense = 0
#define TMC5130_mA_PER_BIT_HIGH_SENSITIVITY 34      // mA per LSB for Irun and Ihold with vsense = 1

class TMC5130 {
    public:
        // Constructor
        TMC5130(int cs_pin);
        TMC5130(int cs_pin, SPIClass *spi);

        // Initialisation
        bool begin(void);

        // Simplified configuration
        bool setStepsPerRev(uint32_t steps);        // Default 200, call before any other config if changing
        bool setMaxRPM(float rpm);                  // Used to limit stepper speed and sets threshold for CoolStep and StealthChop at 1/3rd of max RPM
        bool setIrun(uint16_t rms_mA);              // Set run current limit in mA, max 1800mA rms
        bool setIhold(uint16_t rms_mA);             // Set hold current limit in mA, max 1000mA rms
        bool setRPM(float rpm);                     // Target RPM value
        bool setAcceleration(float rpm_per_s);     // Accelleration/decelleration value in RPM/s
        bool setStealthChop(bool enable);           // Enable StealthChop mode (NOTE: Disables load feedback!)
        bool setDirection(bool forward);            // Set direction
        bool invertDirection(bool invert);          // Invert direction

        // Motion control
        bool run(void);
        bool stop(void);


        // Read write functions
        uint8_t readRegister(uint8_t reg, uint32_t *data);
        bool writeRegister(uint8_t reg, uint32_t data);

        // Structs
        struct config_t {
            float max_rpm = 200.0;
            uint32_t steps_per_rev = 200;
            uint16_t irun = 100;
            uint16_t ihold = 50;
            float rpm = 0.0;
            float accelleration = 10.0;
            bool stealth_chop = false;
            bool stall_guard2 = false;
            bool dc_step = false;
            bool spread_cycle = false;
        } config;

        struct status_t {
            float rpm;
            float load;
            bool running;
            bool stall;
            bool overTemp;
            bool openCircuit;
            bool shortCircuit;
        } status;

        // Register map and defaults - RTFM
        struct register_t {
            uint32_t GCONF = 0x00000004;            // RW   POR: 0x0 - StealthChop disabled, 0x4 - StealthChop enabled
            uint32_t GSTAT = 0;                     // R
            uint32_t IFCNT = 0;                     // R
            uint32_t NODECONF = 0;                  // W
            uint32_t IOIN = 0;                      // R
            uint32_t X_COMPARE = 0;                 // W
            uint32_t IHOLD_IRUN = 0x00050000;       // W    IHOLDDELAY default 5 (about 100ms)
            uint32_t TPOWERDOWN = 0;                // W
            uint32_t TSTEP = 0;                     // R
            uint32_t TPWMTHRS = 0;                  // W
            uint32_t TCOOLTHRS = 0;                 // W
            uint32_t THIGH = 0;                     // W
            uint32_t RAMPMODE = 1;                  // RW   default velocity mode positive direction
            uint32_t XACTUAL = 0;                   // RW
            uint32_t VACTUAL = 0;                   // R
            uint32_t VSTART = 1;                    // W    VSTART > VSTOP
            uint32_t A1 = 0;                        // W
            uint32_t V1 = 0;                        // W
            uint32_t AMAX = 0;                      // W
            uint32_t VMAX = 0;                      // W
            uint32_t DMAX = 0;                      // W
            uint32_t D1 = 0;                        // W
            uint32_t VSTOP = 0;                     // W
            uint32_t TZEROWAIT = 0x000FFF;          // W    4095 = about 150ms
            uint32_t XTARGET = 0;                   // RW
            uint32_t VDCMIN = 0;                    // W
            uint32_t SW_MODE = 0;                   // RW
            uint32_t RAMP_STAT = 0;                 // R
            uint32_t XLATCH = 0;                    // R
            uint32_t ENCMODE = 0;                   // RW
            uint32_t X_ENC = 0;                     // RW
            uint32_t ENC_CONST = 0;                 // W
            uint32_t ENC_STATUS = 0;                // R
            uint32_t ENC_LATCH = 0;                 // R
            uint32_t MSLUT_0 = 0;                   // W
            uint32_t MSLUT_1 = 0;                   // W
            uint32_t MSLUT_2 = 0;                   // W
            uint32_t MSLUT_3 = 0;                   // W
            uint32_t MSLUT_4 = 0;                   // W
            uint32_t MSLUT_5 = 0;                   // W
            uint32_t MSLUT_6 = 0;                   // W
            uint32_t MSLUT_7 = 0;                   // W
            uint32_t MSLUTSEL = 0;                  // W
            uint32_t MSLUTSTART = 0;                // W
            uint32_t MSCNT = 0;                     // R
            uint32_t MSCURACT = 0;                  // R
            uint32_t CHOPCONF = 0x0002A1B0;         // RW   POR: 0x0
            uint32_t COOLCONF = 0x00008044;         // W    POR: 0x0 - CoolStep disabled, 0x00008044 - CoolStep enabled
            uint32_t DCCTRL = 0;                    // W
            uint32_t DRV_STATUS = 0;                // R
            uint32_t PWMCONF = 0x00050480;          // W    POR: 0x00050480
            uint32_t PWM_SCALE = 0;                 // R
            uint32_t ENCM_CTRL = 0;                 // W
            uint32_t LOST_STEPS = 0;                // R
        } reg;

    private:
        int _cs_pin;
        SPIClass *_spi;
        bool _initialised;

        uint8_t ImAtoIRUN_IHOLD(uint16_t mAval, bool vsense);
        uint32_t RPMtoTSTEP(float rpm);
};