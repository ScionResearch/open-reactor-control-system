/*
Written by J Peake - Scion Research April 2023
Driver library for the MCP3464 sigma delta 8 channel ADC. Read the datasheet and change the configuration registers as necessary.
*/

#ifndef MCP346x_h
#define MCP346x_h

#include <Arduino.h>
#include <SPI.h>

//Hardware Specific
#define MCP346X_ADDRESS_bm			0x01 << 6
#define MCP346X_uV_PER_LSB			62.5		// uV per LSB = Vref/2^15 = 2,048,000µV/32768 = 62.5µV
#define MCP346x_SPI_CLK_FREQ_MHz	10000000

//Command Modes
#define MCP346X_FAST_COMMAND_bm		0x00
#define MCP346X_STAT_READ_bm		0x01
#define MCP346X_INC_WRITE_bm		0x02
#define MCP346X_INC_READ_bm			0x03

//Fast Commands					Address: << Offset:						Description:
#define MCP346X_CNVST_bm			0x0A << 2	//						ADC Conversion Start/Restart fast command (Overwrites ADC_MODE[1:0] = 11)
#define MCP346X_STBY_bm				0x0B << 2	//						ADC Standby mode fast command (Overwrites ADC_MODE[1:0] = 10)
#define MCP346X_SHTDN_bm			0x0C << 2	//						ADC Shutdown mode fast command (Overwrites ADC_MODE[1:0] = 00)
#define MCP346X_FULL_SHTDN_bm		0x0D << 2	//						Full-Shutdown mode fast command (Overwrites CONFIG0[7:0] = 0x00)
#define MCP346X_FULL_RST_bm			0x0E << 2	//						Device Full Reset fast command (resets whole register map to default value)

//Register Addresses			Address: << Offset:	R/W:	Bits:		Description:
#define MCP346X_ADCDATA_bm			0x00 << 2	//	R		4/16/32		Latest A/D conversion data output value (16 or 32 bits depending on DATA_FORMAT[1:0]), or modulator output stream (4-bit wide) in MDAT Output mode.
#define MCP346X_CONFIG0_bm			0x01 << 2	//	R/W		8			ADC operating mode, Master Clock mode and Input Bias Current Source mode.
#define MCP346X_CONFIG1_bm			0x02 << 2	//	R/W		8			Prescale and OSR settings.
#define MCP346X_CONFIG2_bm			0x03 << 2	//	R/W		8			ADC boost and gain settings, autozeroing settings for analog multiplexer, voltage reference and ADC.
#define MCP346X_CONFIG3_bm			0x04 << 2	//	R/W		8			Conversion mode, data and CRC format settings, enable for CRC on communications, enable for digital offset and gain error calibrations.
#define MCP346X_IRQ_bm				0x05 << 2	//	R/W		8			IRQ Status bits and IRQ mode settings, enable for Fast commands and for conversion start pulse.
#define MCP346X_MUX_bm				0x06 << 2	//	R/W		8			Analog multiplexer input selection (MUX mode only).
#define MCP346X_SCAN_bm				0x07 << 2	//	R/W		24			SCAN mode settings.
#define MCP346X_TIMER_bm			0x08 << 2	//	R/W		24			Delay value for TIMER between each SCAN cycles.
#define MCP346X_OFFSETCAL_bm		0x09 << 2	//	R/W		24			ADC Digital Offset calibration value.
#define MCP346X_GAINCAL_bm			0x0A << 2	//	R/W		24			ADC Digital Gain calibration value.
#define MCP346X_LOCK_bm				0x0D << 2	//	R/W		8			Password value for SPI Write mode locking.
#define MCP346X_CRCCFG_bm			0x0F << 2	//	R		16			CRC Checksum for the device configuration.

//---------------------------------Configuration----------------------------------//

//---------------------------------CONFIG 0----------------------------------//

//Clock Select
#define MCP346X_CLK_SEL_INT_AMCLK_bm	0x3 << 4	// Internal clock is selected and AMCLK is present on the analog master clock output pin
#define MCP346X_CLK_SEL_INT_NO_EXT_bm	0x2 << 4	// Internal clock is selected and no clock output is present on the CLK pin.
#define MCP346X_CLK_SEL_EXT_bm			0x0 << 4	// External digital clock. (default)

//Current Source/Sink Selection bits for sensor bias. (Source on VIN+/Sink on VIN-)
#define MCP346X_CS_SEL_15uA_bm			0x3 << 2	// 15 ?A is applied to the ADC inputs.
#define MCP346X_CS_SEL_3_7uA_bm			0x2 << 2	// 3.7 ?A is applied to the ADC inputs.
#define MCP346X_CS_SEL_0_9uA_bm			0x1 << 2	// 0.9 ?A is applied to the ADC inputs.
#define MCP346X_CS_SEL_NONE_bm			0x0 << 2	// No current source is applied to the ADC inputs. (default)

//ADC operating mode Selection bits
#define MCP346X_ADC_MODE_CONVERT_bm		0x3 << 0	// ADC Conversion mode.
#define MCP346X_ADC_MODE_STANDBY_bm		0x2 << 0	// ADC Standby mode.
#define MCP346X_ADC_MODE_SHUTDOWN_bm	0x0 << 0	// ADC Shutdown mode.

//---------------------------------CONFIG 1----------------------------------//

//Pre-scaler value selection for analog master clock (AMCLK)
#define MCP346X_AMCLK_DIV_8_bm			0x3 << 6	// AMCLK = MCLK/8
#define MCP346X_AMCLK_DIV_4_bm			0x2 << 6	// AMCLK = MCLK/4
#define MCP346X_AMCLK_DIV_2_bm			0x1 << 6	// AMCLK = MCLK/2
#define MCP346X_AMCLK_DIV_1_bm			0x0 << 6	// AMCLK = MCLK

//Oversampling ratio for delta sigma A/D conversion
#define MCP346X_OSR_98304_bm			0xF << 2	// Over Sample Ratio 98304
#define MCP346X_OSR_81920_bm			0xE << 2	// Over Sample Ratio 81920
#define MCP346X_OSR_49152_bm			0xD << 2	// Over Sample Ratio 49152
#define MCP346X_OSR_40960_bm			0xC << 2	// Over Sample Ratio 40960
#define MCP346X_OSR_24576_bm			0xB << 2	// Over Sample Ratio 24576
#define MCP346X_OSR_20480_bm			0xA << 2	// Over Sample Ratio 20480
#define MCP346X_OSR_16384_bm			0x9 << 2	// Over Sample Ratio 16384
#define MCP346X_OSR_8192_bm				0x8 << 2	// Over Sample Ratio 8192
#define MCP346X_OSR_4096_bm				0x7 << 2	// Over Sample Ratio 4096
#define MCP346X_OSR_2048_bm				0x6 << 2	// Over Sample Ratio 2048
#define MCP346X_OSR_1024_bm				0x5 << 2	// Over Sample Ratio 1024
#define MCP346X_OSR_512_bm				0x4 << 2	// Over Sample Ratio 512
#define MCP346X_OSR_256_bm				0x3 << 2	// Over Sample Ratio 256 (default)
#define MCP346X_OSR_128_bm				0x2 << 2	// Over Sample Ratio 128
#define MCP346X_OSR_64_bm				0x1 << 2	// Over Sample Ratio 64
#define MCP346X_OSR_32_bm				0x0 << 2	// Over Sample Ratio 32

//---------------------------------CONFIG 2----------------------------------//

//ADC Bias current selection
#define MCP346X_BOOST_2_bm				0x3 << 6	// ADC channel has current x2
#define MCP346X_BOOST_1_bm				0x2 << 6	// ADC channel has current x1 (default)
#define MCP346X_BOOST_0_66_bm			0x1 << 6	// ADC channel has current x0.66
#define MCP346X_BOOST_0_5_bm			0x0 << 6	// ADC channel has current x0.5

//ADC Gain selection
#define MCP346X_GAIN_64_bm				0x7 << 3	// Gain is x64 (x16 analog, x4 digital)
#define MCP346X_GAIN_32_bm				0x6 << 3	// Gain is x32 (x16 analog, x2 digital)
#define MCP346X_GAIN_16_bm				0x5 << 3	// Gain is x16
#define MCP346X_GAIN_8_bm				0x4 << 3	// Gain is x8
#define MCP346X_GAIN_4_bm				0x3 << 3	// Gain is x4
#define MCP346X_GAIN_2_bm				0x2 << 3	// Gain is x2
#define MCP346X_GAIN_1_bm				0x1 << 3	// Gain is x1 (default)
#define MCP346X_GAIN_0_33_bm			0x0 << 3	// Gain is x1/3

//Auto-zeroing MUX setting
#define MCP346X_AZ_MUX_ENABLE_bm		0x1 << 2	// ADC auto-zeroing algorithm is enabled. This setting multiplies by two the conversion time and does not allow Continuous Conversion mode operation (which is then replaced by a series of consecutive one-shot mode conversions).
#define MCP346X_AZ_MUX_DISABLE_bm		0x0 << 2	// Analog input multiplexer auto-zeroing algorithm is disabled. (default)

//---------------------------------CONFIG 3----------------------------------//

//Conversion mode Selection
#define MCP346X_CONV_MODE_CONTINUOUS_bm		0x3 << 6	// Continuous Conversion mode or continuous conversion cycle in SCAN mode
#define MCP346X_CONV_MODE_1SHOT_STBY_bm		0x2 << 6	// One-shot conversion or one-shot cycle in SCAN mode and sets ADC_MODE[1:0] to 10 (Standby) at the end of the conversion or at the end of the conversion cycle in the SCAN mode.
#define MCP346X_CONV_MODE_1SHOT_SHUTDN_bm	0x0 << 6	// One-shot conversion or one-shot cycle in SCAN mode and sets ADC_MODE[1:0] to 0X (Shutdown) at the end of the conversion or at the end of the conversion cycle in the SCAN mode. (default).

//ADC output data format selection
#define MCP346X_DATA_FORMAT_32_CH_ID_bm		0x3 << 4	// 32-bit (17-bit right justified data plus Channel ID: CHID[3:0] plus SGN extension (12 bits) plus 16-bit ADC data. Allows over-range with the SGN extension.
#define MCP346X_DATA_FORMAT_32_SGN_bm		0x2 << 4	// 32-bit (17-bit right justified data): SGN extension (16-bit) plus 16-bit ADC data. Allows over-range with the SGN extension.
#define MCP346X_DATA_FORMAT_32_bm			0x1 << 4	// 32-bit (16-bit left justified data): 16-bit ADC data plus 0x0000 (16-bit). Does not allow over-range (ADC code locked to 0xFFFF or 0x8000).
#define MCP346X_DATA_FORMAT_16_bm			0x0 << 4	// 16-bit (default ADC coding): 16-bit ADC data. Does not allow over-range (ADC code locked to 0xFFFF or 0x8000). (default)

//CRC checksum format selection on read communications (does not affect CRCCFG coding)
#define MCP346X_CRC_FORMAT_32_bm			0x1 << 3	// CRC-16 followed by 16 zeros (32-bit format)
#define MCP346X_CRC_FORMAT_16_bm			0x0 << 3	// CRC-16 only (16-bit format) (default)

//CRC checksum selection on read communications (does not affect CRCCFG calculations)
#define MCP346X_CRC_ENABLED_bm				0x1 << 2	// CRC on communication enabled.
#define MCP346X_CRC_DISABLED_bm				0x0 << 2	// CRC on communication disabled (default)

//Enable digital offset calibration
#define MCP346X_OFFSET_CAL_ENABLED_bm		0x1 << 1	// Enable digital offset calibration.
#define MCP346X_OFFSET_CAL_DISABLED_bm		0x0 << 1	// Disable digital offset calibration.

//Enable digital gain calibration.
#define MCP346X_GAIN_CAL_ENABLED_bm			0x1 << 0	// Enable digital gain calibration.
#define MCP346X_GAIN_CAL_DISABLED_bm		0x0 << 0	// Disable digital gain calibration.

//---------------------------------IRQ----------------------------------//

//Configuration for the IRQ/MDAT pin.
#define MCP346X_IRQ_PIN_MODE_MDAT_bm		0x1 << 3	// MDAT output is selected. Only POR and CRC interrupts can be present on this pin and take priority over MDAT output.
#define MCP346X_IRQ_PIN_MODE_IRQ_bm			0x0 << 3	// IRQ output is selected. All interrupts can appear on the IRQ/MDAT pin. (default)
#define MCP346X_IRQ_PIN_STATE_HI_bm			0x1 << 2	// The inactive state is logic High. (does not require a pull-up resistor to DVDD)
#define MCP346X_IRQ_PIN_STATE_OPEN_bm		0x0 << 2	// The inactive state is High Z (requires a pull-up resistor to DVDD). (default)

//Enable fast commands in the command byte.
#define MCP346X_FAST_COMMAND_ENABLE_bm		0x1 << 1	// Fast commands are enabled. (default)
#define MCP346X_FAST_COMMAND_DISABLE_bm		0x0 << 1	// Fast commands are disabled.

//Enable Conversion start Interrupt output
#define MCP346X_CONV_START_INT_ENABLE_bm	0x1 << 0	// Enabled. (default)
#define MCP346X_CONV_START_INT_DISABLE_bm	0x0 << 0	// Disabled.

//---------------------------------MUX----------------------------------//

//Mux Input Selection
#define MCP346X_MUX_IN_INTERNAL_VCM			0xF
#define MCP346X_MUX_IN_TEMP_DIODE_M			0xE
#define MCP346X_MUX_IN_TEMP_DIODE_P			0xD
#define MCP346X_MUX_IN_REFIN_MINUS			0xC
#define MCP346X_MUX_IN_REFIN_PLUS			0xB
#define MCP346X_MUX_IN_AVDD					0x9
#define MCP346X_MUX_IN_AGND					0x8
#define MCP346X_MUX_IN_CH7					0x7
#define MCP346X_MUX_IN_CH6					0x6
#define MCP346X_MUX_IN_CH5					0x5
#define MCP346X_MUX_IN_CH4					0x4
#define MCP346X_MUX_IN_CH3					0x3
#define MCP346X_MUX_IN_CH2					0x2
#define MCP346X_MUX_IN_CH1					0x1
#define MCP346X_MUX_IN_CH0					0x0

//---------------------------------SCAN----------------------------------//

// Delay time (TDLY_SCAN) between each conversion during a SCAN cycle.
#define MCP346X_SCAN_DELAY_512_bm			0x7 << 21	// 512 * DMCLK
#define MCP346X_SCAN_DELAY_256_bm			0x5 << 21	// 256 * DMCLK
#define MCP346X_SCAN_DELAY_128_bm			0x5 << 21	// 128 * DMCLK
#define MCP346X_SCAN_DELAY_64_bm			0x4 << 21	// 64 * DMCLK
#define MCP346X_SCAN_DELAY_32_bm			0x3 << 21	// 32 * DMCLK
#define MCP346X_SCAN_DELAY_16_bm			0x2 << 21	// 16 * DMCLK
#define MCP346X_SCAN_DELAY_8_bm				0x1 << 21	// 8 * DMCLK
#define MCP346X_SCAN_DELAY_NONE_bm			0x0 << 21	// No delay. (default)

// SCAN channel selection bits
#define MCP346X_SCAN_OFFSET					0x1 << 15
#define MCP346X_SCAN_VCM					0x1 << 14
#define MCP346X_SCAN_AVDD					0x1 << 13
#define MCP346X_SCAN_TEMP					0x1 << 12
#define MCP346X_SCAN_DIF_CH_6_7				0x1 << 11
#define MCP346X_SCAN_DIF_CH_4_5				0x1 << 10
#define MCP346X_SCAN_DIF_CH_2_3				0x1 << 9
#define MCP346X_SCAN_DIF_CH_0_1				0x1 << 8
#define MCP346X_SCAN_CH_7					0x1 << 7
#define MCP346X_SCAN_CH_6					0x1 << 6
#define MCP346X_SCAN_CH_5					0x1 << 5
#define MCP346X_SCAN_CH_4					0x1 << 4
#define MCP346X_SCAN_CH_3					0x1 << 3
#define MCP346X_SCAN_CH_2					0x1 << 2
#define MCP346X_SCAN_CH_1					0x1 << 1
#define MCP346X_SCAN_CH_0					0x1 << 0
#define MCP346X_SCAN_ALL_CH					0x00FF

class MCP346x
{
	public:
		MCP346x(uint32_t cs_pin, uint32_t irq_pin, SPIClass *spi_port);
		
		struct config_type {
			uint8_t clk_sel;
			uint8_t cs_sel;
			uint8_t adc_mode;
			uint8_t amclk_div;
			uint8_t osr;
			uint8_t boost;
			uint8_t gain;
			uint8_t az_mux;
			uint8_t conv_mode;
			uint8_t data_format;
			uint8_t crc_format;
			uint8_t crc_en;
			uint8_t offset_cal_en;
			uint8_t gain_cal_en;
			uint8_t irq_pin_mode;
			uint8_t irq_pin_state;
			uint8_t fast_command_en;
			uint8_t conv_start_int_en;
			uint8_t mux_pos_input;
			uint8_t mux_neg_input;
			uint8_t scan_delay;
			uint16_t scan_channels;
			uint32_t timer;
			int16_t offset_cal;
			int16_t gain_cal;
		};

		struct device_descriptor {
			uint32_t cs_pin;
			uint32_t irq_pin;
			SPIClass *spi_port;
			struct config_type config;
			int32_t results[16];
			int32_t	microvolts[16];
			uint16_t new_data;
		};
		
		struct device_descriptor descriptor;
		
		bool begin(void);
		uint8_t write(uint8_t tx_byte);
		uint8_t write(uint8_t *tx_data, uint8_t num_bytes, uint8_t reg_addr_bm);
		int read(uint8_t *rx_buf, uint8_t num_bytes, uint8_t reg_addr_bm);
		int write_config(void);
		bool start_continuous_adc(uint16_t channels);
		bool start_single_adc(uint16_t channels);
		bool read_adc(void);
		
	private:
		void get_config_bytes(uint8_t *config_bytes);
		void adc_read_complete_ISR(void);
		static void static_ISR(void) {
			anchor->adc_read_complete_ISR();
		}		
		static MCP346x* anchor;
		bool adc_completed = false;
};

#endif //MCP346x_h