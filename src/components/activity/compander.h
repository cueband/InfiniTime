// Compander to reduce dynamic range (16-bit unsigned <-> 8-bit unsigned)
// Dan Jackson

#ifndef COMPANDER_H
#define COMPANDER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

uint8_t compander_compress(uint16_t value);
uint16_t compander_decompress(uint8_t value);

#ifdef __cplusplus
}
#endif

#endif
