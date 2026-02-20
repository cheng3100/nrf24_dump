/*
 * bit_reverse, crc16_update and globals for dump.
 */
#include "dump_config.h"
#include "dump_types.h"
#include <string.h>

uint8_t  sub_protocol = XN297DUMP_1M;
uint8_t  option       = 0xFF;
uint8_t  RX_num       = 5;
uint8_t  packet[50];
uint8_t  packet_in[50];
uint8_t  packet_length;
uint8_t  hopping_frequency[50];
uint8_t  hopping_frequency_no;
uint8_t  rf_ch_num;
uint8_t  rx_tx_addr[5];
uint16_t bind_counter;
uint8_t  phase;
uint16_t crc;
uint16_t crc16_polynomial = 0x1021;
uint8_t  prev_power = 0xFD;

uint8_t bit_reverse(uint8_t b_in)
{
	uint8_t b_out = 0;
	for (uint8_t i = 0; i < 8; ++i) {
		b_out = (b_out << 1) | (b_in & 1);
		b_in >>= 1;
	}
	return b_out;
}

void crc16_update(uint8_t a, uint8_t bits)
{
	crc ^= (uint16_t)a << 8;
	while (bits--) {
		if (crc & 0x8000)
			crc = (crc << 1) ^ crc16_polynomial;
		else
			crc = crc << 1;
	}
}
