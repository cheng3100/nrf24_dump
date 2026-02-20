/*
 * Standalone XN297Dump (NRF24L01 RC sniffer) config.
 * Only NRF24L01-related code; no other protocols.
 */
#ifndef DUMP_CONFIG_H
#define DUMP_CONFIG_H

#define XN297DUMP_STANDALONE 1
#define NRF24L01_ONLY        1

/* Sub-protocol: 0=250K, 1=1M, 2=2M, 3=Auto, 4=NRF raw, 5=CC2500(stub), 6=XN297 */
enum XN297DUMP {
	XN297DUMP_250K   = 0,
	XN297DUMP_1M     = 1,
	XN297DUMP_2M     = 2,
	XN297DUMP_AUTO   = 3,
	XN297DUMP_NRF    = 4,
	XN297DUMP_CC2500 = 5,
	XN297DUMP_XN297  = 6,
};

/* Defaults (can override in main or via Serial) */
#ifndef DUMP_DEFAULT_SUB_PROTOCOL
#define DUMP_DEFAULT_SUB_PROTOCOL  XN297DUMP_1M   /* 1 Mbps */
#endif
#ifndef DUMP_DEFAULT_OPTION
#define DUMP_DEFAULT_OPTION       0xFF           /* 0xFF = scan all channels; 0..84 = fixed channel */
#endif
#ifndef DUMP_DEFAULT_RX_NUM
#define DUMP_DEFAULT_RX_NUM       5              /* address length 3, 4, or 5 */
#endif

#endif /* DUMP_CONFIG_H */
