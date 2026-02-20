/*
 * RC model air sniffer - standalone XN297Dump (NRF24L01).
 * Build with platformio: env stm32f103 or esp32s3.
 */
#include "../include/dump_config.h"
#include "../include/dump_platform.h"
#include "../include/dump_types.h"
#include <Arduino.h>
#include <SPI.h>

extern void XN297Dump_init(void);
extern void XN297Dump_run(void);  /* never returns */

#ifdef __cplusplus
extern "C" {
#endif
void setup(void) {
	dump_platform_debug_init();
	dump_platform_timer_init();
	dump_platform_spi_init();

	sub_protocol = DUMP_DEFAULT_SUB_PROTOCOL;
	option       = DUMP_DEFAULT_OPTION;
	RX_num       = DUMP_DEFAULT_RX_NUM;

	dump_platform_debugln("XN297Dump standalone - RC sniffer");
	dump_platform_debug("sub=%u option=%u addr_len=%u\n", (unsigned)sub_protocol, (unsigned)option, (unsigned)RX_num);

	XN297Dump_init();
	XN297Dump_run();
}

void loop(void) {
	/* never reached when XN297Dump_run() is used */
}
#ifdef __cplusplus
}
#endif
