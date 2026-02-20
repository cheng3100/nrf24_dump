/*
 * CLI command parser for XN297Dump.
 * Commands:
 *   help              - show help
 *   status            - show current settings
 *   mode <0-6>        - set sub_protocol (0=250K,1=1M,2=2M,3=Auto,4=NRF,5=CC2500,6=XN297)
 *   ch <0-84|255|scan> - set RF channel (255 or 'scan' = scan all)
 *   addr <3-5>        - set address length
 *   start             - start dumping
 *   stop              - stop dumping
 *   restart           - restart with current settings
 */
#include "../include/dump_cli.h"
#include "../include/dump_config.h"
#include "../include/dump_platform.h"
#include "../include/dump_types.h"
#include "../include/iface_nrf24l01.h"
#include <string.h>
#include <stdlib.h>

#define CLI_BUF_SIZE 64

volatile bool cli_dump_running = false;
static volatile bool s_restart_requested = false;
static char s_cmd_buf[CLI_BUF_SIZE];
static uint8_t s_cmd_idx = 0;

static const char *mode_names[] = {
	"250K", "1M", "2M", "Auto", "NRF", "CC2500", "XN297"
};

void cli_print_help(void)
{
	dump_platform_debugln("=== NRF24L01 XN297 Dump CLI ===");
	dump_platform_debugln("Commands:");
	dump_platform_debugln("  help              - show this help");
	dump_platform_debugln("  status            - show current settings");
	dump_platform_debugln("  detect            - detect NRF24L01 module");
	dump_platform_debugln("  mode <0-6>        - set mode (0=250K,1=1M,2=2M,3=Auto,4=NRF,6=XN297)");
	dump_platform_debugln("  ch <0-84|255|scan> - set RF channel (255/scan = scan all)");
	dump_platform_debugln("  addr <3-5>        - set address length");
	dump_platform_debugln("  start             - start dumping");
	dump_platform_debugln("  stop              - stop dumping");
	dump_platform_debugln("  restart           - restart with current settings");
	dump_platform_debugln("");
}

static uint8_t s_nrf_detected = 0;

void cli_detect_nrf(void)
{
	dump_platform_debug("Detecting NRF24L01... ");
	s_nrf_detected = NRF24L01_Detect();
	if (s_nrf_detected) {
		dump_platform_debugln("FOUND");
	} else {
		dump_platform_debugln("NOT FOUND");
		dump_platform_debugln("  Check SPI wiring: MOSI, MISO, SCK, CSN");
	}
}

uint8_t cli_is_nrf_detected(void)
{
	return s_nrf_detected;
}

void cli_print_status(void)
{
	dump_platform_debugln("=== Current Settings ===");
	dump_platform_debug("  Mode (sub_protocol): %d", sub_protocol);
	if (sub_protocol <= XN297DUMP_XN297)
		dump_platform_debugln(" (%s)", mode_names[sub_protocol]);
	else
		dump_platform_debugln("");
	
	if (option == 0xFF)
		dump_platform_debugln("  Channel (option):    scan (0xFF)");
	else
		dump_platform_debugln("  Channel (option):    %d (0x%02X)", option, option);
	
	dump_platform_debugln("  Addr len (RX_num):   %d", RX_num);
	dump_platform_debugln("  Dump running:        %s", cli_dump_running ? "YES" : "NO");
	dump_platform_debugln("");
}

void cli_init(void)
{
	s_cmd_idx = 0;
	s_restart_requested = false;
	cli_dump_running = false;
	
	dump_platform_debugln("");
	dump_platform_debugln("========================================");
	dump_platform_debugln("   NRF24L01 XN297 Dump - CLI Interface");
	dump_platform_debugln("========================================");
	dump_platform_debugln("");
	cli_print_help();
	cli_print_status();
	dump_platform_debug("> ");
}

static void cli_parse_cmd(const char *cmd)
{
	char *p;
	char arg[32];
	
	while (*cmd == ' ' || *cmd == '\t') cmd++;
	if (*cmd == '\0') {
		dump_platform_debug("> ");
		return;
	}
	
	if (strncmp(cmd, "help", 4) == 0) {
		cli_print_help();
	}
	else if (strncmp(cmd, "status", 6) == 0) {
		cli_print_status();
	}
	else if (strncmp(cmd, "detect", 6) == 0) {
		cli_detect_nrf();
	}
	else if (strncmp(cmd, "mode ", 5) == 0 || strncmp(cmd, "sub ", 4) == 0) {
		p = (char *)cmd + (cmd[0] == 'm' ? 5 : 4);
		int val = atoi(p);
		if (val >= 0 && val <= 6) {
			sub_protocol = (uint8_t)val;
			dump_platform_debugln("Mode set to %d (%s)", sub_protocol, 
				sub_protocol <= XN297DUMP_XN297 ? mode_names[sub_protocol] : "?");
		} else {
			dump_platform_debugln("Error: mode must be 0-6");
		}
	}
	else if (strncmp(cmd, "ch ", 3) == 0 || strncmp(cmd, "channel ", 8) == 0) {
		p = (char *)cmd + (cmd[1] == 'h' ? 3 : 8);
		while (*p == ' ') p++;
		if (strncmp(p, "scan", 4) == 0) {
			option = 0xFF;
			dump_platform_debugln("Channel set to SCAN (0xFF)");
		} else {
			int val = atoi(p);
			if (val >= 0 && val <= 255) {
				option = (uint8_t)val;
				if (option == 0xFF)
					dump_platform_debugln("Channel set to SCAN (0xFF)");
				else
					dump_platform_debugln("Channel set to %d (0x%02X)", option, option);
			} else {
				dump_platform_debugln("Error: channel must be 0-84 or 255/scan");
			}
		}
	}
	else if (strncmp(cmd, "addr ", 5) == 0) {
		p = (char *)cmd + 5;
		int val = atoi(p);
		if (val >= 3 && val <= 5) {
			RX_num = (uint8_t)val;
			dump_platform_debugln("Address length set to %d", RX_num);
		} else {
			dump_platform_debugln("Error: addr must be 3, 4, or 5");
		}
	}
	else if (strncmp(cmd, "start", 5) == 0) {
		if (cli_dump_running) {
			dump_platform_debugln("Dump already running");
		} else {
			dump_platform_debugln("Starting dump...");
			cli_dump_running = true;
			s_restart_requested = true;
		}
	}
	else if (strncmp(cmd, "stop", 4) == 0) {
		if (!cli_dump_running) {
			dump_platform_debugln("Dump not running");
		} else {
			dump_platform_debugln("Stopping dump...");
			cli_dump_running = false;
		}
	}
	else if (strncmp(cmd, "restart", 7) == 0) {
		dump_platform_debugln("Restarting dump with current settings...");
		cli_dump_running = true;
		s_restart_requested = true;
	}
	else {
		dump_platform_debugln("Unknown command: %s", cmd);
		dump_platform_debugln("Type 'help' for available commands");
	}
	
	dump_platform_debug("> ");
}

void cli_process(void)
{
	while (dump_platform_serial_available()) {
		int c = dump_platform_serial_read();
		if (c < 0) break;
		
		if (c == '\r' || c == '\n') {
			if (s_cmd_idx > 0) {
				dump_platform_debugln("");
				s_cmd_buf[s_cmd_idx] = '\0';
				cli_parse_cmd(s_cmd_buf);
				s_cmd_idx = 0;
			}
		}
		else if (c == 0x08 || c == 0x7F) {
			if (s_cmd_idx > 0) {
				s_cmd_idx--;
				dump_platform_debug("\b \b");
			}
		}
		else if (s_cmd_idx < CLI_BUF_SIZE - 1) {
			s_cmd_buf[s_cmd_idx++] = (char)c;
			char echo[2] = { (char)c, 0 };
			dump_platform_debug("%s", echo);
		}
	}
}

void cli_request_restart(void)
{
	s_restart_requested = true;
}

bool cli_restart_requested(void)
{
	return s_restart_requested;
}

void cli_clear_restart(void)
{
	s_restart_requested = false;
}
