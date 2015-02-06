// ISRs for FireFly project

#include <avr/io.h>

#define FLAG_WAVE           0                               // PWM is active for a LED
#define FLAG_TIMER          1                               // PWM is active
#define FLAG_UPDATE         2                               // Watchdog signaled a update

#define SIZEOF_FLY          8                               // sizeof struct firefly_t
#define WAVE_END_L          0                               // offsets to members in struct firefly_t
#define WAVE_END_H          1
#define WAVE_PTR_L          2
#define WAVE_PTR_H          3
#define FLY0                0 * SIZEOF_FLY                  // offsets for 3 flies relative to _fly_ptr
#define FLY1                1 * SIZEOF_FLY
#define FLY2                2 * SIZEOF_FLY

#define _saveZ              r2
#define _saveY              r4
#define _saveSREG           r6
#define _zero               r7
#define _ocr_ptr            r8
#define _fly_ptr            r10
#define _ddr_idx            r16
#define _flag               r17

#define _a                  r18                             // fly a
#define _a_ocr              r18
#define _a_ddr              r19
#define _b                  r20                             // fly b
#define _b_ocr              r20
#define _b_ddr              r21
#define _c                  r22                             // fly c
#define _c_ocr              r22
#define _c_ddr              r23
#define _y                  r28
#define _yl                 r28
#define _yh                 r29
#define _z                  r30
#define _zl                 r30
#define _zh                 r31

.extern fireflies
.extern ddr_data
.extern ocr_data

// Timer0 Overflow ISR to calculate OCR0A+DDRB Values for next 3 fireflies
// load wave samples == PWM dutycycle, calculate next OCR and DDRB values
// min. 122+1*18 cycles == 0.85% MCU time for all PWM stuff
// max. 185+3*18 cycles == 1.46% MCU time for all PWM stuff
// running timer0 on XTAL/64 we need 3 timer cycles = 192 MCU cylces for Overflow ISR,
// thus a wave sample can't be greater as 252
// a LED dutycycle can be from 0 to 252 = 0% upto 98%, thats no problem because on
// higher values we don't see any differencies
// on each call we calculate for next 3 fireflies, after 4 calls we are finished
// one complete PWM cycles for 12 fireflies
.global TIM0_OVF_vect
TIM0_OVF_vect:
        in      _saveSREG, _SFR_IO_ADDR(SREG)
        movw    _saveZ, _z
        movw    _saveY, _y
        push    _a_ocr
        push    _a_ddr
        push    _b_ocr
        push    _b_ddr
        push    _c_ocr
        push    _c_ddr

        clr     _a_ocr
        clr     _b_ocr
        clr     _c_ocr

        out     _SFR_IO_ADDR(DDRB), _zero

        movw    _y, _fly_ptr
// fly a
        ldd     _zl, y +FLY0 +WAVE_PTR_L                    // if FLY0->wave_ptr == 0 goto 11:  Fly don't ligthing
        ldd     _zh, y +FLY0 +WAVE_PTR_H
        sbiw    _zl, 0
        breq    11f
        ori     _flag, (1 << FLAG_WAVE)                     // any fly is ligthning
        lpm     _a_ocr, z+                                  // load next wave sample = FLY0->wavr_ptr*
        ldd     _a_ddr, y +FLY0 +WAVE_END_L                 // check iff FLY0.wave_ptr >= FLY0.wave_end, finished lightning ?
        cp      _zl, _a_ddr
        ldd     _a_ddr, y +FLY0 +WAVE_END_H
        cpc     _zh, _a_ddr
        brne    10f
        clr     _zl                                         // if finish setup FLY0.wave_ptr = nil
        clr     _zh
10:     std     y +FLY0 +WAVE_PTR_L, _zl
        std     y +FLY0 +WAVE_PTR_H, _zh
11:
// fly b                                                    // do the same as for FLY0
        ldd     _zl, y +FLY1 +WAVE_PTR_L
        ldd     _zh, y +FLY1 +WAVE_PTR_H
        sbiw    _zl, 0
        breq    21f
        ori     _flag, (1 << FLAG_WAVE)
        lpm     _b_ocr, z+
        ldd     _b_ddr, y +FLY1 +WAVE_END_L
        cp      _zl, _b_ddr
        ldd     _b_ddr, y +FLY1 +WAVE_END_H
        cpc     _zh, _b_ddr
        brne    20f
        clr     _zl
        clr     _zh
20:     std     y +FLY1 +WAVE_PTR_L, _zl
        std     y +FLY1 +WAVE_PTR_H, _zh
21:
// fly c                                                    // do the same as for FLY0
        ldd     _zl, y +FLY2 +WAVE_PTR_L
        ldd     _zh, y +FLY2 +WAVE_PTR_H
        sbiw    _zl, 0
        breq    31f
        ori     _flag, (1 << FLAG_WAVE)
        lpm     _c_ocr, z+
        ldd     _c_ddr, y +FLY2 +WAVE_END_L
        cp      _zl, _c_ddr
        ldd     _c_ddr, y +FLY2 +WAVE_END_H
        cpc     _zh, _c_ddr
        brne    30f
        clr     _zl
        clr     _zh
30:     std     y +FLY2 +WAVE_PTR_L, _zl
        std     y +FLY2 +WAVE_PTR_H, _zh
31:
        adiw    _y, 3 * SIZEOF_FLY                           // increment _fly_ptr by 3 flies
// load DDRB/PORTB values from ddr_data for current 3 flies
        ldi     _zl, lo8(ddr_data)
        ldi     _zh, hi8(ddr_data)
        add     _zl, _ddr_idx
        adc     _zh, _zero
        lpm     _a_ddr, z+
        out     _SFR_IO_ADDR(PORTB), _a_ddr
        lpm     _a_ddr, z+
        lpm     _b_ddr, z+
        lpm     _c_ddr, z+
        subi    _ddr_idx, -4
        andi    _ddr_idx, 0x0F
        brne    40f
        sbrs    _flag, FLAG_WAVE                            // any fly was lightning ?
        andi    _flag, ~(1 << FLAG_TIMER)                   // no, then reset FLAG_TIMER to signal nothing is playing
        andi    _flag, ~(1 << FLAG_WAVE)
        ldi     _yl, lo8(fireflies)                         // _fly_ptr = &fireflies[]
        ldi     _yh, hi8(fireflies)
40:     movw    _fly_ptr, _y
// now sort each pair of DDRB/OCR0A values descending to OCR0A value == PWM dutycycle for each LED
        cp      _b_ocr, _c_ocr
        brsh    50f
        movw    _y, _b
        movw    _b, _c
        movw    _c, _y
50:     cp      _a_ocr, _b_ocr
        brsh    51f
        movw    _y, _a
        movw    _a, _b
        movw    _b, _y
        cp      _b_ocr, _c_ocr
        brsh    51f
        movw    _y, _b
        movw    _b, _c
        movw    _c, _y
51:
// OCR0A values must be negated
        neg     _a_ocr
        neg     _b_ocr
        neg     _c_ocr
// setup PINs
        or      _b_ddr, _a_ddr
        or      _c_ddr, _b_ddr
// store calculated OCR0A/DDRB pairs to ocr_data[] for Output Compare ISR
        ldi     _yl, lo8(ocr_data)
        ldi     _yh, hi8(ocr_data)
        movw    _ocr_ptr, _y
        cp      _a_ocr, _b_ocr
        breq    60f
        st      y+, _b_ocr
        st      y+, _a_ddr
60:     cp      _b_ocr, _c_ocr
        breq    61f
        st      y+, _c_ocr
        st      y+, _b_ddr
61:     st      y+, _zero
        st      y+, _c_ddr
// setup first OC-Event time
        out     _SFR_IO_ADDR(OCR0A), _a_ocr
// clear interrupt flags from to early signaled events
        ldi     _a_ocr, (1 << OCF0A) | (1 << TOV0)
        out     _SFR_IO_ADDR(TIFR), _a_ocr

        pop     _c_ddr
        pop     _c_ocr
        pop     _b_ddr
        pop     _b_ocr
        pop     _a_ddr
        pop     _a_ocr
        movw    _y, _saveY
        movw    _z, _saveZ
        out     _SFR_IO_ADDR(SREG), _saveSREG
        reti   

// Timer0 Compare Match ISR for driving LED-PWMs
// load current DDRB value and setup next OCR0A value
.global TIM0_COMPA_vect
TIM0_COMPA_vect:
        movw    _saveZ, _z
        movw    _z, _ocr_ptr
        ld      _saveSREG, z+
        out     _SFR_IO_ADDR(OCR0A), _saveSREG
        ld      _saveSREG, z+
        out     _SFR_IO_ADDR(DDRB), _saveSREG
        movw    _ocr_ptr, _z
        movw    _z, _saveZ
        reti        // 18 clocks

// Watchdog ISR for timing in Power Down Sleep Mode
.global WDT_vect
WDT_vect:
        in      _saveSREG, _SFR_IO_ADDR(SREG)
        ori     _flag, (1 << FLAG_UPDATE)
        out     _SFR_IO_ADDR(SREG), _saveSREG

.global ADC_vect
ADC_vect:
        reti
