#ifndef lfsr32_H
#define lfsr32_H
#include <inttypes.h>

extern uint32_t seed;  // lfsr register, MUST be alays != 0
// generate pseudo random bits, only last maximal 16 bits returned, but the LFSR can be forward seeked upto 256 bits
// ATTENTION, calling lfsr(0) seeks 256 bits forward instead 0 bits.
extern uint16_t lfsr(uint8_t bitcount);
extern uint16_t lfsr_poly(uint8_t bitcount, uint32_t polynom);
#endif



