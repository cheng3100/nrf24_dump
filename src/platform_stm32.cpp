/*
 * Platform implementation for STM32F103 (e.g. 4-in-1 module).
 * SPI, NRF CSN on PB7. Timer uses micros() for portability.
 * Debug output uses hardware UART1 (Serial1); requires -DHAVE_HWSERIAL1 in platformio.ini.
 */
#ifdef PIO_PLATFORM_STM32

#include "../include/dump_platform.h"
#include <Arduino.h>
#include <SPI.h>

extern "C" {

#ifndef NRF_CSN_PIN
#define NRF_CSN_PIN  PB7
#endif
#ifndef NRF_CE_PIN
#define NRF_CE_PIN   -1   /* no CE control, tie high on board */
#endif

static SPIClass *spi = nullptr;

void dump_platform_debug_init(void) {
	Serial1.begin(115200);
}

void dump_platform_debug(const char *fmt, ...) {
	char buf[128];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	Serial1.print(buf);
}

void dump_platform_debugln(const char *fmt, ...) {
	char buf[128];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	Serial1.println(buf);
}

static uint16_t s_prev_cnt;

void dump_platform_timer_init(void) {
	s_prev_cnt = (uint16_t)(micros() & 0xFFFF);
}

uint32_t dump_platform_timer_get_us(void) {
	return (uint32_t)micros() * 2U;
}

uint16_t dump_platform_timer_get_cnt(void) {
	return (uint16_t)(micros() & 0xFFFF);
}

uint16_t dump_platform_timer_get_timeH(void) {
	return (uint16_t)((micros() >> 16) & 0xFFFF);
}

int dump_platform_timer_overflow(void) {
	uint16_t c = (uint16_t)(micros() & 0xFFFF);
	int ov = (c < s_prev_cnt) ? 1 : 0;
	s_prev_cnt = c;
	return ov;
}

void dump_platform_spi_init(void) {
	spi = &SPI;
	spi->begin();
	spi->setBitOrder(MSBFIRST);
	spi->setDataMode(SPI_MODE0);
	spi->setClockDivider(SPI_CLOCK_DIV8);  /* 9 MHz for 72MHz */
	pinMode(NRF_CSN_PIN, OUTPUT);
	digitalWrite(NRF_CSN_PIN, HIGH);
	if (NRF_CE_PIN >= 0) {
		pinMode(NRF_CE_PIN, OUTPUT);
		digitalWrite(NRF_CE_PIN, HIGH);
	}
}

void dump_platform_spi_write(uint8_t byte) {
	spi->transfer(byte);
}

uint8_t dump_platform_spi_read(void) {
	return spi->transfer(0xFF);
}

void dump_platform_nrf_csn_high(void) { digitalWrite(NRF_CSN_PIN, HIGH); }
void dump_platform_nrf_csn_low(void)  { digitalWrite(NRF_CSN_PIN, LOW); }
void dump_platform_nrf_ce_high(void) { if (NRF_CE_PIN >= 0) digitalWrite(NRF_CE_PIN, HIGH); }
void dump_platform_nrf_ce_low(void)  { if (NRF_CE_PIN >= 0) digitalWrite(NRF_CE_PIN, LOW); }

void dump_platform_delay_us(unsigned int us) {
	delayMicroseconds(us);
}

} /* extern "C" */

#endif /* PIO_PLATFORM_STM32 */
