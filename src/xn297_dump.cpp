/*
 * XN297Dump - NRF24L01 RC sniffer (from Multiprotocol/XN297Dump_nrf24l01.ino).
 * Full support for all modes: 250K/1M/2M, Auto, NRF, XN297.
 */
#include "../include/dump_config.h"
#include "../include/dump_platform.h"
#include "../include/dump_types.h"
#include "../include/dump_cli.h"
#include "../include/iface_nrf24l01.h"
#include "../include/iface_xn297.h"
#include "../include/xn297_tables.h"
#include <string.h>
#include <stdlib.h>

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
static uint32_t time_stamp;

static uint8_t  *nbr_rf;
static uint32_t *time_rf;
static uint8_t  compare_channel;

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
	if (sub_protocol < XN297DUMP_AUTO)
		bitrate = sub_protocol;
	else
		bitrate = XN297DUMP_1M;
	
	address_length = RX_num;
	if (address_length < 3 || address_length > 5) address_length = 5;
	
	XN297Dump_RF_init();
	bind_counter = 0;
	rf_ch_num = 0xFF;
	old_option = option ^ 0x55;
	phase = 0;
	timeH = 0;
	time_stamp = 0;
	nbr_rf = NULL;
	time_rf = NULL;
	
	debugln("Initialized: mode=%d ch=%d addr=%d", sub_protocol, option, address_length);
}

static void XN297Dump_mode_basic(void)
{
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
			uint32_t time;
			if ((phase & 0x01) == 0) {
				phase = 1;
				time = 0;
			} else {
				time = ((uint32_t)timeH << 16) + timeL - time_stamp;
			}
			if (XN297Dump_process_packet()) {
				debug("RX: %5luus C=%d ", (unsigned long)(time >> 1), hopping_frequency_no);
				time_stamp = ((uint32_t)timeH << 16) + timeL;
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
}

static void XN297Dump_mode_nrf(void)
{
	if (phase == 0) {
		address_length = 5;
		memcpy(rx_tx_addr, (uint8_t *)"\xCC\xCC\xCC\xCC\xCC", address_length);
		bitrate = XN297DUMP_250K;
		packet_length = 9;
		
		NRF24L01_Initialize();
		NRF24L01_SetTxRxMode(TXRX_OFF);
		NRF24L01_SetTxRxMode(RX_EN);
		NRF24L01_WriteReg(NRF24L01_03_SETUP_AW, address_length - 2);
		NRF24L01_WriteRegisterMulti(NRF24L01_0A_RX_ADDR_P0, rx_tx_addr, address_length);
		NRF24L01_WriteReg(NRF24L01_11_RX_PW_P0, packet_length);
		NRF24L01_WriteReg(NRF24L01_05_RF_CH, option);
		old_option = option;
		
		debug("NRF dump, len=%d, rf=%d, address length=%d, bitrate=", packet_length, option, address_length);
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
		NRF24L01_WriteReg(NRF24L01_00_CONFIG, _BV(NRF24L01_00_PWR_UP) | _BV(NRF24L01_00_PRIM_RX));
		phase++;
		time_stamp = 0;
	}
	else {
		if (NRF24L01_ReadReg(NRF24L01_07_STATUS) & _BV(NRF24L01_07_RX_DR)) {
			if (NRF24L01_ReadReg(NRF24L01_09_CD)) {
				XN297Dump_overflow();
				uint16_t timeL = dump_platform_timer_get_cnt();
				if (dump_platform_timer_overflow()) {
					timeH++;
					timeL = 0;
				}
				uint32_t time = ((uint32_t)timeH << 16) + timeL - time_stamp;
				debug("RX: %5luus ", (unsigned long)(time >> 1));
				time_stamp = ((uint32_t)timeH << 16) + timeL;
				NRF24L01_ReadPayload(packet, packet_length);
				debug("C: %02X P:", option);
				for (uint8_t i = 0; i < packet_length; i++)
					debug(" %02X", packet[i]);
				debugln("");
				memcpy(packet_in, packet, packet_length);
			}
			NRF24L01_WriteReg(NRF24L01_07_STATUS, 0x70);
			NRF24L01_SetTxRxMode(TXRX_OFF);
			NRF24L01_SetTxRxMode(RX_EN);
			NRF24L01_FlushRx();
			NRF24L01_WriteReg(NRF24L01_00_CONFIG, _BV(NRF24L01_00_PWR_UP) | _BV(NRF24L01_00_PRIM_RX));
		}
		XN297Dump_overflow();
		if (old_option != option) {
			debugln("Channel changed to %d", option);
			NRF24L01_WriteReg(NRF24L01_05_RF_CH, option);
			old_option = option;
		}
	}
}

static void XN297Dump_mode_xn297(void)
{
	if (phase == 0) {
		address_length = 5;
		memcpy(rx_tx_addr, (uint8_t *)"\x00\x00\x00\x00\x00", address_length);
		packet_length = 9;
		
		XN297_Configure(XN297_CRCEN, XN297_SCRAMBLED, XN297_1M);
		XN297_SetTxRxMode(TXRX_OFF);
		XN297_SetTXAddr(rx_tx_addr, address_length);
		XN297_SetRXAddr(rx_tx_addr, packet_length);
		XN297_Hopping(option);
		old_option = option;
		XN297_SetTxRxMode(RX_EN);
		
		debugln("XN297 dump, len=%d, rf=%d, address length=%d", packet_length, option, address_length);
		phase = 1;
		time_stamp = 0;
	}
	else {
		bool rx = XN297_IsRX();
		if (rx) {
			XN297_SetTxRxMode(TXRX_OFF);
			XN297Dump_overflow();
			uint16_t timeL = dump_platform_timer_get_cnt();
			if (dump_platform_timer_overflow()) {
				timeH++;
				timeL = 0;
			}
			uint32_t time = ((uint32_t)timeH << 16) + timeL - time_stamp;
			debug("RX: %5luus ", (unsigned long)(time >> 1));
			time_stamp = ((uint32_t)timeH << 16) + timeL;
			if (XN297_ReadPayload(packet_in, packet_length)) {
				debug("OK:");
				for (uint8_t i = 0; i < packet_length; i++)
					debug(" %02X", packet_in[i]);
			} else {
				debug(" NOK");
			}
			debugln("");
			XN297_SetTxRxMode(RX_EN);
		}
		XN297Dump_overflow();
		if (old_option != option) {
			debugln("C=%d(%02X)", option, option);
			XN297_Hopping(option);
			old_option = option;
		}
	}
}

static void XN297Dump_mode_auto(void)
{
	switch (phase) {
	case 0:
		debugln("------------------------");
		debugln("Detecting XN297 packets.");
		XN297Dump_RF_init();
		debug("Trying RF channel: 0");
		hopping_frequency_no = 0;
		bitrate = 0;
		phase++;
		break;
	case 1:
		if (bind_counter > XN297DUMP_PERIOD_SCAN) {
			hopping_frequency_no++;
			bind_counter = 0;
			if (hopping_frequency_no > XN297DUMP_MAX_RF_CHANNEL) {
				hopping_frequency_no = 0;
				bitrate++;
				bitrate %= 3;
				debugln("");
				XN297Dump_RF_init();
				debug("Trying RF channel: 0");
			}
			if (hopping_frequency_no)
				debug(",%d", hopping_frequency_no);
			NRF24L01_WriteReg(NRF24L01_05_RF_CH, hopping_frequency_no);
			NRF24L01_WriteReg(NRF24L01_07_STATUS, 0x70);
			NRF24L01_SetTxRxMode(TXRX_OFF);
			NRF24L01_SetTxRxMode(RX_EN);
			NRF24L01_FlushRx();
			NRF24L01_WriteReg(NRF24L01_00_CONFIG, (0 << NRF24L01_00_EN_CRC) | (1 << NRF24L01_00_CRCO) | (1 << NRF24L01_00_PWR_UP) | (1 << NRF24L01_00_PRIM_RX));
		}
		if (NRF24L01_ReadReg(NRF24L01_07_STATUS) & _BV(NRF24L01_07_RX_DR)) {
			if (NRF24L01_ReadReg(NRF24L01_09_CD)) {
				NRF24L01_ReadPayload(packet, XN297DUMP_MAX_PACKET_LEN);
				if (XN297Dump_process_packet()) {
					debug("\r\n\r\nPacket detected: bitrate=");
					switch (bitrate) {
					case XN297DUMP_250K:
						XN297_Configure(XN297_CRCEN, scramble ? XN297_SCRAMBLED : XN297_UNSCRAMBLED, XN297_250K);
						debug("250K");
						break;
					case XN297DUMP_2M:
						XN297_Configure(XN297_CRCEN, scramble ? XN297_SCRAMBLED : XN297_UNSCRAMBLED, XN297_1M);
						NRF24L01_SetBitrate(NRF24L01_BR_2M);
						debug("2M");
						break;
					default:
						XN297_Configure(XN297_CRCEN, scramble ? XN297_SCRAMBLED : XN297_UNSCRAMBLED, XN297_1M);
						debug("1M");
						break;
					}
					debug(" C=%d ", hopping_frequency_no);
					if (enhanced) {
						debug("Enhanced ");
						debug("pid=%d ", pid);
						if (ack) debug("ack ");
					}
					debug("S=%c A=", scramble ? 'Y' : 'N');
					for (uint8_t i = 0; i < address_length; i++) {
						debug(" %02X", packet[i]);
						rx_tx_addr[i] = packet[i];
					}
					debug(" P(%d)=", packet_length - address_length);
					for (uint8_t i = address_length; i < packet_length; i++)
						debug(" %02X", packet[i]);
					packet_length = packet_length - address_length;
					debugln("\r\n--------------------------------");
					debugln("Identifying all RF channels in use.");
					bind_counter = 0;
					hopping_frequency_no = 0;
					rf_ch_num = 0;
					packet_count = 0;
					nbr_rf = (uint8_t *)malloc(XN297DUMP_MAX_RF_CHANNEL * sizeof(uint8_t));
					if (nbr_rf == NULL) {
						debugln("\r\nCan't allocate memory for next phase!!!");
						phase = 0;
						break;
					}
					debug("Trying RF channel: 0");
					XN297_SetTXAddr(rx_tx_addr, address_length);
					XN297_SetRXAddr(rx_tx_addr, packet_length);
					XN297_RFChannel(0);
					XN297_SetTxRxMode(TXRX_OFF);
					XN297_SetTxRxMode(RX_EN);
					phase = 2;
				}
			}
		}
		break;
	case 2:
		if (bind_counter > XN297DUMP_PERIOD_SCAN) {
			hopping_frequency_no++;
			bind_counter = 0;
			if (packet_count && packet_count <= 20)
				debug("\r\nTrying RF channel: ");
			packet_count = 0;
			if (hopping_frequency_no > XN297DUMP_MAX_RF_CHANNEL) {
				uint8_t nbr_max = 0, j = 0;
				debug("\r\n\r\n%d RF channels identified:", rf_ch_num);
				compare_channel = 0;
				for (uint8_t i = 0; i < rf_ch_num; i++) {
					debug(" %d[%d]", hopping_frequency[i], nbr_rf[i]);
					if (nbr_rf[i] > nbr_max) {
						nbr_max = nbr_rf[i];
						compare_channel = i;
					}
				}
				nbr_max = (nbr_max * 2) / 3;
				debug("\r\nKeeping only RF channels with more than %d packets:", nbr_max);
				for (uint8_t i = 0; i < rf_ch_num; i++)
					if (nbr_rf[i] >= nbr_max) {
						hopping_frequency[j] = hopping_frequency[i];
						debug(" %d", hopping_frequency[j]);
						if (compare_channel == i) {
							compare_channel = j;
							debug("*");
						}
						j++;
					}
				rf_ch_num = j;
				free(nbr_rf);
				nbr_rf = NULL;
				time_rf = (uint32_t *)malloc(rf_ch_num * sizeof(uint32_t));
				if (time_rf == NULL) {
					debugln("\r\nCan't allocate memory for next phase!!!");
					phase = 0;
					break;
				}
				debugln("\r\n--------------------------------");
				debugln("Identifying RF channels order.");
				hopping_frequency_no = 0;
				phase = 3;
				packet_count = 0;
				bind_counter = 0;
				debugln("Time between CH:%d and CH:%d", hopping_frequency[compare_channel], hopping_frequency[hopping_frequency_no]);
				time_rf[hopping_frequency_no] = 0xFFFFFFFF;
				XN297_RFChannel(hopping_frequency[compare_channel]);
				uint16_t timeL = dump_platform_timer_get_cnt();
				if (dump_platform_timer_overflow()) {
					timeH++;
					timeL = 0;
				}
				time_stamp = ((uint32_t)timeH << 16) + timeL;
				XN297_SetTxRxMode(TXRX_OFF);
				XN297_SetTxRxMode(RX_EN);
				XN297Dump_overflow();
				break;
			}
			debug(",%d", hopping_frequency_no);
			XN297_RFChannel(hopping_frequency_no);
			XN297_SetTxRxMode(TXRX_OFF);
			XN297_SetTxRxMode(RX_EN);
		}
		if (XN297_IsRX()) {
			if (NRF24L01_ReadReg(NRF24L01_09_CD)) {
				uint8_t res;
				if (enhanced) {
					res = XN297_ReadEnhancedPayload(packet, packet_length);
					res++;
				} else
					res = XN297_ReadPayload(packet, packet_length);
				if (res) {
					XN297Dump_overflow();
					uint16_t timeL = dump_platform_timer_get_cnt();
					if (dump_platform_timer_overflow()) {
						timeH++;
						timeL = 0;
					}
					uint32_t time;
					if (packet_count == 0) {
						hopping_frequency[rf_ch_num] = hopping_frequency_no;
						rf_ch_num++;
						time = 0;
					} else
						time = ((uint32_t)timeH << 16) + timeL - time_stamp;
					debug("\r\nRX on channel: %d, Time: %5luus P:", hopping_frequency_no, (unsigned long)(time >> 1));
					time_stamp = ((uint32_t)timeH << 16) + timeL;
					for (uint8_t i = 0; i < packet_length; i++)
						debug(" %02X", packet[i]);
					packet_count++;
					nbr_rf[rf_ch_num - 1] = packet_count;
					if (packet_count > 20) {
						bind_counter = XN297DUMP_PERIOD_SCAN + 1;
						debug("\r\nTrying RF channel: ");
					}
				}
			}
			XN297_SetTxRxMode(TXRX_OFF);
			XN297_SetTxRxMode(RX_EN);
		}
		XN297Dump_overflow();
		break;
	case 3:
		if (bind_counter > XN297DUMP_PERIOD_SCAN) {
			hopping_frequency_no++;
			bind_counter = 0;
			if (hopping_frequency_no >= rf_ch_num) {
				uint8_t next = 0;
				debugln("\r\n\r\nChannel order:");
				debugln("%d:     0us", hopping_frequency[compare_channel]);
				uint8_t i = 0;
				do {
					uint32_t time = time_rf[i];
					if (time != 0xFFFFFFFF) {
						next = i;
						for (uint8_t j = 1; j < rf_ch_num; j++)
							if (time > time_rf[j]) {
								next = j;
								time = time_rf[j];
							}
						time_rf[next] = 0xFFFFFFFF;
						debugln("%d: %5luus", hopping_frequency[next], (unsigned long)time);
						i = 0;
					}
					i++;
				} while (i < rf_ch_num);
				free(time_rf);
				time_rf = NULL;
				debugln("\r\n--------------------------------");
				debugln("Identifying Sticks and features.");
				phase = 4;
				hopping_frequency_no = 0;
				break;
			}
			debugln("Time between CH:%d and CH:%d", hopping_frequency[compare_channel], hopping_frequency[hopping_frequency_no]);
			time_rf[hopping_frequency_no] = 0xFFFFFFFF;
			XN297_RFChannel(hopping_frequency[compare_channel]);
			uint16_t timeL = dump_platform_timer_get_cnt();
			if (dump_platform_timer_overflow()) {
				timeH++;
				timeL = 0;
			}
			time_stamp = ((uint32_t)timeH << 16) + timeL;
			XN297_SetTxRxMode(TXRX_OFF);
			XN297_SetTxRxMode(RX_EN);
		}
		if (XN297_IsRX()) {
			if (NRF24L01_ReadReg(NRF24L01_09_CD)) {
				uint8_t res;
				if (enhanced) {
					res = XN297_ReadEnhancedPayload(packet, packet_length);
					res++;
				} else
					res = XN297_ReadPayload(packet, packet_length);
				if (res) {
					XN297Dump_overflow();
					uint16_t timeL = dump_platform_timer_get_cnt();
					if (dump_platform_timer_overflow()) {
						timeH++;
						timeL = 0;
					}
					if (packet_count & 1) {
						uint32_t time = ((uint32_t)timeH << 16) + timeL - time_stamp;
						if (time_rf[hopping_frequency_no] > (time >> 1))
							time_rf[hopping_frequency_no] = time >> 1;
						debugln("Time: %5luus", (unsigned long)(time >> 1));
						XN297_RFChannel(hopping_frequency[compare_channel]);
					} else {
						time_stamp = ((uint32_t)timeH << 16) + timeL;
						XN297_RFChannel(hopping_frequency[hopping_frequency_no]);
					}
					packet_count++;
					if (packet_count > 24) {
						bind_counter = XN297DUMP_PERIOD_SCAN + 1;
						packet_count = 0;
					}
				}
			}
			XN297_SetTxRxMode(TXRX_OFF);
			XN297_SetTxRxMode(RX_EN);
		}
		XN297Dump_overflow();
		break;
	case 4:
		if (XN297_IsRX()) {
			uint8_t res;
			if (enhanced) {
				res = XN297_ReadEnhancedPayload(packet, packet_length);
				res++;
			} else
				res = XN297_ReadPayload(packet, packet_length);
			if (res) {
				if (memcmp(packet_in, packet, packet_length)) {
					debug("P:");
					for (uint8_t i = 0; i < packet_length; i++)
						debug(" %02X", packet[i]);
					debugln("");
					memcpy(packet_in, packet, packet_length);
				}
			}
			XN297_SetTxRxMode(TXRX_OFF);
			XN297_SetTxRxMode(RX_EN);
		}
		break;
	}
	bind_counter++;
}

void XN297Dump_step(void)
{
	if (!cli_dump_running)
		return;
	
	switch (sub_protocol) {
	case XN297DUMP_250K:
	case XN297DUMP_1M:
	case XN297DUMP_2M:
		XN297Dump_mode_basic();
		break;
	case XN297DUMP_AUTO:
		XN297Dump_mode_auto();
		break;
	case XN297DUMP_NRF:
		XN297Dump_mode_nrf();
		break;
	case XN297DUMP_XN297:
		XN297Dump_mode_xn297();
		break;
	default:
		XN297Dump_mode_basic();
		break;
	}
	XN297Dump_overflow();
}

void XN297Dump_run(void)
{
	for (;;) {
		cli_process();
		
		if (cli_restart_requested()) {
			cli_clear_restart();
			XN297Dump_init();
		}
		
		XN297Dump_step();
	}
}
