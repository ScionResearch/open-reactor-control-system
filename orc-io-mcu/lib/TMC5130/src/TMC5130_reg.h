// ==============================
// Register Addresses
// ==============================
// General configuration
#define TMC5130_REG_GCONF         0x00      // Read + Write
#define TMC5130_REG_GSTAT         0x01      // Read only
#define TMC5130_REG_IFCNT         0x02      // Read only
#define TMC5130_REG_NODECONF      0x03      // Write only
#define TMC5130_REG_IOIN          0x04      // Read only
#define TMC5130_REG_OUTPUT        0x04      // Write only
#define TMC5130_REG_X_COMPARE     0x05      // Write only
// Velocity Dependent Driver Feature Control Register Set
#define TMC5130_REG_IHOLD_IRUN    0x10      // Write only
#define TMC5130_REG_TPOWERDOWN    0x11      // Write only
#define TMC5130_REG_TSTEP         0x12      // Read only
#define TMC5130_REG_TPWMTHRS      0x13      // Write only
#define TMC5130_REG_TCOOLTHRS     0x14      // Write only
#define TMC5130_REG_THIGH         0x15      // Write only
// Ramp Generator Motion Control Register Set
#define TMC5130_REG_RAMPMODE      0x20      // Read + Write
#define TMC5130_REG_XACTUAL       0x21      // Read + Write
#define TMC5130_REG_VACTUAL       0x22      // Read only
#define TMC5130_REG_VSTART        0x23      // Write only
#define TMC5130_REG_A1            0x24      // Write only
#define TMC5130_REG_V1            0x25      // Write only
#define TMC5130_REG_AMAX          0x26      // Write only
#define TMC5130_REG_VMAX          0x27      // Write only
#define TMC5130_REG_DMAX          0x28      // Write only
#define TMC5130_REG_D1            0x2A      // Write only
#define TMC5130_REG_VSTOP         0x2B      // Write only
#define TMC5130_REG_TZEROWAIT     0x2C      // Write only
#define TMC5130_REG_XTARGET       0x2D      // Read + Write
// Ramp Generator Driver Feature Control Register Set
#define TMC5130_REG_VDCMIN        0x33      // Write only
#define TMC5130_REG_SW_MODE       0x34      // Read + Write
#define TMC5130_REG_RAMP_STAT     0x35      // Read only
#define TMC5130_REG_XLATCH        0x36      // Read only
// Encoder Registers
#define TMC5130_REG_ENCMODE       0x38      // Read + Write
#define TMC5130_REG_X_ENC         0x39      // Read + Write
#define TMC5130_REG_ENC_CONST     0x3A      // Write only
#define TMC5130_REG_ENC_STATUS    0x3B      // Read only
#define TMC5130_REG_ENC_LATCH     0x3C      // Read only
// Motor Driver Registers
#define TMC5130_REG_MSLUT_0       0x60      // Write only
#define TMC5130_REG_MSLUT_1       0x61      // Write only
#define TMC5130_REG_MSLUT_2       0x62      // Write only
#define TMC5130_REG_MSLUT_3       0x63      // Write only
#define TMC5130_REG_MSLUT_4       0x64      // Write only
#define TMC5130_REG_MSLUT_5       0x65      // Write only
#define TMC5130_REG_MSLUT_6       0x66      // Write only
#define TMC5130_REG_MSLUT_7       0x67      // Write only
#define TMC5130_REG_MSLUTSEL      0x68      // Write only
#define TMC5130_REG_MSLUTSTART    0x69      // Write only
#define TMC5130_REG_MSCNT         0x6A      // Read only
#define TMC5130_REG_MSCURACT      0x6B      // Read only
#define TMC5130_REG_CHOPCONF      0x6C      // Read + Write
#define TMC5130_REG_COOLCONF      0x6D      // Write only
#define TMC5130_REG_DCCTRL        0x6E      // Write only
#define TMC5130_REG_DRV_STATUS    0x6F      // Read only
#define TMC5130_REG_PWMCONF       0x70      // Write only
#define TMC5130_REG_PWM_SCALE     0x71      // Read only
#define TMC5130_REG_ENCM_CTRL     0x72      // Write only
#define TMC5130_REG_LOST_STEPS    0x73      // Read only

// ==============================
// Bit Masks and Bit Positions
// ==============================
// General configuration bitmasks REG 0x00
#define TMC5130_GCONF_I_SCALE_AIN_bm            0x00000001  // Use voltage supplied to AIN as current reference
#define TMC5130_GCONF_INTERNAL_RSENSE_bm        0x00000002  // Internal sense resistors. Use current supplied into AIN as reference for internal sense resistor
#define TMC5130_GCONF_EN_PWM_MODE_bm            0x00000004  // StealthChop voltage PWM mode enabled (depending on velocity thresholds). Switch from off to on state while in stand still, only. 
#define TMC5130_GCONF_ENC_MODE_bm               0x00000008  // Enable commutation by full step encoder (DCIN_CFG5 = ENC_A, DCEN_CFG4 = ENC_B)
#define TMC5130_GCONF_REVERSE_SHAFT_bm          0x00000010  // Inverse motor direction
#define TMC5130_GCONF_DIAG0_ERROR_bm            0x00000020  // Enable DIAG0 active on driver errors: Over temperature (ot), short to GND (s2g), DIAG0 always shows the reset-status, i.e. is active low during reset condition.
#define TMC5130_GCONF_DIAG0_OTPW_bm             0x00000040  // Enable DIAG0 active on driver over temperature prewarning (otpw)
#define TMC5130_GCONF_DIAG0_STALL_bm            0x00000080  // diag0_stall (with SD_MODE=1) 1: Enable DIAG0 active on motor stall (set TCOOLTHRS before using this feature)
                                                            // diag0_step (with SD_MODE=0) 0: DIAG0 outputs interrupt signal 1: Enable DIAG0 as STEP output (dual edge triggered steps) for external STEP/DIR driver
#define TMC5130_GCONF_DIAG1_STALL_bm            0x00000100  // diag1_stall (with SD_MODE=1) 1: Enable DIAG1 active on motor stall (set TCOOLTHRS before using this feature)
                                                            // diag1_dir (with SD_MODE=0) 0: DIAG1 outputs position compare signal 1: Enable DIAG1 as DIR output for external STEP/DIR driver
#define TMC5130_GCONF_DIAG1_INDEX_bm            0x00000200  // diag1_index (only with SD_MODE=1) 1: Enable DIAG1 active on index position (microstep look up table position 0)
#define TMC5130_GCONF_DIAG1_ONSTATE_bm          0x00000400  // diag1_onstate (only with SD_MODE=1) 1: Enable DIAG1 active when chopper is on (for the coil which is in the second half of the fullstep)
#define TMC5130_GCONF_DIAG1_STEPS_SKIPPED_bm    0x00000800  // diag1_steps_skipped (only with SD_MODE=1) 1: Enable output toggle when steps are skipped in DcStep mode (increment of LOST_STEPS). Do not enable in conjunction with other DIAG1 options.
#define TMC5130_GCONF_DIAG0_PUSH_PULL_bm        0x00001000  // diag0_int_pushpull 0: SWN_DIAG0 is open collector output (active low) 1: Enable SWN_DIAG0 push pull output (active high)
#define TMC5130_GCONF_DIAG1_PUSH_PULL_bm        0x00002000  // diag1_int_pushpull 0: SWN_DIAG1 is open collector output (active low) 1: Enable SWN_DIAG1 push pull output (active high)
#define TMC5130_GCONF_SMALL_HYSTERESIS_bm       0x00004000  // small_hysteresis 0: Hysteresis for step frequency comparison is 1/16 1: Hysteresis for step frequency comparison is 1/32
#define TMC5130_GCONF_STOP_ENABLE_bm            0x00008000  // stop_enable 0: Normal operation 1: Emergency stop: ENCA_DCIN stops the sequencer when tied high (no steps become executed by the sequencer, motor goes to standstill state).
#define TMC5130_GCONF_DIRECT_MODE_bm            0x00010000  // direct_mode
                                                            // 0: Normal operation
                                                            // 1: Motor coil currents and polarity directly programmed via serial interface: Register XTARGET (0x2D) specifies signed coil A current (bits 8..0) and coil B current (bits 24..16).
                                                            // In this mode, the current is scaled by IHOLD setting. Velocity based current regulation of StealthChop is not available in this mode. The automatic StealthChop current regulation will work only for low stepper motor velocities.
#define TMC5130_GCONF_TEST_MODE_bm              0x00020000  // test_mode 0: Normal operation 1: Enable analog test output on pin ENCN_DCO. IHOLD[1..0] selects the function of ENCN_DCO: 0…2: T120, DAC, VDDH Attention: Not for user, set to 0 for normal operation!

// Global status bit positions REG 0x01
#define TMC5130_GSTAT_RESET_bp                  1           // reset 1: Indicates that the IC has been reset since the last read access to GSTAT. All registers have been cleared to reset values.
#define TMC5130_GSTAT_DRV_ERR_bp                2           // drv_err 1: Indicates, that the driver has been shut down due to overtemperature or short circuit detection since the last read access. Read DRV_STATUS for details. The flag can only be reset when all error conditions are cleared.
#define TMC5130_GSTAT_UV_CP_bp                  3           // uv_cp 1: Indicates an undervoltage on the charge pump. The driver is disabled in this case.

// SLave configuration bit positions REG 0x03
#define TMC5130_SLAVECONF_SLAVEADDR_bp          0           // SLAVEADDR: These eight bits set the address of unit for the UART interface. The address becomes incremented by one when the external address pin NEXTADDR is active. Range: 0-253 (254 cannot be incremented), default=0
#define TMC5130_SLAVECONF_SEND_DELAY_bp         8           // SENDDELAY:
                                                            // 0, 1: 8 bit times (not allowed with multiple slaves)
                                                            // 2, 3: 3*8 bit times
                                                            // 4, 5: 5*8 bit times
                                                            // 6, 7: 7*8 bit times
                                                            // 8, 9: 9*8 bit times
                                                            // 10, 11: 11*8 bit times
                                                            // 12, 13: 13*8 bit times
                                                            // 14, 15: 15*8 bit times

//Input register bit positions REG 0x04
#define TMC5130_IOIN_REFL_STEP_bp               0           // Step input state
#define TMC5130_IOIN_REFR_DIR_bp                1           // Direction input state
#define TMC5130_IOIN_ENCB_DCEN_CFG4_bp          2           // Encoder B input state
#define TMC5130_IOIN_ENCA_DCIN_CFG5_bp          3           // Encoder A input state
#define TMC5130_IOIN_DRV_ENN_CFG6_bp            4           // Enable input state
#define TMC5130_IOIN_ENC_N_DCO_bp               5           // Encoder N input state
#define TMC5130_IOIN_SD_MODE_bp                 6           // SD_MODE (1=External step and dir source)
#define TMC5130_IOIN_SWCOMP_IN_bp               7           // Shows voltage difference of SWN and SWP. Bring DIAG outputs to high level with pushpull disabled to test the comparator
#define TMC5130_VERSION_bp                      24          // VERSION: 0x11=first version of the IC Identical numbers mean full digital compatibility

// Output register bit positions REG 0x04
#define TMC5130_IOOUT_SWP_bp                    0           // Sets the IO output pin polarity in UART mode

// IHOLD_IRUN bit positions REG 0x10
#define TMC5130_IHOLD_IRUN_IHOLD_bp             0           // IHOLD: 0-4 - Standstill current (0=1/32…31=32/32) In combination with StealthChop mode, setting IHOLD=0 allows to choose freewheeling or coil short circuit for motor stand still.
#define TMC5130_IHOLD_IRUN_IRUN_bp              8           // IRUN: 0-4 - Motor run current (0=1/32…31=32/32)  Hint: Choose sense resistors in a way, that normal IRUN is 16 to 31 for best microstep performance
#define TMC5130_IHOLD_IRUN_IHOLDDELAY_bp        16          // IHOLDDELAY: 0-3 - Controls the number of clock cycles for motor power down after a motion as soon as standstill is detected (stst=1) and TPOWERDOWN has expired.
                                                            // The smooth transition avoids a motor jerk upon power down. 0: instant power down 1..15: Delay per current reduction step in multiple of 2^18 clocks

// SW_MODE - Reference Switch & StallGuard2 Event Configuration Register REG 0x34
#define TMC5130_SW_MODE_STOP_L_ENABLE_bp        0           // 1: Enables automatic motor stop during active left reference switch input Hint: The motor restarts in case the stop switch becomes released.
#define TMC5130_SW_MODE_STOP_R_ENABLE_bp        1           // 1: Enables automatic motor stop during active right reference switch input Hint: The motor restarts in case the stop switch becomes released.
#define TMC5130_SW_MODE_POL_STOP_L_bp           2           // Sets the active polarity of the left reference switch input 0=non-inverted, high active: a high level on REFL stops the motor 1=inverted, low active: a low level on REFL stops the motor
#define TMC5130_SW_MODE_POL_STOP_R_bp           3           // Sets the active polarity of the right reference switch input 0=non-inverted, high active: a high level on REFR stops the motor 1=inverted, low active: a low level on REFR stops the motor
#define TMC5130_SW_MODE_SWAP_LR_bp              4           // Swap the left and the right reference switch input REFL and REFR
#define TMC5130_SW_MODE_LATCH_L_ACTIVE_bp       5           // 1: Activates latching of the position to XLATCH upon an active going edge on the left reference switch input REFL. Hint: Activate latch_l_active to detect any spurious stop event by reading status_latch_l.
#define TMC5130_SW_MODE_LATCH_L_INACTIVE_bp     6           // 1: Activates latching of the position to XLATCH upon an inactive going edge on the left reference switch input REFL. The active level is defined by pol_stop_l
#define TMC5130_SW_MODE_LATCH_R_ACTIVE_bp       7           // 1: Activates latching of the position to XLATCH upon an active going edge on the right reference switch input REFR. Hint: Activate latch_r_active to detect any spurious stop event by reading status_latch_r
#define TMC5130_SW_MODE_LATCH_R_INACTIVE_bp     8           // 1: Activates latching of the position to XLATCH upon an inactive going edge on the right reference switch input REFR. The active level is defined by pol_stop_r.
#define TMC5130_SW_MODE_EN_LATCH_ENCODER_bp     9           // 1: Latch encoder position to ENC_LATCH upon reference switch event
#define TMC5130_SW_MODE_SG_STOP_bp              10          // 1: Enable stop by StallGuard2 (also available in DcStep mode). Disable to release motor after stop event. Attention: Do not enable during motor spin-up, wait until the motor velocity exceeds a certain value, where StallGuard2 delivers a stable result. This velocity threshold should be programmed using TCOOLTHRS.
#define TMC5130_SW_MODE_EN_SOFTSTOP_bp          11          // 0: Hard stop 1: Soft stop
                                                            // The soft stop mode always uses the deceleration ramp settings DMAX, V1, D1, VSTOP and TZEROWAIT for stopping the motor. A stop occurs when the velocity sign matches the reference switch position (REFL for negative velocities, REFR for positive velocities) and the respective switch stop function is enabled.
                                                            // A hard stop also uses TZEROWAIT before the motor becomes released. Attention: Do not use soft stop in combination with StallGuard2

// RAMP_STAT - Ramp & Reference Switch Status Register REG 0x35
#define TMC5130_RAMP_STAT_STOP_L_bp             0           // Reference switch left status (1=active)
#define TMC5130_RAMP_STAT_STOP_R_bp             1           // Reference switch right status (1=active)
#define TMC5130_RAMP_STAT_LATCH_L_bp            2           // 1: Latch left ready (enable position latching using SW_MODE settings latch_l_active or latch_l_inactive) (Flag is cleared upon reading)
#define TMC5130_RAMP_STAT_LATCH_R_bp            3           // 1: Latch right ready (enable position latching using SW_MODE settings latch_r_active or latch_r_inactive) (Flag is cleared upon reading)
#define TMC5130_RAMP_STAT_EVENT_STOP_L_bp       4           // 1: Signals an active stop left condition due to stop switch. The stop condition and the interrupt condition can be removed by setting RAMP_MODE to hold mode or by commanding a move to the opposite direction.
                                                            // In soft_stop mode, the condition will remain active until the motor has stopped motion into the direction of the stop switch. Disabling the stop switch or the stop function also clears the flag, but the motor will continue motion.
                                                            // This bit is ORed to the interrupt output signal.
#define TMC5130_RAMP_STAT_EVENT_STOP_R_bp       5           // See above
#define TMC5130_RAMP_STAT_EVENT_STOP_SG_bp      6           // 1: Signals an active StallGuard2 stop event. Reading the register will clear the stall condition and the motor may re-start motion unless the motion controller has been stopped. (Flag and interrupt condition are cleared upon reading) This bit is ORed to the interrupt output signal
#define TMC5130_RAMP_STAT_EVENT_POS_REACHED_bp  7           // 1: Signals that the target position has been reached (position_reached becoming active). (Flag and interrupt condition are cleared upon reading) This bit is ORed to the interrupt output signal.
#define TMC5130_RAMP_STAT_VELOCITY_REACHED_bp   8           // 1: Signals that the target velocity is reached. This flag becomes set while VACTUAL and VMAX match
#define TMC5130_RAMP_STAT_POSITION_REACHED_bp   9           // 1: Signals that the target position is reached. This flag becomes set while XACTUAL and XTARGET match.
#define TMC5130_RAMP_STAT_VZERO_bp              10          // 1: Signals that the actual velocity is 0
#define TMC5130_RAMP_STAT_T_WAIT_ZERO_ACTIVE_bp 11          // 1: Signals that TZEROWAIT is active after a motor stop. During this time, the motor is in standstill.
#define TMC5130_RAMP_STAT_SECOND_MOVE_bp        12          // 1: Signals that the automatic ramp required moving back in the opposite direction, e.g. due to on-the-fly parameter change (Flag is cleared upon reading)
#define TMC5130_RAMP_STAT_STATUS_SG_bp          13          // 1: Signals an active StallGuard2 input from the CoolStep driver or from the DcStep unit, if enabled. Hint: When polling this flag, stall events may be missed – activate sg_stop to be sure not to miss the stall event.

// ENCMODE - Encoder register REG 0x38
#define TMC5130_ENCMODE_POL_A_bp                 0          // Required A polarity for an N channel event (0=neg., 1=pos.)
#define TMC5130_ENCMODE_POL_B_bp                 1          // Required B polarity for an N channel event (0=neg., 1=pos.)
#define TMC5130_ENCMODE_POL_N_bp                 2          // Defines active polarity of N (0=low active, 1=high active)
#define TMC5130_ENCMODE_IGNORE_AB_bp             3          // 0: An N event occurs only when polarities given by pol_N, pol_A and pol_B match. 1: Ignore A and B polarity for N channel event
#define TMC5130_ENCMODE_CLR_CONT_bp              4          // 1: Always latch or latch and clear X_ENC upon an N event (once per revolution, it is recommended to combine this setting with edge sensitive N event)
#define TMC5130_ENCMODE_CLR_ONCE_bp              5          // 1: Latch or latch and clear X_ENC on the next N event following the write access
#define TMC5130_ENCMODE_N_EVENT_EDGE_bp          6          // 00: N channel event is active during an active N event level, 01: N channel is valid upon active going N event, 10: N channel is valid upon inactive going N event, 11: N channel is valid upon active going and inactive going N event
#define TMC5130_ENCMODE_CLR_ENC_X_bp             8          // 0: Upon N event, X_ENC becomes latched to ENC_LATCH only 1: Latch and additionally clear encoder counter X_ENC at N-event
#define TMC5130_ENCMODE_LATCH_X_ACT_bp           9          // 1: Also latch XACTUAL position together with X_ENC. Allows latching the ramp generator position upon an N channel event as selected by pos_edge and neg_edge.
#define TMC5130_ENCMODE_ENC_SEL_DECIMAL_bp       10         // 0: Encoder prescaler divisor binary mode: Counts ENC_CONST (fractional part) /65536 1: Encoder prescaler divisor decimal mode: Counts in ENC_CONST (fractional part) /10000

// MSLUTSEL – Look up Table Segmentation Definition REG 0x68
                                                            // Width control bit coding W0…W3:
                                                            // 00: MSLUT entry 0, 1 select: -1, +0
                                                            // 01: MSLUT entry 0, 1 select: +0, +1
                                                            // 10: MSLUT entry 0, 1 select: +1, +2
                                                            // 11: MSLUT entry 0, 1 select: +2, +3
#define TMC5130_MSLUTSEL_W0_bp                  0           // LUT width select from ofs00 to ofs(X1-1)
#define TMC5130_MSLUTSEL_W1_bp                  2           // LUT width select from ofs(X1) to ofs(X2-1)
#define TMC5130_MSLUTSEL_W2_bp                  4           // LUT width select from ofs(X2) to ofs(X3-1)
#define TMC5130_MSLUTSEL_W3_bp                  6           // LUT width select from ofs(X3) to ofs255
                                                            // The sine wave look-up table can be divided into up to
                                                            // four segments using an individual step width control
                                                            // entry Wx. The segment borders are selected by X1, X2
                                                            // and X3.
                                                            // Segment 0 goes from 0 to X1-1.
                                                            // Segment 1 goes from X1 to X2-1.
                                                            // Segment 2 goes from X2 to X3-1.
                                                            // Segment 3 goes from X3 to 255.
                                                            // For defined response the values shall satisfy: 0<X1<X2<X3
#define TMC5130_MSLUTSEL_X1_bp                  8           // LUT segment 1 start
#define TMC5130_MSLUTSEL_X2_bp                  16          // LUT segment 2 start
#define TMC5130_MSLUTSEL_X3_bp                  24          // LUT segment 3 start

// CHOPCONF – Chopper Configuration REG 0x6C
#define TMC5130_CHOPCONF_TOFF0_bp               0           // TOFF off time and driver enable. Off time setting controls duration of slow decay phase NCLK= 24 + 32*TOFF b0000: Driver disable, all bridges off. b0001: 1 – use only with TBL ≥ 2. b0010 - b1111: 2 - 15. 
#define TMC5130_CHOPCONF_HSTRT0_bp              4           // chm=0: HSTRT hysteresis start value added to HEND, b000 - b111: Add 1, 2, …, 8 to hysteresis low value HEND (1/512 of this setting adds to current setting) Attention: Effective HEND+HSTRT ≤ 16. Hint: Hysteresis decrement is done each 16 clocks
                                                            // chm=1: TFD [2..0] Fast decay time setting (MSB: fd3): b0000 - b1111: Fast decay time setting TFD with NCLK= 32*TFD (b0000: slow decay only)
#define TMC5130_CHOPCONF_HEND_bp                7           // chm=0: HEND hysteresis low value, b0000 - b1111: Hysteresis is -3, -2, -1, 0, 1, …, 12 (1/512 of this setting adds to current setting) This is the hysteresis value which becomes used for the hysteresis chopper
                                                            // chm=1: OFFSET sine wave offset, b0000 b1111: Offset is -3, -2, -1, 0, 1, …, 12 This is the sine wave offset and 1/512 of the value becomes added to the absolute value of each sine wave entry
#define TMC5130_CHOPCONF_FD3_bp                 11          // chm=1: MSB of fast decay time setting TFD
#define TMC5130_CHOPCONF_DISFDCC_bp             12          // fast decay mode chm=1: disfdcc=1 disables current comparator usage for termination of the fast decay cycle
#define TMC5130_CHOPCONF_RNDTF_bp               13          // random TOFF time 0: Chopper off time is fixed as set by TOFF 1: Random mode, TOFF is random modulated by dNCLK= -24 … +6 clocks
#define TMC5130_CHOPCONF_CHM_bp                 14          // chopper mode 0: Standard mode (SpreadCycle) 1: Constant off time with fast decay time. Fast decay time is also terminated when the negative nominal current is reached. Fast decay is after on time.
#define TMC5130_CHOPCONF_TBL_bp                 15          // TBL blank time select b00 … b11: Set comparator blank time to 16, 24, 36 or 54 clocks Hint: b01 or b10 is recommended for most applications
#define TMC5130_CHOPCONF_VSENSE_bp              17          // sense resistor voltage based current scaling 0: Low sensitivity, high sense resistor voltage 1: High sensitivity, low sense resistor voltage
#define TMC5130_CHOPCONF_VHIGHFS_bp             18          // high velocity fullstep selection - This bit enables switching to fullstep, when VHIGH is exceeded. Switching takes place only at 45° position.
                                                            // The fullstep target current uses the current value from the microstep table at the 45° position
#define TMC5130_CHOPCONF_VHIGHCHM_bp            19          // high velocity chopper mode - This bit enables switching to chm=1 and fd=0, when VHIGH is exceeded. This way, a higher velocity can be achieved. Can be combined with vhighfs=1.
                                                            // If set, the TOFF setting automatically becomes doubled during high velocity operation in order to avoid doubling of the chopper frequency.
#define TMC5130_CHOPCONF_SYNC_bp                20          // SYNC PWM synchronization clock
                                                            // This register allows synchronization of the chopper for both phases of a two-phase motor in order to avoid the occurrence of a beat, especially at low motor velocities. It is automatically switched off above VHIGH.
                                                            // b0000: Chopper sync function chopSync off
                                                            // b0001 - b1111: Synchronization with fSYNC = fCLK/(sync*64) Hint: Set TOFF to a low value, so that the chopper cycle is ended before the next sync clock pulse occurs.
                                                            // Set for the double desired chopper frequency for chm=0, for the desired base chopper frequency for chm=1.
#define TMC5130_CHOPCONF_MRES_bp                24          // MRES micro step resolution
                                                            // b0000: Native 256 microstep setting. Normally use this setting with the internal motion controller.
                                                            // b0001 - b1000: 128, 64, 32, 16, 8, 4, 2, FULLSTEP Reduced microstep resolution esp. for STEP/DIR operation. The resolution gives the number of microstep entries per sine quarter wave.
                                                            // The driver automatically uses microstep positions which result in a symmetrical wave, when choosing a lower microstep resolution. step width=2^MRES [microsteps]
#define TMC5130_CHOPCONF_INTPOL_bp              28          // interpolation to 256 microsteps 1: The actual microstep resolution (MRES) becomes extrapolated to 256 microsteps for smoothest motor operation (useful for STEP/DIR operation, only)
#define TMC5130_CHOPCONF_DEDGE_bp               29          // enable double edge step pulses 1: Enable step impulse at each step edge to reduce step frequency requirement. 
#define TMC5130_CHOPCONF_DISS2G_bp              30          // short to GND protection disable 0: Short to GND protection is on 1: Short to GND protection is disabled

// COOLCONF – Smart Energy Control CoolStep and StallGuard2 REG 0x6D
#define TMC5130_COOLCONF_SEMIN_bp               0           // minimum StallGuard2 value for smart current control and smart current enable
                                                            // If the StallGuard2 result falls below SEMIN*32, the motor current becomes increased to reduce motor load angle. b0000: smart current control CoolStep off b0001 … b1111: 1 … 15
#define TMC5130_COOLCONF_SEUP_bp                5           // current up step width - Current increment steps per measured StallGuard2 value 5 seup0 b00 … b11: 1, 2, 4, 8
#define TMC5130_COOLCONF_SEMAX_bp               8           // StallGuard2 hysteresis value for smart current control
                                                            // If the StallGuard2 result is equal to or above (SEMIN+SEMAX+1)*32, the motor current becomes decreased to save energy. b0000 … b1111: 0 … 15
#define TMC5130_COOLCONF_SEDN_bp                13          // current down step speed - b00: For each 32 StallGuard2 values decrease by one, b01: For each 8 StallGuard2 values decrease by one, b10: For each 2 StallGuard2 values decrease by one, b11: For each StallGuard2 value decrease by one
#define TMC5130_COOLCONF_SEIMIN_bp              15          // minimum current for smart current control - 0: 1/2 of current setting (IRUN), 1: 1/4 of current setting (IRUN)
#define TMC5130_COOLCONF_SGT_bp                 16          // 7-bit StallGuard2 threshold value. This signed value controls StallGuard2 level for stall output and sets the optimum measurement range for readout. A lower value gives a higher sensitivity. Zero is the starting value working with most motors.
                                                            // Set to -64 to +63: A higher value makes StallGuard2 less sensitive and requires more torque to indicate a stall.
#define TMC5130_COOLCONF_SFILT_bp               24          // StallGuard2 filter enable. 0: Standard mode, high time resolution for StallGuard2. 1: Filtered mode, StallGuard2 signal updated for each four fullsteps (resp. six fullsteps for 3 phase motor) only to compensate for motor pole tolerances

// PWMCONF – VOLTAGE MODE PWM STEALTHCHOP REG 0x 70
#define TMC5130_PWMCONF_PWM_AMPL_bp             0           // 8-bit User defined amplitude (offset)
                                                            // pwm_autoscale=0: User defined PWM amplitude offset (0-255) The resulting amplitude (limited to 0…255) is: PWM_AMPL + PWM_GRAD * 256 / TSTEP
                                                            // pwm_autoscale=1: User defined maximum PWM amplitude when switching back from current chopper mode to voltage PWM mode (switch over velocity defined by TPWMTHRS).
                                                            // Do not set too low values, as the regulation cannot measure the current when the actual PWM value goes below a setting specific value. Settings above 0x40 recommended.
#define TMC5130_PWMCONF_PWM_GRAD_bp             8           // 8-bit User defined amplitude (gradient) or regulation loop gradient
                                                            // pwm_autoscale=0: Velocity dependent gradient for PWM amplitude: PWM_GRAD * 256 / TSTEP is added to PWM_AMPL
                                                            // pwm_autoscale=1: User defined maximum PWM amplitude 10 change per half wave (1 to 15)
#define TMC5130_PWMCONF_PWM_FREQ_bp             16          // 2-bit PWM frequency selection - b00: fPWM=2/1024 fCLK, b01: fPWM=2/683 fCLK, b10: fPWM=2/512 fCLK b11: fPWM=2/410 fCLK
#define TMC5130_PWMCONF_PWM_AUTOSCALE_bp        18          // PWM autoscale 0: User defined PWM amplitude. The current settings have no influence. 
                                                            // 1: Enable automatic current control Attention: When using a user defined sine wave table, the amplitude of this sine wave table should not be less than 244. Best results are obtained with 247 to 252 as peak values
#define TMC5130_PWMCONF_PWM_SYMMETRIC_bp        19          // Force symmetric PWM - 0: The PWM value may change within each PWM cycle (standard mode), 1: A symmetric PWM cycle is enforced
#define TMC5130_PWMCONF_FREEWHEEL_bp            20          // 2-bit Allows different standstill modes - Stand still option when motor current setting is zero (IHOLD=0). b00: Normal operation, b01: Freewheeling, b10: Coil shorted using LS drivers, b11: Coil shorted using HS drivers

// DRV_STATUS – StallGuard2 Value and Driver Error Flags REG 0x6F
#define TMC5130_DRV_STATUS_SG_RESULT_bp         0           // 10-bit StallGuard2 result respectively PWM on  ime for coil A in standstill for motor temperature detection
                                                            // Mechanical load measurement: The StallGuard2 result gives a means to measure mechanical motor load. A higher value means lower mechanical load. A value of 0 signals highest load. With optimum SGT setting, this is an indicator for a motor stall.
                                                            // The stall detection compares SG_RESULT to 0 to detect a stall. SG_RESULT is used as a base for CoolStep operation, by comparing it to a programmable upper and a lower limit. It is not applicable in StealthChop mode.
                                                            // SG_RESULT is ALSO applicable when DcStep is active. StallGuard2 works best with microstep operation. Temperature measurement: In standstill, no StallGuard2 result can be obtained. SG_RESULT shows the chopper on-time for motor coil A instead.
                                                            // If the motor is moved to a determined microstep position at a certain current setting, a comparison of the chopper on-time can help to get a rough estimation of motor temperature. As the motor heats up, its coil resistance rises, and the chopper on-time increases. 
#define TMC5130_DRV_STATUS_FSACTIVE_bp          15          // full step active indicator - 1: Indicates that the driver has switched to fullstep as defined by chopper mode settings and velocity thresholds. 
#define TMC5130_DRV_STATUS_CS_ACTUAL_bp         16          // 5-bit actual motor current / smart energy current - Actual current control scaling, for monitoring smart energy current scaling controlled via settings in register COOLCONF, or for monitoring the function of the automatic current scaling.
#define TMC5130_DRV_STATUS_STALLGUARD_bp        24          // StallGuard2 status - 1: Motor stall detected (SG_RESULT=0) or DcStep stall in DcStep mode.
#define TMC5130_DRV_STATUS_OT_bp                25          // overtemperature flag - 1: Overtemperature limit has been reached. Drivers become disabled until otpw is also cleared due to cooling down of the IC. The overtemperature flag is common for both bridges.
#define TMC5130_DRV_STATUS_OTPW_bp              26          // overtemperature prewarning flag - 1: Overtemperature pre-warning threshold is exceeded. The overtemperature pre-warning flag is common for both bridges
#define TMC5130_DRV_STATUS_S2GA_bp              27          // short to ground indicator phase A - 1: Short to GND detected on phase A or B. The driver becomes disabled. The flags stay active, until the driver is disabled by software (TOFF=0) or by the ENN in
#define TMC5130_DRV_STATUS_S2GB_bp              28          // short to ground indicator phase B - 1: As above
#define TMC5130_DRV_STATUS_OLA_bp               29          // open load indicator phase A - 1: Open load detected on phase A or B. Hint: This is just an informative flag. The driver takes no action upon it. False detection may occur in fast motion and standstill. Check during slow motion, only.
#define TMC5130_DRV_STATUS_OLB_bp               30          // open load indicator phase B - 1: As above
#define TMC5130_DRV_STATUS_STST_bp              31          // standstill indicator - This flag indicates motor stand still in each operation mode. This occurs 2^20 clocks after the last step pulse.