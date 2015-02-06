#ifndef polynom
#define polynom  0xC3A8AD09
#endif

        .section .bss
        .global seed
seed:   .long   0

        .text
        .global lfsr
        .type   lfsr, @function
lfsr:
        ldi     r23, lo8(polynom >> 24)
        ldi     r22, lo8(polynom >> 16)
        ldi     r21, lo8(polynom >> 8)
        ldi     r20, lo8(polynom)

        .global lfsr_poly
        .type   lfsr_poly, @function
lfsr_poly:

        mov     r0, r24
        clr     r24
        clr     r25
        ldi     r31, hi8(seed)
        ldi     r30, lo8(seed)
        ldd     r27, z +3
        ldd     r26, z +2
        ldd     r19, z +1
        ldd     r18, z +0
10:     lsr     r27
        ror     r26
        ror     r19
        ror     r18
        brcc    11f
        eor     r27, r23
        eor     r26, r22
        eor     r19, r21
        eor     r18, r20
11:     adc     r24, r24
        adc     r25, r25
        dec     r0
        brne    10b
        std     z +3, r27
        std     z +2, r26
        std     z +1, r19
        std     z +0, r18
        ret

.end
