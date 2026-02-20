/*
 * CLI command parser for XN297Dump.
 * Supports runtime parameter configuration via UART.
 */
#ifndef DUMP_CLI_H
#define DUMP_CLI_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* CLI state: running or stopped */
extern volatile bool cli_dump_running;

/* Initialize CLI (print banner, help) */
void cli_init(void);

/* Process any pending serial input (non-blocking) */
void cli_process(void);

/* Print current status */
void cli_print_status(void);

/* Print help */
void cli_print_help(void);

/* Detect NRF24L01 and print result */
void cli_detect_nrf(void);

/* Returns 1 if NRF24L01 was detected */
uint8_t cli_is_nrf_detected(void);

/* Called when dump needs to restart with new params */
void cli_request_restart(void);

/* Check if restart was requested */
bool cli_restart_requested(void);

/* Clear restart flag */
void cli_clear_restart(void);

#ifdef __cplusplus
}
#endif

#endif /* DUMP_CLI_H */
