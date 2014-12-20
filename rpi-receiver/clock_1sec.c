/*
 * Wireless Receiver for Raspberry Pi
 *
 * Copyright: Rolfe Bozier, rolfe@pobox.com, 2012
 */
#include <avr/interrupt.h>

#include "avr-common.h"
#include "common.h"

/*
 * A 1-second counter.
 * 
 * We need to use Timer0 (8-bit) as Timer1 is used in the radio receiver.
 * The maximum we can count up to is 1024 * 256 (prescaler + max count),
 * so we need to increment the seconds counter every 38 interrupts.
 */

static volatile clock_time_t    clock_1sec;
static volatile uint8_t         clock_tick_ctr;

ISR(TIMER0_OVF_vect)
{
    if (++clock_tick_ctr == 38)
    {
        clock_tick_ctr = 0;
        clock_1sec++;
    }
}

void
clock_init(void)
{
    /*
     * Set Timer/Counter0 mode to Normal (overflow)
     * WGM12:0 = 000
     */
    cbi(TCCR0B, WGM02);
    cbi(TCCR0A, WGM01);
    cbi(TCCR0A, WGM00);

    /*
     * Set pre-scaler to /1024
     * CS12:0 = 101
     */
    sbi(TCCR0B, CS02);
    cbi(TCCR0B, CS01);
    sbi(TCCR0B, CS00);

    /*
     * Enable Timer/Counter0 MAX interrupt
     */
    sbi(TIMSK0, TOIE0);

    clock_1sec = 0;
    clock_tick_ctr = 0;

    /*
     * Enable Timer0
     */
    cbi(PRR, PRTIM0);
}

/*
 * Get the current clock value
 */
clock_time_t clock_time(void)
{
    clock_time_t t;

    cli();
    t = clock_1sec;
    sei();

    return t;
}

/*
 * Get the current clock value, but don't lock. Used it we know interrupts are
 * disabled.
 */
clock_time_t clock_time_unlocked(void)
{
    return clock_1sec;
}
