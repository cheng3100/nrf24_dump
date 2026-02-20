/*
 * XN297 (NRF24L01-based) interface declarations.
 * NRF24L01-only build.
 */
#ifndef IFACE_XN297_H
#define IFACE_XN297_H

#include <stdint.h>
#include <stdbool.h>
#include "iface_nrf24l01.h"

#define XN297_UNSCRAMBLED   false
#define XN297_SCRAMBLED     true
#define XN297_CRCDIS        false
#define XN297_CRCEN         true
#define XN297_1M            false
#define XN297_250K          true

/* Called from dump; implemented in xn297_emu.c */
#ifdef __cplusplus
void XN297_Configure(bool crc_en, bool scramble_en, bool bitrate, bool force_nrf = false);
#else
void XN297_Configure(bool crc_en, bool scramble_en, bool bitrate, bool force_nrf);
#endif
void XN297_SetTXAddr(const uint8_t *addr, uint8_t len);
void XN297_SetRXAddr(const uint8_t *addr, uint8_t rx_packet_len);
void XN297_SetTxRxMode(enum TXRX_State mode);
bool XN297_IsRX(void);
bool XN297_ReadPayload(uint8_t *msg, uint8_t len);
uint8_t XN297_ReadEnhancedPayload(uint8_t *msg, uint8_t len);
void XN297_Hopping(uint8_t index);
void XN297_RFChannel(uint8_t number);

#endif /* IFACE_XN297_H */
