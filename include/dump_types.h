/*
 * Types and globals shared by XN297Dump and NRF24L01/XN297.
 */
#ifndef DUMP_TYPES_H
#define DUMP_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _BV
#define _BV(bit) (1 << (bit))
#endif

/* Protocol state (set in main or by serial) */
extern uint8_t  sub_protocol;
extern uint8_t  option;        /* RF channel 0..84 or 0xFF = scan */
extern uint8_t  RX_num;        /* address length 3, 4, or 5 */

/* Packet buffer and protocol vars */
extern uint8_t  packet[50];
extern uint8_t  packet_in[50];
extern uint8_t  packet_length;
extern uint8_t  packet_count;
extern uint8_t  hopping_frequency[50];
extern uint8_t  hopping_frequency_no;
extern uint8_t  rf_ch_num;
extern uint8_t  rx_tx_addr[5];
extern uint16_t bind_counter;
extern uint8_t  phase;
extern uint16_t crc;
extern uint16_t crc16_polynomial;

/* NRF24L01 */
extern uint8_t  prev_power;

/* Helpers (implemented in dump_util.c or main) */
uint8_t  bit_reverse(uint8_t b_in);
void     crc16_update(uint8_t a, uint8_t bits);

/* Stubs for multiprotocol compatibility */
#define BIND_DONE
#define TX_MAIN_PAUSE_off
#define IS_RX_FLAG_on 0

#ifdef __cplusplus
}
#endif

#endif /* DUMP_TYPES_H */
