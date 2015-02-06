#ifndef FIREFLY_H
#define FIREFLY_H

#include <inttypes.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include "lfsr32.h"
#include "wave.h"

/*
   PB2 =      = InOut = LED0
   PB1 =      = InOut = LED1
   PB0 =      = InOut = LED2
   PB5 =      = InOut = LED3
   PB3 = ADC3 = Input = BAT+/SOLAR+
   PB4 = ADC2 = Input = SOLAR-

   connection schemata for LEDs are:
   - two antiparallel connected LEDs to two wire, makes six such wires
   - connect these wires to PB0-PB1, PB0-PB2, PB0-PB5, PB1-PB2, PB1-PB5, PB2-PB5
   - the order of these connections are not relevant

   setup:
   - install AVRootloader.HEX on AVR
   - setup fuses to XTAL=8Mhz, no Brownout Detection, no WDTON
   - connect RS232 on PB2 and test Bootloader (reading/writing EEPROM)
   - setup fuses to disable RESET Pin and lockbits
   - connect with bootloader an program FireFly.HEX
*/


#define FLAG_WAVE           (1 << 0)                    // PWM is active for a LED
#define FLAG_TIMER          (1 << 1)                    // PWM is active
#define FLAG_UPDATE         (1 << 2)                    // Watchdog signaled a update
#define FLAG_ISNIGHT        (1 << 3)                    // is at night
#define FLAG_MEASURE        (1 << 4)                    // measure battery and solarpanel

//#define MEASURE_INTERVAL    (uint16_t)(6 *  3750)       // each 6 minutes measure interval
//#define MEASURE_INTERVAL    (uint16_t)(2 *  3750)       // each 2 minutes measure interval
#define MEASURE_INTERVAL    (uint16_t)(1 *  375)       // each 2 * 6 seconds measure interval

#define BATTERY_OK          (uint16_t)(0.9 * 400)       // 0.9 Volt

// LED Rows, Pins connected to LEDs
#define R0  (1 << PB0)
#define R1  (1 << PB1)
#define R2  (1 << PB2)
#define R3  (1 << PB3)

// bitmasks for driving LEDs on PORTB and DDRB, first value are for PORTB (always 1),
// next 3 values for DDRB to connect LEDs to GND (on) or as high-Z input (off)
// you can reorder this array iff needed so long as each row depends to only one PORTB value,
// means in each row you can savely reorder the colums and/or you can savely reorder the rows.
const prog_uint8_t ddr_data[16] = {
    R0, R0^R1, R0^R2, R0^R3,
    R1, R1^R0, R1^R2, R1^R3,
    R2, R2^R0, R2^R1, R2^R3,
    R3, R3^R0, R3^R1, R3^R2};

// a Firefly
typedef struct {
    uint16_t wave_end;                              // pointer to end of wave samples
    uint16_t wave_ptr;                              // pointer to current wave sample == PWM Dutycylce of one LED
    uint16_t hungry;                                // energy points that must be feed to activate lightning of firefly
    uint16_t energy;                                // next energy points to add to hungry
} firefly_t;                                        // one energy/hungry point relates to one WDT cycle of 16ms

typedef firefly_t* firefly_p;

// some macros, don't use WinAVR GCC default macros because these produce inefficient compiled code
#define  wdt_reset()         asm volatile("wdr")
#define  cli()               asm volatile("cli")
#define  sei()               asm volatile("sei")
#define  sleep_cpu()         asm volatile("sleep")

// read word from FLASH with postincrement addresspointer
#define pgm_read_word_inc(addr) \
(__extension__({                \
    uint16_t __result;          \
    __asm__                     \
    (                           \
        "lpm %A0, Z+" "\n\t"    \
        "lpm %B0, Z+" "\n\t"    \
        : "=r" (__result),      \
          "=z" (addr)           \
        : "1" (addr)            \
    );                          \
    __result;                   \
}))


#endif
