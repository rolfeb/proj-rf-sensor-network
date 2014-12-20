#include <stdio.h>
#include <string.h>

#include <util/delay.h>
#include <util/crc16.h>
#include MCU_H

#include "avr-common.h"
#include "wireless.h"

static volatile uint8_t *rx_ddr;
static volatile uint8_t *rx_porto;
static volatile uint8_t *rx_porti;
static volatile uint8_t rx_pin;

#define MSG_MAX_LENGTH      WL_SENSOR_MSG_MAX_SIZE

typedef enum
{
    RS_IDLE       = 0,
    RS_WANT_LENGTH,
    RS_WANT_MESSAGE,
    RS_WANT_CRC,
}
    recv_state_t;

static volatile recv_state_t  recv_state  = RS_IDLE;

static volatile uint8_t     msg_length;
static volatile uint8_t     msg_crc;
static volatile char        *msg_wrptr;

char                        msg_buffer[MSG_MAX_LENGTH];
volatile uint8_t            msg_pending     = 0;
volatile uint8_t            msg_error       = 0;

typedef enum
{
    UNSYNC       = 0,
    SYNCING,
    SYNCED,
    GOTHDR,
    GOTLEN,
    GOTMSG,
}
    state_t;


static inline void push_msg_byte(uint8_t v)
{
    if (msg_wrptr == &msg_buffer[MSG_MAX_LENGTH-1])
    {
        msg_buffer[0] = v;
        msg_wrptr = msg_buffer;
    }
    else
        *++msg_wrptr= v;
}

static volatile uint16_t    sample_bits;
static volatile state_t     state           = UNSYNC;
static volatile uint8_t     sample_ctr;

static volatile uint8_t     current_byte;
static volatile uint8_t     bit_ctr;

ISR(TIMER1_COMPA_vect)
{
    // sbi(PORTA, PA7);    // DEBUG: enter interrupt handler
    // cbi(PORTA, PA5);    // DEBUG: lost sync
    // cbi(PORTA, PA4);    // DEBUG: EOB
    // cbi(PORTA, PA3);    // DEBUG: sampling

    /*
     * Read in another sample for a bit value.
     */
    sample_bits <<= 1;
    if (*rx_porti & (1 << rx_pin))
    {
        // sbi(PORTA, PA6);    // DEBUG: sample
        sample_bits |= 1;
    }
    else
    {
        // cbi(PORTA, PA6);    // DEBUG: sample
    }

    if (state == UNSYNC)
    {
        /*
         * The initial sync matches a set of 16 bits with a 0->1 (manchester 0)
         * bit in the middle.
         */
        if ((sample_bits & 0x1ff8) == 0x00f8)
        // if ((sample_bits & 0x3ffc) == 0x00fc)
        // if (sample_bits == 0x00ff)
        {
            state = SYNCING;
            // PORTA = (PORTA & 0xf8) | (state & 0x7);
            sample_ctr = 16;
        }
    }
    else
    {
        if (--sample_ctr == 0)
        {
            uint8_t     m;

            // sbi(PORTA, PA3);    // DEBUG: EOM

            /*
             * Look for a transition in the middle of the 16-bit sample. A
             * transition is either 0->1 or 1->0.  To implement a poor man's
             * PLL, we set the number of bits to sample to try to get the
             * next transition in the middle of the sample.
             */
            if ((m = (sample_bits >> 2) & 0x3) == 1 || m == 2)
                sample_ctr = 21;
            else
            if ((m = (sample_bits >> 3) & 0x3) == 1 || m == 2)
                sample_ctr = 20;
            else
            if ((m = (sample_bits >> 4) & 0x3) == 1 || m == 2)
                sample_ctr = 19;
            else
            if ((m = (sample_bits >> 5) & 0x3) == 1 || m == 2)
                sample_ctr = 18;
            else
            if ((m = (sample_bits >> 6) & 0x3) == 1 || m == 2)
                sample_ctr = 17;
            else
            if ((m = (sample_bits >> 7) & 0x3) == 1 || m == 2)
                sample_ctr = 16;
            else
            if ((m = (sample_bits >> 8) & 0x3) == 1 || m == 2)
                sample_ctr = 15;
            else
            if ((m = (sample_bits >> 9) & 0x3) == 1 || m == 2)
                sample_ctr = 14;
            else
            if ((m = (sample_bits >> 10) & 0x3) == 1 || m == 2)
                sample_ctr = 13;
            else
            if ((m = (sample_bits >> 11) & 0x3) == 1 || m == 2)
                sample_ctr = 14;
            else
            if ((m = (sample_bits >> 12) & 0x3) == 1 || m == 2)
                sample_ctr = 15;
            else
            {
                /*
                 * Didn't find a transition, so go back to the UNSYNC state.
                 */
                state = UNSYNC;
                // sbi(PORTA, PA5);    // DEBUG: lost sync
                // PORTA = (PORTA & 0xf8) | (state & 0x7);
            }

            if (state == SYNCING)
            {

                /*
                 * Accumulate the newly-sampled bit into the current byte
                 * (at the MSB end).
                 */
                current_byte = (current_byte >> 1) | (m == 2 ? (1 << 7) : 0);

                /*
                 * We end the sync stream of zeroes with 2 1-bits.
                 */
                if (current_byte == 0xc0)
                {
                    state = SYNCED;
                    // PORTA = (PORTA & 0xf8) | (state & 0x7);
                    bit_ctr = 0;
                }
            }
            else
            if (state != UNSYNC)
            {
                /*
                 * Accumulate the newly-sampled bit into the current byte
                 * (at the MSB end).
                 */
                current_byte = (current_byte >> 1) | (m == 2 ? (1 << 7) : 0);
                bit_ctr++;

                if (bit_ctr == 8)
                {
                    // sbi(PORTA, PA4);    // DEBUG: EOB

                    /*
                     * We've accepted a complete byte. Use this to manage the
                     * FSM state.
                     */
                    if (state == SYNCED)
                    {
                        /*
                         * First byte in a message is a 0xc4 header byte
                         */
                        if (current_byte == 0xc4)
                        {
                            state = GOTHDR;
                            // PORTA = (PORTA & 0xf8) | (state & 0x7);
                        }
                    }
                    else
                    if (state == GOTHDR)
                    {
                        /*
                         * Second byte in a message is the message length. We
                         * start to accumulate bytes into a CRC value.
                         */
                        msg_length = current_byte;
                        msg_crc = _crc_ibutton_update(0, current_byte);

                        msg_wrptr = msg_buffer;

                        state = GOTLEN;
                        // PORTA = (PORTA & 0xf8) | (state & 0x7);
                    }
                    else
                    if (state == GOTLEN)
                    {
                        /*
                         * Include another message byte.
                         */
                        *msg_wrptr++ = current_byte;
                        msg_crc = _crc_ibutton_update(msg_crc, current_byte);

                        if (msg_wrptr >= msg_buffer + msg_length)
                        {
                            state = GOTMSG;
                            // PORTA = (PORTA & 0xf8) | (state & 0x7);
                        }
                    }
                    else
                    if (state == GOTMSG)
                    {
                        /*
                         * Check the message CRC matches what we have received.
                         */
                        if (msg_crc == current_byte)
                        {
                            // CRC check succeeded
                            *msg_wrptr++ = '\0';
                            msg_pending = 1;
                        }
                        else
                            msg_error = 1;

                        /*
                         * Reset back to the UNSYNC state.
                         */
                        state = UNSYNC;
                        // PORTA = (PORTA & 0xf8) | (state & 0x7);
                    }

                    /*
                     * Reset the current byte
                     */
                    current_byte = 0;
                    bit_ctr = 0;
                }
            }
        }
    }
    // cbi(PORTA, PA7);    // DEBUG: leave interrupt handler
}


void
wireless_init(volatile uint8_t *ddr, volatile uint8_t *porto, volatile uint8_t *porti, uint8_t pin)
{
    rx_ddr = ddr;
    rx_porto = porto;
    rx_porti = porti;
    rx_pin = pin;

    cbi(*rx_ddr, rx_pin);
    cbi(*rx_porto, rx_pin);   // deactivate pull-up resistor

    /*
     * Set the high-speed sampling counter. We use Timer1 without a prescaler
     * to get higher precision with a non-2^n clock.
     */
    // set prescaler = off (clkT1 = clkIO)
    cbi(TCCR1B, CS12);
    cbi(TCCR1B, CS11);
    sbi(TCCR1B, CS10);

    // set CTC mode using OCR1A as max
    cbi(TCCR1B, WGM13);
    sbi(TCCR1B, WGM12);
    cbi(TCCR1A, WGM11);
    cbi(TCCR1A, WGM10);

    // set timer value = 260 = 0x0104
    OCR1AH = 0x01;
    OCR1AL = 0x04;

    // send interrupt on timeout
    sbi(TIMSK1, OCIE1A);

    // reset counter value
    TCNT1H = 0;
    TCNT1L = 0;

    // enable Timer1 in Power Reduction Register
    cbi(PRR, PRTIM1);

    // make OC1A visible on I/O pin
    sbi(DDRD, PD5);

    // OC1A pin = toggle on match
    cbi(TCCR1A, COM1A1);
    sbi(TCCR1A, COM1A0);

    msg_wrptr = msg_buffer;

    // PORTA = 0;      // DEBUG
}
