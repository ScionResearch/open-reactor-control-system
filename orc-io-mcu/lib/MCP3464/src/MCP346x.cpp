#include <MCP346x.h>

MCP346x* MCP346x::anchor = nullptr;
	
MCP346x::MCP346x(uint32_t cs_pin, uint32_t irq_pin, SPIClass *spi_port)
{
	anchor = this;	// Creates static handle for ISR
	
	// Default configuration, write new values before calling begin() to change
	descriptor.config.clk_sel = MCP346X_CLK_SEL_INT_NO_EXT_bm;
	descriptor.config.cs_sel = MCP346X_CS_SEL_NONE_bm;
	descriptor.config.adc_mode = MCP346X_ADC_MODE_SHUTDOWN_bm;
	descriptor.config.amclk_div = MCP346X_AMCLK_DIV_1_bm;
	descriptor.config.osr = MCP346X_OSR_98304_bm;
	descriptor.config.boost = MCP346X_BOOST_1_bm;
	descriptor.config.gain = MCP346X_GAIN_1_bm;
	descriptor.config.az_mux = MCP346X_AZ_MUX_DISABLE_bm;
	descriptor.config.conv_mode = MCP346X_CONV_MODE_CONTINUOUS_bm;
	descriptor.config.data_format = MCP346X_DATA_FORMAT_32_CH_ID_bm;
	descriptor.config.crc_format = MCP346X_CRC_FORMAT_16_bm;
	descriptor.config.crc_en = MCP346X_CRC_DISABLED_bm;
	descriptor.config.offset_cal_en = MCP346X_OFFSET_CAL_DISABLED_bm;
	descriptor.config.gain_cal_en = MCP346X_GAIN_CAL_DISABLED_bm;
	descriptor.config.irq_pin_mode = MCP346X_IRQ_PIN_MODE_IRQ_bm;
	descriptor.config.irq_pin_state = MCP346X_IRQ_PIN_STATE_HI_bm;
	descriptor.config.fast_command_en = MCP346X_FAST_COMMAND_ENABLE_bm;
	descriptor.config.conv_start_int_en = MCP346X_CONV_START_INT_DISABLE_bm;
	descriptor.config.mux_pos_input = MCP346X_MUX_IN_CH0;
	descriptor.config.mux_neg_input = MCP346X_MUX_IN_CH0;
	descriptor.config.scan_delay = MCP346X_SCAN_DELAY_NONE_bm;
	descriptor.config.scan_channels = MCP346X_SCAN_ALL_CH;
	descriptor.config.timer = 0xFFFF0;
	descriptor.config.offset_cal = 0;
	descriptor.config.gain_cal = 0;
	
	descriptor.cs_pin = cs_pin;
	descriptor.irq_pin = irq_pin;
	descriptor.spi_port = spi_port;
}

// Begin function initialises IC over SPI interface, returns true if OK, false if initialisation failed
bool MCP346x::begin(void)
{
	// Pin setup
	pinMode(descriptor.cs_pin, OUTPUT);
	pinMode(descriptor.irq_pin, INPUT);
	digitalWrite(descriptor.cs_pin, LOW);
	delay(10);
	digitalWrite(descriptor.cs_pin, HIGH);
	delay(10);
	
	// SPI init
	descriptor.spi_port->begin();
	
	// Reset MCP346x IC and write device configuration
	uint8_t cmd = {MCP346X_ADDRESS_bm | MCP346X_FULL_RST_bm | MCP346X_FAST_COMMAND_bm};
	write(cmd);
	delayMicroseconds(10);
	if (!write_config()) return false;
	attachInterrupt(digitalPinToInterrupt(descriptor.irq_pin), MCP346x::static_ISR, FALLING);
	return true;
}

// Single byte write function (sends byte as it is passed in to function), returns status byte
uint8_t MCP346x::write(uint8_t tx_byte)
{
	digitalWrite(descriptor.cs_pin, LOW);
	descriptor.spi_port->beginTransaction(SPISettings(MCP346x_SPI_CLK_FREQ_MHz, MSBFIRST, SPI_MODE0));
	uint8_t rx_byte = descriptor.spi_port->transfer(tx_byte);
	descriptor.spi_port->endTransaction();
	digitalWrite(descriptor.cs_pin, HIGH);
	return rx_byte;
}

// Multi-byte write function, required address for first byte write, returns status byte
uint8_t MCP346x::write(uint8_t *tx_data, uint8_t num_bytes, uint8_t reg_addr_bm)
{
	uint8_t reg_write_cmd = MCP346X_ADDRESS_bm | reg_addr_bm | MCP346X_INC_WRITE_bm;
	digitalWrite(descriptor.cs_pin, LOW);
	descriptor.spi_port->beginTransaction(SPISettings(MCP346x_SPI_CLK_FREQ_MHz, MSBFIRST, SPI_MODE0));
	uint8_t rx_byte = descriptor.spi_port->transfer(reg_write_cmd);
	for(int i = 0; i < num_bytes; i++) descriptor.spi_port->transfer(tx_data[i]);
	descriptor.spi_port->endTransaction();
	digitalWrite(descriptor.cs_pin, HIGH);
	return rx_byte;
}

// Multi-byte read function
int MCP346x::read(uint8_t *rx_buf, uint8_t num_bytes, uint8_t reg_addr_bm)
{
	uint8_t reg_read_cmd = MCP346X_ADDRESS_bm | reg_addr_bm | MCP346X_INC_READ_bm;
	digitalWrite(descriptor.cs_pin, LOW);
	descriptor.spi_port->beginTransaction(SPISettings(MCP346x_SPI_CLK_FREQ_MHz, MSBFIRST, SPI_MODE0));
	descriptor.spi_port->transfer(reg_read_cmd);
	for(int i = 0; i < num_bytes; i++) rx_buf[i] = descriptor.spi_port->transfer(0);
	descriptor.spi_port->endTransaction();
	digitalWrite(descriptor.cs_pin, HIGH);
	return 1;
}

// Writes the config array to MCP346x registers, reads them back. Returns 1 on success, 0 on mismatch
int MCP346x::write_config(void)
{
	uint8_t tx_data[18], rx_data[18];
	get_config_bytes(tx_data);
	write(tx_data, 18, MCP346X_CONFIG0_bm);
	read(rx_data, 18, MCP346X_CONFIG0_bm);
	for (int i = 0; i < 18; i++) {
		uint8_t err = tx_data[i] ^ rx_data[i];
		if (i == 4) err &= 0x0F;
		if (err) return 0;
	}
	return 1;
}

bool MCP346x::start_continuous_adc(uint16_t channels)
{
	if(channels) {
		uint8_t tx_data[3] = {(uint8_t)(descriptor.config.scan_delay << 4), (uint8_t)(channels >> 8), (uint8_t)channels};
		write(tx_data, 3, MCP346X_SCAN_bm);
		
		uint8_t rx_data[3];
		read(rx_data, 3, MCP346X_SCAN_bm);
		uint8_t err = 0;
		for(int i = 0; i < 3; i++) err |= rx_data[i] ^ tx_data[i];
		if(err) return false;
	}
	
	uint8_t tx_data[1] =	{	MCP346X_CONV_MODE_CONTINUOUS_bm | 
								descriptor.config.data_format |
								descriptor.config.crc_format |
								descriptor.config.crc_en |
								descriptor.config.offset_cal_en |
								descriptor.config.gain_cal_en};

	write(tx_data, 1, MCP346X_CONFIG3_bm);
	
	uint8_t rx_data[1];
	read(rx_data, 1, MCP346X_CONFIG3_bm);	
	uint8_t err = rx_data[0] ^ tx_data[0];
	if(err) return false;
	
	uint8_t cmd = MCP346X_ADDRESS_bm | MCP346X_CNVST_bm | MCP346X_FAST_COMMAND_bm;
	write(cmd);
	return true;
}

bool MCP346x::start_single_adc(uint16_t channels)
{
	if(channels) {
		uint8_t tx_data[3] = {(uint8_t)(descriptor.config.scan_delay << 4), (uint8_t)(channels >> 8), (uint8_t)channels};
		write(tx_data, 3, MCP346X_SCAN_bm);
		
		uint8_t rx_data[3];
		read(rx_data, 3, MCP346X_SCAN_bm);
		uint8_t err = 0;
		for(int i = 0; i < 3; i++) err |= rx_data[i] ^ tx_data[i];
		if(err) return false;
	}
	
	uint8_t tx_data[1] =	{	MCP346X_CONV_MODE_1SHOT_STBY_bm |
								descriptor.config.data_format |
								descriptor.config.crc_format |
								descriptor.config.crc_en |
								descriptor.config.offset_cal_en |
								descriptor.config.gain_cal_en};
	
	write(tx_data, 1, MCP346X_CONFIG3_bm);
	
	uint8_t rx_data[1];
	read(rx_data, 1, MCP346X_CONFIG3_bm);	
	uint8_t err = rx_data[0] ^ tx_data[0];
	if(err) return false;
	
	uint8_t cmd = MCP346X_ADDRESS_bm | MCP346X_CNVST_bm | MCP346X_FAST_COMMAND_bm;
	write(cmd);
	return true;
}

//----------------------------------Private Functions-------------------------------------//

void MCP346x::get_config_bytes(uint8_t *config_bytes)
{	
	config_bytes[0] =	descriptor.config.clk_sel |					//CONFIG 0
						descriptor.config.cs_sel | 
						descriptor.config.adc_mode;
						
	config_bytes[1] =	(descriptor.config.amclk_div | 				//CONFIG 1
						descriptor.config.osr) 
						& 0xFC;
						
	config_bytes[2] =	descriptor.config.boost | 					//CONFIG 2
						descriptor.config.gain | 
						descriptor.config.az_mux | 
						0x03;
						
	config_bytes[3] =	descriptor.config.conv_mode | 				//CONFIG 3
						descriptor.config.data_format | 
						descriptor.config.crc_format | 
						descriptor.config.crc_en | 
						descriptor.config.offset_cal_en | 
						descriptor.config.gain_cal_en;
						
	config_bytes[4] =	(descriptor.config.irq_pin_mode |			//IRQ
						descriptor.config.irq_pin_state |
						descriptor.config.fast_command_en |
						descriptor.config.conv_start_int_en)
						& 0x0F;
						
	config_bytes[5] =	descriptor.config.mux_pos_input << 4 |		//MUX
						descriptor.config.mux_neg_input;
						
	config_bytes[6] =	descriptor.config.scan_delay << 4;			//SCAN
	config_bytes[7] =	descriptor.config.scan_channels >> 8;	
	config_bytes[8] =	(uint8_t)descriptor.config.scan_channels;
	
	config_bytes[9] =	descriptor.config.timer >> 16;				//TIMER
	config_bytes[10] =	descriptor.config.timer >> 8;
	config_bytes[11] =	(uint8_t)descriptor.config.timer;
	
	config_bytes[12] =	descriptor.config.offset_cal >> 8;			//OFFSETCAL
	config_bytes[13] =	(uint8_t)descriptor.config.offset_cal;
	config_bytes[14] =	0x00;
	
	uint16_t gain_cal_calc = 0x8000 - descriptor.config.gain_cal;	//GAINCAL
	config_bytes[15] =	gain_cal_calc >> 8;
	config_bytes[16] =	(uint8_t)gain_cal_calc;
	config_bytes[17] =	0x00;
}

void MCP346x::adc_read_complete_ISR(void)
{
	noInterrupts();
		
	if(!digitalRead(descriptor.irq_pin)) {
		uint8_t rx_data[4];
		read(rx_data, 4, MCP346X_ADCDATA_bm);
		
		uint8_t channel = (rx_data[0] >> 4) & 0x0F;
				
		descriptor.results[channel] = (uint32_t)rx_data[2] << 8 | (uint32_t)rx_data[3];
		if(rx_data[1] && 1) descriptor.results[channel] -= 0xFFFF;
		
		descriptor.microvolts[channel] = (float)descriptor.results[channel] * MCP346X_uV_PER_LSB;
		
		descriptor.new_data |= 1 << channel;
	}
	interrupts();	
}
