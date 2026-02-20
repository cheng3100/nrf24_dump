/*
 * Platform abstraction for XN297Dump: timer, debug (UART), SPI/pins.
 * STM32F103 and ESP32-S3.
 */
#ifndef DUMP_PLATFORM_H
#define DUMP_PLATFORM_H

#include <stdint.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Debug output: map to Serial */
void dump_platform_debug_init(void);
void dump_platform_debug(const char *fmt, ...);
void dump_platform_debugln(const char *fmt, ...);

/* Timer used for packet timing. Units: 0.5us (2MHz). */
void dump_platform_timer_init(void);
uint32_t dump_platform_timer_get_us(void);   /* full 32-bit time (0.5us units) */
uint16_t dump_platform_timer_get_cnt(void);  /* low 16 bits (like TCNT1) */
/* Returns 1 if 16-bit counter wrapped since last call. Call each loop. */
int dump_platform_timer_overflow(void);

/* SPI and NRF24L01 pins */
void dump_platform_spi_init(void);
void dump_platform_spi_write(uint8_t byte);
uint8_t dump_platform_spi_read(void);

void dump_platform_nrf_csn_high(void);
void dump_platform_nrf_csn_low(void);
void dump_platform_nrf_ce_high(void);
void dump_platform_nrf_ce_low(void);

void dump_platform_delay_us(unsigned int us);

#ifdef __cplusplus
}
#endif

#endif /* DUMP_PLATFORM_H */
