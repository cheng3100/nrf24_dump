/*
 * RC model air sniffer - standalone XN297Dump (NRF24L01).
 * Build with platformio: env stm32f103 or esp32s3.
 * 
 * CLI Commands:
 *   help              - show help
 *   status            - show current settings
 *   mode <0-6>        - set mode (0=250K,1=1M,2=2M,3=Auto,4=NRF,6=XN297)
 *   ch <0-84|255|scan> - set RF channel (255/scan = scan all)
 *   addr <3-5>        - set address length
 *   start             - start dumping
 *   stop              - stop dumping
 *   restart           - restart with current settings
 */
#include <Arduino.h>
#include <SPI.h>
#include "../include/dump_config.h"
#include "../include/dump_platform.h"
#include "../include/dump_types.h"
#include "../include/dump_cli.h"

extern void XN297Dump_init(void);
extern void XN297Dump_run(void);

/* STM32 Arduino core expects C linkage for setup/loop; ESP32 expects C++ */
#ifdef PIO_PLATFORM_STM32
extern "C" {
#endif

void setup(void) {
	dump_platform_debug_init();
	dump_platform_timer_init();
	dump_platform_spi_init();

	sub_protocol = DUMP_DEFAULT_SUB_PROTOCOL;
	option       = DUMP_DEFAULT_OPTION;
	RX_num       = DUMP_DEFAULT_RX_NUM;

	cli_init();
	cli_detect_nrf();
	
	XN297Dump_run();
}

void loop(void) {
	/* never reached when XN297Dump_run() is used */
}

#ifdef PIO_PLATFORM_STM32
}
#endif
