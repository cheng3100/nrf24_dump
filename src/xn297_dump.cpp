/*
 * XN297Dump - NRF24L01 RC sniffer (from Multiprotocol/XN297Dump_nrf24l01.ino).
 * Standalone: only sub_protocol 0/1/2 (250K/1M/2M) with channel scan or fixed channel.
 */
#include "../include/dump_config.h"
#include "../include/dump_platform.h"
#include "../include/dump_types.h"
#include "../include/iface_nrf24l01.h"
#include "../include/iface_xn297.h"
#include "../include/xn297_tables.h"
#include <string.h>
#include <stdio.h>

#define XN297DUMP_PERIOD_SCAN    50000
#define XN297DUMP_MAX_RF_CHANNEL 84
#define XN297DUMP_MAX_PACKET_LEN 32
#define XN297DUMP_CRC_LENGTH     2

#define debug  dump_platform_debug
#define debugln dump_platform_debugln

static uint16_t timeH;
static uint8_t  address_length;
static uint8_t  bitrate;
static uint8_t  old_option;
static bool     scramble, enhanced, ack;
static uint8_t  pid;

#define pgm_read_word(addr) (*(const uint16_t *)(addr))

extern uint8_t bit_reverse(uint8_t);
extern void crc16_update(uint8_t a, uint8_t bits);

static bool XN297Dump_process_packet(void)
{
	uint16_t crcxored;
	uint8_t packet_sc[XN297DUMP_MAX_PACKET_LEN], packet_un[XN297DUMP_MAX_PACKET_LEN];
	enhanced = false;
	crc = 0xb5d2;

	for (uint8_t i = 0; i < address_length; i++) {
		crc16_update(packet[i], 8);
		packet_un[address_length - 1 - i] = packet[i];
		packet_sc[address_length - 1 - i] = packet[i] ^ xn297_scramble[i];
	}
	for (uint8_t i = address_length; i < XN297DUMP_MAX_PACKET_LEN - XN297DUMP_CRC_LENGTH; i++) {
		crc16_update(packet[i], 8);
		packet_sc[i] = bit_reverse(packet[i] ^ xn297_scramble[i]);
		packet_un[i] = bit_reverse(packet[i]);
		crcxored = crc ^ pgm_read_word(&xn297_crc_xorout[i + 1 - 3]);
		if ((crcxored >> 8) == packet[i + 1] && (crcxored & 0xff) == packet[i + 2]) {
			packet_length = i + 1;
			memcpy(packet, packet_un, packet_length);
			scramble = false;
			return true;
		}
		crcxored = crc ^ pgm_read_word(&xn297_crc_xorout_scrambled[i + 1 - 3]);
		if ((crcxored >> 8) == packet[i + 1] && (crcxored & 0xff) == packet[i + 2]) {
			packet_length = i + 1;
			memcpy(packet, packet_sc, packet_length);
			scramble = true;
			return true;
		}
	}

	uint16_t crc_save = 0xb5d2;
	packet_length = 0;
	for (uint8_t i = 0; i < XN297DUMP_MAX_PACKET_LEN - XN297DUMP_CRC_LENGTH; i++) {
		packet_sc[i] = packet[i] ^ xn297_scramble[i];
		crc = crc_save;
		crc16_update(packet[i], 8);
		crc_save = crc;
		crc16_update(packet[i + 1] & 0xC0, 2);
		crcxored = (packet[i + 1] << 10) | (packet[i + 2] << 2) | (packet[i + 3] >> 6);
		if (i >= 3) {
			if ((crc ^ pgm_read_word(&xn297_crc_xorout_scrambled_enhanced[i - 3])) == crcxored) {
				packet_length = i;
				scramble = true;
				i++;
				packet_sc[i] = packet[i] ^ xn297_scramble[i];
				memcpy(packet_un, packet_sc, packet_length + 2);
				break;
			}
			if ((crc ^ pgm_read_word(&xn297_crc_xorout_enhanced[i - 3])) == crcxored) {
				packet_length = i;
				scramble = false;
				memcpy(packet_un, packet, packet_length + 2);
				break;
			}
		}
	}
	if (packet_length != 0) {
		enhanced = true;
		if ((packet_un[address_length] >> 1) != packet_length - address_length) {
			for (uint8_t i = 3; i <= 5; i++)
				if ((packet_un[i] >> 1) == packet_length - i)
					address_length = i;
		}
		pid = ((packet_un[address_length] & 0x01) << 1) | (packet_un[address_length + 1] >> 7);
		ack = (packet_un[address_length + 1] >> 6) & 0x01;
		for (uint8_t i = 0; i < address_length; i++)
			packet[address_length - 1 - i] = packet_un[i];
		for (uint8_t i = address_length; i < packet_length; i++)
			packet[i] = bit_reverse((packet_un[i + 1] << 2) | (packet_un[i + 2] >> 6));
		return true;
	}
	return false;
}

static void XN297Dump_overflow(void)
{
	if (dump_platform_timer_overflow())
		timeH++;
}

static void XN297Dump_RF_init(void)
{
	NRF24L01_Initialize();
	NRF24L01_SetTxRxMode(RX_EN);
	NRF24L01_WriteReg(NRF24L01_03_SETUP_AW, 0x01);
	NRF24L01_WriteRegisterMulti(NRF24L01_0A_RX_ADDR_P0, (uint8_t *)"\x55\x0F\x71", 3);
	NRF24L01_WriteReg(NRF24L01_11_RX_PW_P0, XN297DUMP_MAX_PACKET_LEN);
	debug("XN297 dump, address length=%d, bitrate=", address_length);
	switch (bitrate) {
	case XN297DUMP_250K:
		NRF24L01_SetBitrate(NRF24L01_BR_250K);
		debugln("250K");
		break;
	case XN297DUMP_2M:
		NRF24L01_SetBitrate(NRF24L01_BR_2M);
		debugln("2M");
		break;
	default:
		NRF24L01_SetBitrate(NRF24L01_BR_1M);
		debugln("1M");
		break;
	}
}

void XN297Dump_init(void)
{
	if (sub_protocol > XN297DUMP_2M) sub_protocol = XN297DUMP_1M;
	bitrate = sub_protocol;
	address_length = RX_num;
	if (address_length < 3 || address_length > 5) address_length = 5;
	XN297Dump_RF_init();
	bind_counter = 0;
	rf_ch_num = 0xFF;
	old_option = option ^ 0x55;
	phase = 0;
	timeH = 0;
}

void XN297Dump_run(void)
{
	static uint32_t time = 0;

	for (;;) {
		if (option == 0xFF && bind_counter > XN297DUMP_PERIOD_SCAN) {
			hopping_frequency_no++;
			bind_counter = 0;
		}
		if (hopping_frequency_no != rf_ch_num) {
			if (hopping_frequency_no > XN297DUMP_MAX_RF_CHANNEL)
				hopping_frequency_no = 0;
			rf_ch_num = hopping_frequency_no;
			debugln("Channel=%d,0x%02X", hopping_frequency_no, hopping_frequency_no);
			NRF24L01_WriteReg(NRF24L01_05_RF_CH, hopping_frequency_no);
			NRF24L01_WriteReg(NRF24L01_07_STATUS, 0x70);
			NRF24L01_SetTxRxMode(TXRX_OFF);
			NRF24L01_SetTxRxMode(RX_EN);
			NRF24L01_FlushRx();
			NRF24L01_WriteReg(NRF24L01_00_CONFIG, (0 << NRF24L01_00_EN_CRC) | (1 << NRF24L01_00_CRCO) | (1 << NRF24L01_00_PWR_UP) | (1 << NRF24L01_00_PRIM_RX));
			phase = 0;
		}
		XN297Dump_overflow();

		if (NRF24L01_ReadReg(NRF24L01_07_STATUS) & _BV(NRF24L01_07_RX_DR)) {
			if (NRF24L01_ReadReg(NRF24L01_09_CD) || option != 0xFF) {
				NRF24L01_ReadPayload(packet, XN297DUMP_MAX_PACKET_LEN);
				XN297Dump_overflow();
				uint16_t timeL = dump_platform_timer_get_cnt();
				if (dump_platform_timer_overflow()) {
					timeH++;
					timeL = 0;
				}
				if ((phase & 0x01) == 0) {
					phase = 1;
					time = 0;
				} else {
					time = ((uint32_t)timeH << 16) + timeL - time;
				}
				if (XN297Dump_process_packet()) {
					debug("RX: %5luus C=%d ", (unsigned long)(time >> 1), hopping_frequency_no);
					time = ((uint32_t)timeH << 16) + timeL;
					if (enhanced) {
						debug("Enhanced ");
						debug("pid=%d ", pid);
						if (ack) debug("ack ");
					}
					debug("S=%c A=", scramble ? 'Y' : 'N');
					for (uint8_t i = 0; i < address_length; i++)
						debug(" %02X", packet[i]);
					debug(" P(%d)=", packet_length - address_length);
					for (uint8_t i = address_length; i < packet_length; i++)
						debug(" %02X", packet[i]);
					debugln("");
				} else {
					debugln("RX: %5luus C=%d Bad CRC", (unsigned long)(time >> 1), hopping_frequency_no);
				}
				XN297Dump_overflow();
				NRF24L01_WriteReg(NRF24L01_07_STATUS, 0x70);
				NRF24L01_SetTxRxMode(TXRX_OFF);
				NRF24L01_SetTxRxMode(RX_EN);
				NRF24L01_FlushRx();
				NRF24L01_WriteReg(NRF24L01_00_CONFIG, (0 << NRF24L01_00_EN_CRC) | (1 << NRF24L01_00_CRCO) | (1 << NRF24L01_00_PWR_UP) | (1 << NRF24L01_00_PRIM_RX));
				XN297Dump_overflow();
			}
		}
		bind_counter++;
		XN297Dump_overflow();
	}
}
