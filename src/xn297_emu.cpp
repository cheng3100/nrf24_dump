/*
 * XN297 emulation (NRF24L01 only) and tables.
 * From Multiprotocol/XN297_EMU.ino
 */
#include "../include/iface_xn297.h"
#include "../include/dump_types.h"
#include "../include/dump_platform.h"
#include <string.h>

#define XN297_NRF false
#define xn297_rf  XN297_NRF  /* always NRF in this build */

bool xn297_scramble_enabled;
bool xn297_crc;
bool xn297_bitrate;
uint8_t xn297_addr_len;
uint8_t xn297_rx_packet_len;
uint8_t xn297_tx_addr[5];
uint8_t xn297_rx_addr[5];

/* Exported for XN297Dump_process_packet (39 bytes, match original); extern "C" + extern for external linkage */
#ifdef __cplusplus
extern "C" {
#endif
extern const uint8_t xn297_scramble[39] = {
	0xE3, 0xB1, 0x4B, 0xEA, 0x85, 0xBC, 0xE5, 0x66,
	0x0D, 0xAE, 0x8C, 0x88, 0x12, 0x69, 0xEE, 0x1F,
	0xC7, 0x62, 0x97, 0xD5, 0x0B, 0x79, 0xCA, 0xCC,
	0x1B, 0x5D, 0x19, 0x10, 0x24, 0xD3, 0xDC, 0x3F,
	0x8E, 0xC5, 0x2F, 0xAA, 0x16, 0xF3, 0x95
};

extern const uint16_t xn297_crc_xorout_scrambled[35] = {
	0x0000, 0x3448, 0x9BA7, 0x8BBB, 0x85E1, 0x3E8C,
	0x451E, 0x18E6, 0x6B24, 0xE7AB, 0x3828, 0x814B,
	0xD461, 0xF494, 0x2503, 0x691D, 0xFE8B, 0x9BA7,
	0x8B17, 0x2920, 0x8B5F, 0x61B1, 0xD391, 0x7401,
	0x2138, 0x129F, 0xB3A0, 0x2988, 0x23CA, 0xC0CB,
	0x0C6C, 0xB329, 0xA0A1, 0x0A16, 0xA9D0
};

extern const uint16_t xn297_crc_xorout[35] = {
	0x0000, 0x3D5F, 0xA6F1, 0x3A23, 0xAA16, 0x1CAF,
	0x62B2, 0xE0EB, 0x0821, 0xBE07, 0x5F1A, 0xAF15,
	0x4F0A, 0xAD24, 0x5E48, 0xED34, 0x068C, 0xF2C9,
	0x1852, 0xDF36, 0x129D, 0xB17C, 0xD5F5, 0x70D7,
	0xB798, 0x5133, 0x67DB, 0xD94E, 0x0A5B, 0xE445,
	0xE6A5, 0x26E7, 0xBDAB, 0xC379, 0x8E20
};

extern const uint16_t xn297_crc_xorout_scrambled_enhanced[35] = {
	0x0000, 0x7EBF, 0x3ECE, 0x07A4, 0xCA52, 0x343B,
	0x53F8, 0x8CD0, 0x9EAC, 0xD0C0, 0x150D, 0x5186,
	0xD251, 0xA46F, 0x8435, 0xFA2E, 0x7EBD, 0x3C7D,
	0x94E0, 0x3D5F, 0xA685, 0x4E47, 0xF045, 0xB483,
	0x7A1F, 0xDEA2, 0x9642, 0xBF4B, 0x032F, 0x01D2,
	0xDC86, 0x92A5, 0x183A, 0xB760, 0xA953
};

extern const uint16_t xn297_crc_xorout_enhanced[35] = {
	0x0000, 0x8BE6, 0xD8EC, 0xB87A, 0x42DC, 0xAA89,
	0x83AF, 0x10E4, 0xE83E, 0x5C29, 0xAC76, 0x1C69,
	0xA4B2, 0x5961, 0xB4D3, 0x2A50, 0xCB27, 0x5128,
	0x7CDB, 0x7A14, 0xD5D2, 0x57D7, 0xE31D, 0xCE42,
	0x648D, 0xBF2D, 0x653B, 0x190C, 0x9117, 0x9A97,
	0xABFC, 0xE68E, 0x0DE7, 0x28A2, 0x1965
};
#ifdef __cplusplus
}
#endif

#define pgm_read_word(addr) (*(const uint16_t *)(addr))

extern uint8_t bit_reverse(uint8_t);
extern void crc16_update(uint8_t a, uint8_t bits);

void XN297_Configure(bool crc_en, bool scramble_en, bool bitrate, bool force_nrf)
{
	xn297_crc = crc_en;
	xn297_scramble_enabled = scramble_en;
	xn297_bitrate = bitrate;
	NRF24L01_Initialize();
	if (bitrate == XN297_250K)
		NRF24L01_SetBitrate(NRF24L01_BR_250K);
}

void XN297_SetTXAddr(const uint8_t *addr, uint8_t len)
{
	if (len > 5) len = 5;
	if (len < 3) len = 3;
	xn297_addr_len = len;
	memcpy(xn297_tx_addr, addr, len);
	uint8_t buf[] = { 0x55, 0x0F, 0x71, 0x0C, 0x00 };
	NRF24L01_WriteReg(NRF24L01_03_SETUP_AW, len - 2);
	NRF24L01_WriteRegisterMulti(NRF24L01_10_TX_ADDR, xn297_addr_len == 3 ? (uint8_t*)(buf+1) : (uint8_t*)buf, 5);
}

void XN297_SetRXAddr(const uint8_t *addr, uint8_t rx_packet_len)
{
	for (uint8_t i = 0; i < xn297_addr_len; ++i) {
		xn297_rx_addr[i] = addr[i];
		if (xn297_scramble_enabled)
			xn297_rx_addr[i] ^= xn297_scramble[xn297_addr_len - i - 1];
	}
	if (xn297_crc) rx_packet_len += 2;
	rx_packet_len += 2;
	if (rx_packet_len > 32) rx_packet_len = 32;
	NRF24L01_WriteRegisterMulti(NRF24L01_0A_RX_ADDR_P0, xn297_rx_addr, xn297_addr_len);
	NRF24L01_WriteReg(NRF24L01_11_RX_PW_P0, rx_packet_len);
	xn297_rx_packet_len = rx_packet_len;
}

void XN297_SetTxRxMode(enum TXRX_State mode)
{
	NRF24L01_WriteReg(NRF24L01_07_STATUS, (1 << NRF24L01_07_RX_DR) | (1 << NRF24L01_07_TX_DS) | (1 << NRF24L01_07_MAX_RT));
	if (mode == TXRX_OFF) {
		NRF24L01_WriteReg(NRF24L01_00_CONFIG, 0);
		dump_platform_nrf_ce_low();
		return;
	}
	dump_platform_nrf_ce_low();
	if (mode == TX_EN) {
		NRF24L01_FlushTx();
		NRF24L01_WriteReg(NRF24L01_00_CONFIG, 1 << NRF24L01_00_PWR_UP);
	} else {
		NRF24L01_FlushRx();
		NRF24L01_WriteReg(NRF24L01_00_CONFIG, (1 << NRF24L01_00_PWR_UP) | (1 << NRF24L01_00_PRIM_RX));
	}
	dump_platform_nrf_ce_high();
}

bool XN297_IsRX(void)
{
	return (NRF24L01_ReadReg(NRF24L01_07_STATUS) & _BV(NRF24L01_07_RX_DR));
}

static void XN297_ReceivePayload(uint8_t *msg, uint8_t len)
{
	if (xn297_crc) len += 2;
	NRF24L01_ReadPayload(msg, len);
}

bool XN297_ReadPayload(uint8_t *msg, uint8_t len)
{
	uint8_t buf[32];
	XN297_ReceivePayload(buf, len);
	for (uint8_t i = 0; i < len; i++) {
		uint8_t b_in = buf[i];
		if (xn297_scramble_enabled)
			b_in ^= xn297_scramble[i + xn297_addr_len];
		msg[i] = bit_reverse(b_in);
	}
	if (!xn297_crc) return true;
	crc = 0xb5d2;
	for (uint8_t i = 0; i < xn297_addr_len; ++i)
		crc16_update(xn297_rx_addr[xn297_addr_len - i - 1], 8);
	for (uint8_t i = 0; i < len; ++i)
		crc16_update(buf[i], 8);
	if (xn297_scramble_enabled)
		crc ^= xn297_crc_xorout_scrambled[xn297_addr_len - 3 + len];
	else
		crc ^= xn297_crc_xorout[xn297_addr_len - 3 + len];
	return ((crc >> 8) == buf[len] && (crc & 0xff) == buf[len + 1]);
}

uint8_t XN297_ReadEnhancedPayload(uint8_t *msg, uint8_t len)
{
	uint8_t buffer[32];
	uint8_t pcf_size;
	XN297_ReceivePayload(buffer, len + 2);
	pcf_size = buffer[0];
	if (xn297_scramble_enabled)
		pcf_size ^= xn297_scramble[xn297_addr_len];
	pcf_size >>= 1;
	if (pcf_size > 32) return 255;
	for (uint8_t i = 0; i < pcf_size; i++) {
		msg[i] = bit_reverse((buffer[i + 1] << 2) | (buffer[i + 2] >> 6));
		if (xn297_scramble_enabled)
			msg[i] ^= bit_reverse((xn297_scramble[xn297_addr_len + i + 1] << 2) | (xn297_scramble[xn297_addr_len + i + 2] >> 6));
	}
	if (!xn297_crc) return pcf_size;
	crc = 0xb5d2;
	for (uint8_t i = 0; i < xn297_addr_len; ++i)
		crc16_update(xn297_rx_addr[xn297_addr_len - i - 1], 8);
	for (uint8_t i = 0; i < pcf_size + 1; ++i)
		crc16_update(buffer[i], 8);
	crc16_update(buffer[pcf_size + 1] & 0xc0, 2);
	if (xn297_scramble_enabled)
		crc ^= xn297_crc_xorout_scrambled_enhanced[xn297_addr_len - 3 + pcf_size];
	else
		crc ^= xn297_crc_xorout_enhanced[xn297_addr_len - 3 + pcf_size];
	uint16_t crcxored = (buffer[pcf_size + 1] << 10) | (buffer[pcf_size + 2] << 2) | (buffer[pcf_size + 3] >> 6);
	return (crc == crcxored) ? pcf_size : 255;
}

void XN297_Hopping(uint8_t index)
{
	NRF24L01_WriteReg(NRF24L01_05_RF_CH, hopping_frequency[index]);
}

void XN297_RFChannel(uint8_t number)
{
	NRF24L01_WriteReg(NRF24L01_05_RF_CH, number);
}
