// Compander to reduce dynamic range (16-bit unsigned <-> 8-bit unsigned)
// Dan Jackson

// A number's decompressed value is double the triangle number: n*(n+1)
// The delta output from each successive compressed value, is double the value itself.

#include "compander.h"

// Derived from Microchip AppNote 91040a by Ross M. Fosler, via https://stackoverflow.com/questions/1100090
static uint16_t int_sqrt32(uint32_t x) {
    uint16_t add = UINT16_C(0x8000), res = 0;
    for(int i = 0; i < 16; i++) {
        uint16_t temp = res | add;
        uint32_t g2 = (uint32_t)temp * temp;
        if (x >= g2) res = temp;
        add >>= 1;
    }
    return res;
}

// Find the triangle root of half the given value (16-bit range -> 8-bit range, non-linearly)
uint8_t compander_compress(uint16_t value)
{
	return (uint8_t)((int_sqrt32((uint32_t)value * 4 + 1) - 1) / 2);
}

// Find double the triangle number of the given value (8-bit range -> 16-bit range, non-linearly)
uint16_t compander_decompress(uint8_t value)
{
	return value * ((uint16_t)value + 1);
}


// cc -DCOMPANDER_TEST compander.c && ./a.out
#ifdef COMPANDER_TEST

#include <stdio.h>

int compander_test() {
	unsigned int compress_errors = 0, decompress_errors = 0;

	for (int input = 0; input <= 255; input++) {
		uint16_t value = compander_decompress(input);
		uint8_t output = compander_compress(value);
		int expected = input;
		if (output != expected) { 
			printf("ERROR: Decompressing %d -> %d -> %d (expected %d, diff %d)\n", input, value, output, expected, output - expected);
			decompress_errors++;
		}
	}

	int previousOutput = 0, changeInput = 0;
	for (int input = 0; input <= 65535; input++) {
		uint8_t value = compander_compress(input);
		uint16_t output = compander_decompress(value);
		if (output != previousOutput) { previousOutput = output; changeInput = input; }
		int expected = input - (input - changeInput);  // expected divergence from input
		if (output != expected) {
			printf("ERROR: Compressing %d -> %d -> %d (expected %d, diff %d)\n", input, value, output, expected, output - expected);
			compress_errors++;
		}
	}

	if (compress_errors > 0 || decompress_errors > 0) {
		printf("COMPANDER: %d compress error(s), %d decompress error(s).\n", compress_errors, decompress_errors);
		return 1;
	} else {
		printf("COMPANDER: All round-trips OK.\n");
		return 0;
	}
}

int main(int argc, char *argv[]) {
	int returnValue = compander_test();
	return returnValue;
}

#endif
