/*
 * Platform implementation for ESP32-S3.
 * SPI (VSPI or default), NRF CSN/CE pins. Timer from micros().
 */
#ifdef PIO_PLATFORM_ESP32

#include "../include/dump_platform.h"
#include <Arduino.h>
#include <SPI.h>

extern "C" {

#ifndef NRF_CSN_PIN
#define NRF_CSN_PIN  5
#endif
#ifndef NRF_CE_PIN
#define NRF_CE_PIN   4
#endif

static SPIClass *spi = nullptr;

void dump_platform_debug_init(void) {
	Serial.begin(115200);
}

void dump_platform_debug(const char *fmt, ...) {
	char buf[192];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	Serial.print(buf);
}

void dump_platform_debugln(const char *fmt, ...) {
	char buf[192];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	Serial.println(buf);
}

int dump_platform_serial_available(void) {
	return Serial.available();
}

int dump_platform_serial_read(void) {
	return Serial.read();
}

void dump_platform_serial_read_line(char *buf, int maxlen) {
	int idx = 0;
	while (idx < maxlen - 1) {
		while (!Serial.available()) { yield(); }
		int c = Serial.read();
		if (c == '\r' || c == '\n') {
			if (idx > 0) break;
			continue;
		}
		buf[idx++] = (char)c;
		Serial.write((char)c);  /* echo */
	}
	buf[idx] = '\0';
	Serial.println();
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
	pinMode(NRF_CSN_PIN, OUTPUT);
	digitalWrite(NRF_CSN_PIN, HIGH);
	pinMode(NRF_CE_PIN, OUTPUT);
	digitalWrite(NRF_CE_PIN, HIGH);
}

void dump_platform_spi_write(uint8_t byte) {
	spi->transfer(byte);
}

uint8_t dump_platform_spi_read(void) {
	return spi->transfer(0xFF);
}

void dump_platform_nrf_csn_high(void) { digitalWrite(NRF_CSN_PIN, HIGH); }
void dump_platform_nrf_csn_low(void)  { digitalWrite(NRF_CSN_PIN, LOW); }
void dump_platform_nrf_ce_high(void)  { digitalWrite(NRF_CE_PIN, HIGH); }
void dump_platform_nrf_ce_low(void)   { digitalWrite(NRF_CE_PIN, LOW); }

void dump_platform_delay_us(unsigned int us) {
	delayMicroseconds(us);
}

} /* extern "C" */

#endif /* PIO_PLATFORM_ESP32 */
