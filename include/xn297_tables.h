#ifndef XN297_TABLES_H
#define XN297_TABLES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const uint8_t  xn297_scramble[39];
extern const uint16_t xn297_crc_xorout_scrambled[35];
extern const uint16_t xn297_crc_xorout[35];
extern const uint16_t xn297_crc_xorout_scrambled_enhanced[35];
extern const uint16_t xn297_crc_xorout_enhanced[35];

#ifdef __cplusplus
}
#endif

#endif
