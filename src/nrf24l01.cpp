/*
 * NRF24L01 SPI driver (from Multiprotocol NRF24l01_SPI.ino).
 * Uses dump_platform for SPI and pins.
 */
#include <Arduino.h>
#include "../include/iface_nrf24l01.h"
#include "../include/dump_types.h"
#include "../include/dump_platform.h"

static uint8_t rf_setup;

static void NRF_CSN_off(void) { dump_platform_nrf_csn_low(); }
static void NRF_CSN_on(void)  { dump_platform_nrf_csn_high(); }

#define NRF_CE_on  dump_platform_nrf_ce_high()
#define NRF_CE_off dump_platform_nrf_ce_low()

#define SPI_Write(b)  dump_platform_spi_write((b))
#define SPI_Read()    dump_platform_spi_read()

void NRF24L01_WriteReg(uint8_t reg, uint8_t data)
{
	NRF_CSN_off;
	SPI_Write(W_REGISTER | (REGISTER_MASK & reg));
	SPI_Write(data);
	NRF_CSN_on;
}

void NRF24L01_WriteRegisterMulti(uint8_t reg, uint8_t *data, uint8_t length)
{
	NRF_CSN_off;
	SPI_Write(W_REGISTER | (REGISTER_MASK & reg));
	for (uint8_t i = 0; i < length; i++)
		SPI_Write(data[i]);
	NRF_CSN_on;
}

uint8_t NRF24L01_ReadReg(uint8_t reg)
{
	NRF_CSN_off;
	SPI_Write(R_REGISTER | (REGISTER_MASK & reg));
	uint8_t data = SPI_Read();
	NRF_CSN_on;
	return data;
}

void NRF24L01_ReadRegisterMulti(uint8_t reg, uint8_t *data, uint8_t length)
{
	NRF_CSN_off;
	SPI_Write(R_REGISTER | (REGISTER_MASK & reg));
	for (uint8_t i = 0; i < length; i++)
		data[i] = SPI_Read();
	NRF_CSN_on;
}

void NRF24L01_ReadPayload(uint8_t *data, uint8_t length)
{
	NRF_CSN_off;
	SPI_Write(R_RX_PAYLOAD);
	for (uint8_t i = 0; i < length; i++)
		data[i] = SPI_Read();
	NRF_CSN_on;
}

void NRF24L01_WritePayload(uint8_t *data, uint8_t length)
{
	NRF_CSN_off;
	SPI_Write(W_TX_PAYLOAD);
	for (uint8_t i = 0; i < length; i++)
		SPI_Write(data[i]);
	NRF_CSN_on;
}

static void NRF24L01_Strobe(uint8_t state)
{
	NRF_CSN_off;
	SPI_Write(state);
	NRF_CSN_on;
}

void NRF24L01_FlushTx(void) { NRF24L01_Strobe(FLUSH_TX); }
void NRF24L01_FlushRx(void) { NRF24L01_Strobe(FLUSH_RX); }

void NRF24L01_SetBitrate(uint8_t bitrate)
{
	rf_setup = (rf_setup & 0xD7) | ((bitrate & 0x02) << 4) | ((bitrate & 0x01) << 3);
	prev_power = (rf_setup >> 1) & 0x03;
	NRF24L01_WriteReg(NRF24L01_06_RF_SETUP, rf_setup);
}

void NRF24L01_SetPower(void)
{
	uint8_t power = 3; /* max for sniffer */
	if (prev_power != power) {
		rf_setup = (rf_setup & 0xF8) | (power << 1);
		NRF24L01_WriteReg(NRF24L01_06_RF_SETUP, rf_setup);
		prev_power = power;
	}
}

void NRF24L01_SetTxRxMode(enum TXRX_State mode)
{
	NRF24L01_WriteReg(NRF24L01_07_STATUS, (1 << NRF24L01_07_RX_DR) | (1 << NRF24L01_07_TX_DS) | (1 << NRF24L01_07_MAX_RT));
	if (mode == TX_EN) {
		NRF_CE_off;
		NRF24L01_WriteReg(NRF24L01_00_CONFIG, (1 << NRF24L01_00_EN_CRC) | (1 << NRF24L01_00_CRCO) | (1 << NRF24L01_00_PWR_UP));
		dump_platform_delay_us(130);
		NRF_CE_on;
	} else if (mode == RX_EN) {
		NRF_CE_off;
		NRF24L01_WriteReg(NRF24L01_00_CONFIG, (1 << NRF24L01_00_EN_CRC) | (1 << NRF24L01_00_CRCO) | (1 << NRF24L01_00_PWR_UP) | (1 << NRF24L01_00_PRIM_RX));
		dump_platform_delay_us(130);
		NRF_CE_on;
	} else {
		NRF24L01_WriteReg(NRF24L01_00_CONFIG, (1 << NRF24L01_00_EN_CRC));
		NRF_CE_off;
	}
}

void NRF24L01_Initialize(void)
{
	rf_setup = 0x09;
	prev_power = 0x00;
	NRF24L01_FlushTx();
	NRF24L01_FlushRx();
	NRF24L01_WriteReg(NRF24L01_01_EN_AA, 0x00);
	NRF24L01_WriteReg(NRF24L01_02_EN_RXADDR, 0x01);
	NRF24L01_WriteReg(NRF24L01_03_SETUP_AW, 0x03);
	NRF24L01_WriteReg(NRF24L01_04_SETUP_RETR, 0x00);
	NRF24L01_SetBitrate(NRF24L01_BR_1M);
	NRF24L01_WriteReg(NRF24L01_1C_DYNPD, 0x00);
	NRF24L01_WriteReg(NRF24L01_1D_FEATURE, 0x01);
	NRF24L01_SetPower();
	NRF24L01_SetTxRxMode(TX_EN);
}

uint8_t NRF24L01_Detect(void)
{
	uint8_t test_addr[5] = { 0xA5, 0x5A, 0xC3, 0x3C, 0x69 };
	uint8_t read_addr[5] = { 0 };
	
	NRF24L01_WriteRegisterMulti(NRF24L01_10_TX_ADDR, test_addr, 5);
	NRF24L01_ReadRegisterMulti(NRF24L01_10_TX_ADDR, read_addr, 5);
	
	for (uint8_t i = 0; i < 5; i++) {
		if (read_addr[i] != test_addr[i])
			return 0;
	}
	
	uint8_t status = NRF24L01_ReadReg(NRF24L01_07_STATUS);
	if (status == 0x00 || status == 0xFF)
		return 0;
	
	return 1;
}
