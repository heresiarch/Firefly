#include "firefly2.h"


#define USE_LDR 1
//#define USE_RESET_LDR 1

firefly_t fireflies[12];                            // 12 fireflies
uint8_t   ocr_data[6];                              // OCR0A and DDRB datas for Timer0 Compare Match ISR, computed PWM Dutycycles
uint16_t  hungry_threshold;                         // threshold for lightning fireflies

// 12 fixed registers assigned to speedup ISRs, Timer0 Overflow and Output Compare
register uint16_t  _saveZ    asm("r2");             // save Z register
register uint16_t  _saveY    asm("r4");             // save Y register
register uint8_t   _saveSREG asm("r6");             // save SREG
register uint8_t   _zero     asm("r7");             // zero Register
register uint8_t*  _ocr_ptr  asm("r8");             // pointer to current ocr_data[] entry
register firefly_p _fly_ptr  asm("r10");            // pointer to current firefly[] entry
register uint8_t   _ddr_idx  asm("r16");            // index into ddr_data[] entry, 0 upto 15 in +4 steps
register uint8_t   flags     asm("r17");

#ifdef USE_LDR
// threshold for LDR
#define LIGHT_THRESHOLD 990
#else
// threshold for solar cell
#define LIGHT_THRESHOLD 512
#endif
#define BAT_THRESHOLD 512

static void init(void)
{
    // init global registers
    _fly_ptr = (firefly_p)&fireflies;
    _zero    = 0;
    _ddr_idx = 0;
    flags    = 0;
    // enable watchdog ISR
    wdt_reset();
    WDTCR |= (1 << WDE) | (1 << WDCE);
    WDTCR  = (1 << WDIE);
    // Port
    PORTB  = 0;
    DDRB   = 0;
    // Power Reduction, enable ADC for setup
    PRR    = (1 << PRTIM1) | (1 << PRTIM0) | (1 << PRUSI);
    //  AC/ADC
    ACSR   = (1 << ACD);
    ADCSRA = 0;
    ADCSRB = 0;
    DIDR0  = (1 << ADC0D) | (1 << ADC2D) | (1 << ADC3D) | (1 << AIN1D) | (1 << AIN0D);
    // Power Reduction, enable Timer0
    PRR    = (1 << PRTIM1) | (1 << PRUSI) | (1 << PRADC);
    // Pin Change Mask on PB2, for calling Bootloader
    // GIMSK  = 0;
    //PCMSK  = (1 << PCINT2);
    // Timer0, Overflow & Compare Match ISR used
    TCCR0A = 0;
    TCCR0B = 0;
    OCR0A  = 0;
    TCNT0  = 0;
    TIMSK  = (1 << OCIE0A) | (1 << TOIE0);
    TIFR   = (1 << OCF0A) | (1 << TOV0);
    // init PRNG, load last seed from EEPROM, calculate next seed and store back to EEPROM, use another polynom as in lfsr()
    eeprom_read_block((uint8_t *)&seed, (uint8_t *)(0), 4);
    lfsr_poly(32, 0x8140C9D5);
    eeprom_write_block((uint8_t *)&seed, (uint8_t *)(0), 4);
}

static uint16_t update_fireflies(void)
{

    // update waves
    firefly_p fly = (firefly_p)&fireflies;
    firefly_p cur;
    for (uint8_t i = 12; i > 0; i--)
    {
        if ((fly->wave_ptr == 0) && (fly->hungry == 0))
        {
            // choose wave for lightning from a set of 32 waves randomly
            uint16_t* data = (uint16_t*)&wave_data[lfsr(5)];
            cur = fly;
            uint16_t ptr = pgm_read_word_inc(data);
            cur->wave_end = pgm_read_word_inc(data);
            cli();
            cur->wave_ptr = ptr;
            flags |= FLAG_TIMER;
            sei();
            // update next energy points, each neighbor fly becomes half of energy points
            uint16_t energy = pgm_read_word_inc(data);
            while (energy > 0)
            {
                cur->energy += energy;
                if (++cur >= &fireflies[12])
                    cur = (firefly_p)&fireflies;
                energy /= 2;
            }
        }
        fly++;
    }
    // calc feeding step and update fireflies hungry value
    uint16_t food = 0xFFFF;
    uint16_t threshold = hungry_threshold;
    fly = (firefly_p)&fireflies;
    for (uint8_t i = 12; i > 0; i--)
    {
        uint16_t hungry = fly->hungry;
        if (fly->energy > 0)
        {
            if ((hungry == 0) || (hungry > threshold))
            {
                uint16_t max = 0xFFFF - fly->energy;
                if (hungry < max) hungry += fly->energy;
                else hungry = 0xFFFF;
                if (threshold < 0xFFF0) threshold += 3;
            }
            else if (threshold > 1)
            {
                threshold -= 2;
            }
        }
        if (hungry < food) food = hungry;
        fly->hungry = hungry;
        fly->energy = 0;
        fly++;
    }
    // hungry_threshold ensures that fireflies with low hungry is ligthning
    // early and so we get a ligthing in periodic intervals of groups of fireflies,
    // eg. or some longer delays of no lightning dependend on how many energy is
    // consumed by the ligthning waves
    hungry_threshold = threshold;
    // feeding
    fly = (firefly_p)&fireflies;
    for (uint8_t i = 12; i > 0; i--)
    {
        fly->hungry -= food;
        fly++;
    }
    // return minimal watchdog timer cycles for sleep mode, eg. food
    return food;
}

static void measure_isnight(void)
{
	flags &= ~FLAG_ISNIGHT;

#ifdef USE_LDR

#ifdef USE_RESET_LDR
    // LDR is connected at ADC Pin via 10kOhm. 15kOhm pullup goes to PB5 aka "RESET".
    // We set PB5 Pin to high and measure the voltage.

    // set "RESET" Pin high
    DDRB  |= (1 << PB5);
    PORTB |= (1 << PB5);
#else
    // LDR is connected at ADC Pin via 10kOhm and has parallel 10nF Cap.
    // We set ADC Pin to high to load the cap for a while and afterwards
    // measure the voltage.

    // set ADC pin as output
    DDRB  |= (1 << PB4);
    PORTB |= (1 << PB4);
#endif
#endif
    // enable ADC Noise Reduction Sleep Mode
    MCUCR  = (1 << SE) | (1 << SM0);

    PRR    = (1 << PRTIM1) | (1 << PRTIM0) | (1 << PRUSI);
    ADCSRA = (1 << ADEN) | (1 << ADIE) | (1 << ADPS2) | (1 << ADPS0) | (1 << ADSC);

    // Vcc as reference, REFS0, REFS1 = 0, MUX3..0 1100 -> measure Bandgap
    ADMUX  = (1 << MUX3) | (1 << MUX2);

    // do a lot of measures that are discarded, because due to the
    // high impedance of the bandgap the sample & hold needs some time to settle..
    sleep_cpu();
    sleep_cpu();
    sleep_cpu();
    sleep_cpu();
    sleep_cpu();
    sleep_cpu();
    sleep_cpu();
    sleep_cpu();
    sleep_cpu();
    sleep_cpu();
    sleep_cpu();
    sleep_cpu();
    sleep_cpu();
    sleep_cpu();
    sleep_cpu();
    sleep_cpu();

    // read battery voltage
    uint16_t bat_volt = ADC;    // 280 ~ 4V, 420 ~ 2.7V, 400 ~ 2.8V, 512 ~ 2.2V

#ifdef USE_LDR
#ifndef USE_RESET_LDR
    // set ADC pin as input and no pull-up
    DDRB  &= ~(1 << PB4);
    PORTB &= ~(1 << PB4);
#endif
#endif
    // read solar panel voltage
    // also use Vcc as reference, we measure negative pin of solar panel
    // so it should be zero if the cell is feeding the batteries
    ADMUX  = (1 << MUX1);

    sleep_cpu();

    uint16_t sol_volt = ADC;

#ifdef USE_RESET_LDR
    DDRB  &= ~(1 << PB5);
    PORTB &= ~(1 << PB5);
#endif
    // sol_volt > 512 means the negative connector of the solar
    // cell is at Vcc / 2
    // bat_volt < 512 means the 1.1V reference that we measure
    // is less than Vcc / 2, i.e. Vcc (the battery) has more
    // than 2 * 1.1V
    if ((sol_volt > LIGHT_THRESHOLD) && (bat_volt < BAT_THRESHOLD))
        flags |= FLAG_ISNIGHT;

    // disable ADC
    ADCSRA = 0;
    PRR    = (1 << PRTIM1) | (1 << PRUSI) | (1 << PRADC);
}

static uint16_t wdt_setup(uint16_t time)
{

    // setup watchdog to next highest timeout that match time
    uint16_t res = 1;
    uint8_t  idx = 1;
    while ((res <= time) && (idx < 10))
    {
        res += res;
        idx++;
    }
    res /= 2;

    if (--idx > 7)
        idx = (idx & 0x07) | (1 << WDP3);
    idx |= (1 << WDIE);                                                 // enable WDT-ISR
    wdt_reset();
    WDTCR |= (1 << WDE) | (1 << WDCE);
    WDTCR  = idx;
    return res;
}

void resetExternalWDT(void)
{
	DDRB |= (1 << 4);
	PORTB &= ~(1 << 4);	/* LED on */
	_delay_ms(1000);
	PORTB |= (1 << 4);	/* LED off */
}

int main(void)
{

    init();
    sei();

    uint16_t timeout = 0;   // remaining time in WDT cycles a 16ms upto next event
    uint16_t measure = 0;   // remaining time upto next measurement

    while (1)
    {
    	if(timeout > 0x7fff)
    	{
    	   	timeout = 0;
    	}

    	// check for update, FLAG_UPDATE is set in Watchdog ISR
        if (flags & FLAG_UPDATE)
        {
            flags &= ~FLAG_UPDATE;
            if (timeout == 0)
            {
                if (flags & FLAG_ISNIGHT)
                {
                    timeout = update_fireflies();
                    if (measure <= timeout)
                    {
                        measure = MEASURE_INTERVAL;
                        flags |= FLAG_MEASURE;
                    }
                    else
                        measure -= timeout;
                }
                else
                {
                    measure = MEASURE_INTERVAL;
                    timeout = measure;
                    flags |= FLAG_MEASURE;
                }
            }
            timeout -= wdt_setup(timeout);

        }
        //
        resetExternalWDT();
        // check if we playing waves
        if (flags & FLAG_TIMER)
        {
            // enable PWM-Timer0, XTAL / 64
            TCCR0B = (1 << CS01) | (1 << CS00);
            // sleep mode idle
            MCUCR  = (1 << SE);
            sleep_cpu();
        }
        else
        {
            // disable Timer0
            TCCR0B = 0;
            OCR0A  = 0;
            TCNT0  = 0;
            // enable deep Power Down Sleep Mode
            MCUCR  = (1 << SE) | (1 << SM1);
            sleep_cpu();
            // check if we must measure battery and solar panel
            if (flags & FLAG_MEASURE)
            {
                flags &= ~FLAG_MEASURE;
                measure_isnight();
            }
        }
    }
}



