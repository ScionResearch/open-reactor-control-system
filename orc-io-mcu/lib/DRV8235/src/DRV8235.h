#pragma once

#include <Arduino.h>
#include <Wire.h>

// IC register defines ------------------------------------------------------|

// DRV8235 defines                          //   7      6       5       4       3       2       1       0
#define DRV8325_I2C_BASE_ADDR       0x30    
#define DRV8325_FAULT_STATUS        0x00    // FAULT  RSVD    STALL    OCP     OVP     TSD     NPOR    RSVD (Read)
#define DRV8325_RC_STATUS1          0x01    // SPEED[7:0]                             (Read)
#define DRV8325_RC_STATUS2          0x02    // RSVD
#define DRV8325_RC_STATUS3          0x03    // RSVD
#define DRV8325_REG_STATUS1         0x04    // VMTR[7:0]                              (Read)
#define DRV8325_REG_STATUS2         0x05    // IMTR[7:0]                              (Read)
#define DRV8325_REG_STATUS3         0x06    // DUTY_READ[5:0]                         (Read)
#define DRV8325_REG_STATUS4         0x07    // RSVD
#define DRV8325_REG_STATUS5         0x08    // RSVD
#define DRV8325_CONFIG0             0x09    // EN_OUT EN_OVP EN_STALL VSNS_SEL* RSVD    RSVD   CLR_FLT  DUTY_CTRL*  (Read/Write)
#define DRV8325_CONFIG1             0x0A    // TINRUSH_LSB[7:0]                       (Read/Write)
#define DRV8325_CONFIG2             0x0B    // TINRUSH_MSB[15:8]                      (Read/Write)
#define DRV8325_CONFIG3             0x0C    // IMODE[1:0]*    SMODE*  INT_VREF* TBLANK* TDEG* OCP_MODE* TSD_MODE*   (Read/Write)
#define DRV8325_CONFIG4             0x0D    // RSVD          STALL_REP CBC_REP PMODE* I2C_BC* I2C_EN_IN1 I2C_PH_IN2 (Read/Write)
#define DRV8325_REG_CTRL0           0x0E    // RSVD          EN_SS     REG_CTRL[1:0]* PWM_FREQ*  W_SCALE[1:0]       (Read/Write)
#define DRV8325_REG_CTRL1           0x0F    // WSET_VSET[7:0] 
#define DRV8325_REG_CTRL2           0x10    // OUT_FLT[1:0]               PROG_DUTY[5:0]
#define DRV8325_RC_CTRL0            0x11    // RSVD
#define DRV8325_RC_CTRL1            0x12    // RSVD
#define DRV8325_RC_CTRL2            0x13    // INV_R_SCALE[1:0] KMC_SCALE[1:0]         RSVD                         (Read/Write)
#define DRV8325_RC_CTRL3            0x14    // INV_R[7:0]                             (Read/Write)
#define DRV8325_RC_CTRL4            0x15    // KMC[7:0]                               (Read/Write)
#define DRV8325_RC_CTRL5            0x16    // RSVD
#define DRV8325_RC_CTRL6            0x17    // RSVD
#define DRV8325_RC_CTRL7            0x18    // KP_DIV[2:0]           KP_MUL[4:0]      (Read/Write)
#define DRV8325_RC_CTRL8            0x19    // KI_DIV[2:0]           KI_MUL[4:0]      (Read/Write)

// DRV8235 fault register bit positions
#define DRV8235_FAULT_bp            7       // 0b during normal operation, 1b during a fault condition. nFAULT pin is pulled down when FAULT bit is 1b. nFAULT pin is released during normal operation.
#define DRV8235_STALL_bp            5       // When this bit is 1b, it indicates motor stall.
#define DRV8235_OCP_bp              4       // 0b during normal operation, 1b if OCP event occurs.
#define DRV8235_OVP_bp              3       // 0b during normal operation, 1b if OVP event occurs.
#define DRV8235_TSD_bp              2       // 0b during normal operation, 1b if TSD event occurs.
#define DRV8235_NPOR_bp             1       // Reset and latched low if VM>VUVLO. Remains reset until the CLR_FLT bit is set to issue a clear fault command. After power up, automatically latched high once CLR_FLT command is issued

// CONFIG0 (0x09) register bit positions and values
#define DRV8235_EN_OUT_bp           7       // 0b: All driver FETs are Hi-Z. 1b: Enables the driver outputs.
#define DRV8235_EN_OVP_bp           6       // Enables the OVP feature. 1b by default, can be made 0b after power-up to disable the OVP feature.
#define DRV8235_EN_STALL_bp         5       // Enables the Stall Detection feature. Stall detection feature can be disabled by setting this bit to 0b.
#define DRV8235_VSNS_SEL_bp         4       // 0b: Use the analog low-pass filter to average out the output voltage for voltage regulation. 0b is the recommended value.
                                            // 1b: Use the digital low-pass filter for voltage regulation. This option perfroms multiplication of the duty cycle with VM to obtain the output voltage
#define DRV8235_CLR_FLT_bp          1       // Clears all latched faults when set to 1b. CLR_FLT is automatically reset.
#define DRV8235_DUTY_CTRL_bp        0       // 0b: User cannot program duty cycle manually.
                                            // 1b: When speed regulation is disabled and the DUTY_CTRL bit is 1b, user can write desired PWM duty to PROG_DUTY bits. The range of duty is 0% (000000b) to 100% (111111b).

// CONFIG3 (0x0C) register bit positions and values
#define DRV8235_IMODE_bp            6       // Determines the behavior of current regulation.
#define DRV8235_SMODE_bp            5       // Programs device response to a stall condition
#define DRV8235_INT_VREF_bp         4       // If set to 1b, sets VREF voltage to 3 V internally. Voltage is not fixed if INT_VREF is set to 0b.
#define DRV8235_TBLANK_bp           3       // Sets the current sense blanking time. If set to 0b, tBLANK=1.8µs. If set to 1b, tBLANK=1.0µs.
#define DRV8235_TDEG_bp             2       // Sets the current regulation and stall detection deglitch time. If set to 0b, tDEG=2µs. If set to 1b, tDEG=1µs
#define DRV8235_OCP_MODE_bp         1       // Programs device response to an overcurrent event. If set to 0b, device is latched off in case of an OCP event. Can be cleared using CLR_FLT.
                                            // If set to 1b, device performs auto-retry after time tretry in case of an OCP event.
#define DRV8235_TSD_MODE_bp         0       // Programs device response to an overtemperature event. If set to 0b, device is latched off in case of a TSD event. If set to 1b, device performs auto-retry when TJ<TTSD–THYS.

// CONFIG4 (0x0D) register bit positions and values
#define DRV8235_STALL_REP_bp        5       // Determines whether stall is reported on the nFAULT pin. When set to 1b, nFAULT is low whenever stall is detected. When set to 0b, stall is not reported on nFAULT output.
#define DRV8235_CBC_REP_bp          4       // When REG_CTRL is set to 01b, the device enters cycle-by-cycle mode of current regulation. In this mode, the device can indicate whenever the H-bridge enters internal current regulation.
                                            // CBC_REP bit is used to determine device outputs' behavior in the cycle-bycycle mode.
                                            // 1b: nFAULT is pulled low when H-Bridge enters internal current regulation.
                                            // 0b: nFAULT is not pulled low when H-Bridge enters internal current regulation.
#define DRV8235_PMODE_bp            3       // Switch between phase/enable mode and PWM mode. 0b: PH/EN. 1b: PWM.
#define DRV8235_I2C_BC_bp           2       // Decides the H-Bridge Control Interface. 0b: Bridge control configured by INx pins. 1b: Bridge control configured by I2C bits I2C_EN_IN1 and I2C_PH_IN2.
#define DRV8235_I2C_EN_IN1_bp       1       // Enable/PWM Input Bit 1 for internal bridge control. Used when I2C_BC=1b. Ignored when I2C_BC=0b.
#define DRV8235_I2C_PH_IN2_bp       0       // Phase/PWM Input Bit 2 for internal bridge control. Used when I2C_BC=1b.Ignored when I2C_BC=0b.

// REG_CTRL0 (0x0E) register bit positions and values
#define DRV8235_EN_SS_bp            5       // Used to enable/disable soft start/stop. 1b: Target motor voltage or speed is soft-started and soft-stopped over the duration of tINRUSH time. 0b: Soft-start/stop feature is disabled
#define DRV8235_REG_CTRL_bp         3       // Selects the current regulation scheme (fixed off-time or cycle-bycycle) or motor speed and voltage regulation.
                                            // 00b: Fixed Off-Time Current Regulation.
                                            // 01b: Cycle-By-Cycle Current Regulation.
                                            // 10b: Motor speed is regulated.
                                            // 11b: Motor voltage is regulated
#define DRV8235_PWM_FREQ_bp         2       // Sets the PWM frequency when bridge control is configured by INx bits (I2C_BC=1b). 0b: PWM frequency is set to 50kHz. 1b: PWM frequency is set to 25kHz
#define DRV8235_W_SCALE_bp          0       // Scaling factor that helps in setting the target motor current ripple speed.
                                            // 00b: 16
                                            // 01b: 32
                                            // 10b: 64
                                            // 11b: 128

#define DRV8235_FIXED_OFF_TIME_bm   0       // Fixed off-time value. The value of this register is used to set the fixed off-time value when REG_CTRL=00b
#define DRV8235_CYCLE_BY_CYCLE_bm   1 << DRV8235_REG_CTRL_bp       // Cycle-by-cycle value. The value of this register is used to set the cycle-by-cycle value when REG_CTRL=01b
#define DRV8235_SPEED_REG_bm        2 << DRV8235_REG_CTRL_bp       // Speed regulation value. The value of this register is used to set the speed regulation value when REG_CTRL=10b
#define DRV8235_VOLTAGE_REG_bm      3 << DRV8235_REG_CTRL_bp       // Voltage regulation value. The value of this register is used to set the voltage regulation value when REG_CTRL=11b

// REG_CTRL2 (0x10) register bit positions and values
#define DRV8235_OUT_FLD_bp          6       // Programs the cut-off frequency of the output voltage filtering.
                                            // 00b: 250Hz
                                            // 01b: 500Hz
                                            // 10b: 750Hz
                                            // 11b: 1000Hz
                                            // For best results, choose a cut-off frequency equal to a value at least 20 times lower than the PWM frequency. Eg, if you PWM at 20kHz, OUT_FLT=11b (1000Hz) is sufficient.
#define DRV8235_PROG_DUTY_bp        0       // When speed/voltage regulation is inactive and DUTY_CTRL is set to 1b, the user can write the desired PWM duty cycle to this register. The range of duty cycle is 0% (000000b) to 100% (111111b).


// RC_CTRL2 (0x13) register bit positions and values
#define DRV8235_INV_R_SCALE_bp      6
#define DRV8235_KMC_SCALE_bp        4

#define DRV8235_INV_R_SCALE_DEFAULT 1
#define DRV8235_INV_R_SCALE_2       0
#define DRV8235_INV_R_SCALE_64      1
#define DRV8235_INV_R_SCALE_1024    2
#define DRV8235_INV_R_SCALE_8192    3

#define DRV8235_KMC_SCALE_DEFAULT   3
#define DRV8235_KMC_SCALE_6144      0
#define DRV8235_KMC_SCALE_12288     1
#define DRV8235_KMC_SCALE_98304     2
#define DRV8235_KMC_SCALE_196608    3

// VSET limits
#define DRV8235_V_MAX                     24.0
#define DRV8235_V_LSB                     0.16733     // From the datasheet section 7.3.6.2.1
#define DRV8235_VSET_MAX                  144         // DRV8235_V_MAX / DRV8235_V_LSB
#define DRV8235_VSET_PERCENT_MUTIPLIER    1.44        // DRV8235_VSET_MAX / 100

// DRV8235 class ------------------------------------------------------------|
class DRV8235
{
    public:
        DRV8235(uint8_t I2C_dev_address = DRV8325_I2C_BASE_ADDR, TwoWire *wire = &Wire);
        DRV8235(uint8_t I2C_dev_address, TwoWire *wire, int faultPin, int currentPin);
        bool begin();

        void set_fault_cb(void (*cb)());
        uint8_t read_status(void);

        void manage(void);              // Measures the motor current

        uint16_t motorCurrent(void);    // Returns the motor current in mA as read by the current feedback pin in manage()
        uint8_t motorCurrentIC(void);   // Returns the raw current value as read by the IC
        uint8_t motorVoltageIC(void);   // Returns the raw voltage value as read by the IC
        uint8_t motorSpeedIC(void);     // Returns the raw speed value as read by the IC

        bool setSpeed(uint8_t speed);   // Set the motor voltage via percent value (0% = 0V, 100% = 24V)
        bool setVoltage(float voltage); // Set the motor voltage in Volts (reolution 0.16733V/LSB)

        bool run(void);                 // Enable the driver H bridge
        bool stop(void);                // Disable the driver H bridge
        bool direction(bool reverse);   // Set/clear the PH input

        // Fault status register flags
        bool faultActive = false;
        bool fault = false;
        bool stall = false;
        bool overVoltage = false;
        bool overCurrent = false;
        bool overTemperature = false;
        bool powerOnReset = false;
        
    private:
        uint8_t _read_byte(uint8_t reg_addr);
        bool _write_byte(uint8_t reg_addr, uint8_t data);

        uint8_t _I2C_dev_address;
        TwoWire *_wire;
        int _faultPin;
        int _currentPin;
        uint16_t _motor_current;
        bool _initialised;
        void (*_fault_cb)();
        bool _fault_cb_set = false;

        bool drv8235_debug = true;

        // Current measurement moving average (per instance)
        uint16_t _current_sample[100] = {0};
        uint8_t _current_sample_ptr = 0;
};