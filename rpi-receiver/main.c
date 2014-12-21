/*
 * Wireless Receiver for Raspberry Pi
 *
 * Copyright: Rolfe Bozier, rolfe@pobox.com, 2012
 */
#include <string.h>

#include <avr/wdt.h>
#include <avr/eeprom.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include <util/twi.h>
#include MCU_H

#include "avr-common.h"

#include "../include/wireless.h"
#include "common.h"


/***************************************************************************
 * Message handling
 ***************************************************************************/

#define MAX_STATIONS    8

#define TX_BUFFER_SIZE  (3 + MAX_STATIONS * (WL_SENSOR_MSG_MAX_SIZE + 2))

static uint8_t              tx_buffer[TX_BUFFER_SIZE];
static uint8_t              *tx_eom;
static uint8_t              *tx_ptr;

typedef struct
{
    uint8_t         msg[WL_SENSOR_MSG_MAX_SIZE];
    clock_time_t    timestamp;
}
    station_info_t;

static station_info_t   stations[MAX_STATIONS];

/***************************************************************************
 * Watchdog management
 ***************************************************************************/

static volatile uint16_t    watchdog_timeouts;

static uint8_t mcusr_saved \
    __attribute__ ((section (".noinit")));

void
watchdog_init(void) \
    __attribute__((naked)) \
    __attribute__((section(".init3")));

/*
 * Save the state of MCUSR (so we know if we had a reset), and disable the
 * watchdog timer. This is called before main().
 */
void watchdog_init(void)
{
    mcusr_saved = MCUSR;
    MCUSR = 0;
    wdt_disable();
}

/***************************************************************************
 * Interrupt handlers
 ***************************************************************************/

ISR(WDT_vect)
{
    uint16_t    timeouts;

    timeouts = eeprom_read_word(0);
    timeouts++;
    eeprom_write_word(0, timeouts);
}

ISR(TWI_vect)
{
    uint8_t     twi_status;

    twi_status = TWSR & 0xf8;

    if (twi_status == TW_ST_SLA_ACK)
    {
        uint8_t         i;
        uint8_t         j;
        uint8_t         n_stations  = 0;
        uint8_t         n_bytes     = 1;
        uint16_t        v16;

        clock_time_t    now         = clock_time_unlocked();

        /*
         * SLA+R received, ACK has been sent
         *
         * Build up the message to send.
         */
        tx_eom = tx_buffer;

        for (i = 0; i < MAX_STATIONS; i++)
        {
            if (WL_SENSOR_MSG_STATION_ID(stations[i].msg) != 0)
            {
                n_stations++;

                n_bytes +=
                    4
                    +
                    WL_SENSOR_MSG_NUM_VALUES(stations[i].msg)
                    *
                    WL_SENSOR_MSG_VALUE_LEN;
            }
        }

        /*
         * Message header
         */
        *tx_eom++ = 0x01;       /* sensor message */
        *tx_eom++ = n_bytes;

        /*
         * Number of stations
         */
        *tx_eom++ = n_stations;

        for (i = 0; i < MAX_STATIONS; i++)
        {
            if (WL_SENSOR_MSG_STATION_ID(stations[i].msg) != 0)
            {
                /*
                 * Sensor information
                 */
                *tx_eom++ = WL_SENSOR_MSG_STATION_ID(stations[i].msg);
                *tx_eom++ = WL_SENSOR_MSG_NUM_VALUES(stations[i].msg);

                /*
                 * Sensor reading[s]
                 */
                for (j = 0; j < WL_SENSOR_MSG_NUM_VALUES(stations[i].msg); j++)
                {
                    v16 = WL_SENSOR_MSG_VALUE(stations[i].msg, j);

                    *tx_eom++ = WL_SENSOR_MSG_TYPE(stations[i].msg, j);
                    *tx_eom++ = ((uint8_t *)&v16)[0];
                    *tx_eom++ = ((uint8_t *)&v16)[1];
                }

                /*
                 * Sensor reading age
                 */
                if (now > stations[i].timestamp)
                    v16 = now - stations[i].timestamp;
                else
                    v16 = stations[i].timestamp = now;
                *tx_eom++ = ((uint8_t *)&v16)[0];
                *tx_eom++ = ((uint8_t *)&v16)[1];
            }
        }

#if 0
        tx_eom = tx_buffer;
        *tx_eom++ = 0x02;       /* error message */
        *tx_eom++ = 1;          /* size of message */
        *tx_eom++ = n_stations;       /* not implemented */
#endif

        tx_ptr = tx_buffer;
    }

    if (twi_status == TW_ST_SLA_ACK || twi_status == TW_ST_DATA_ACK)
    {
        /*
         * SLA+R received, ACK returned, or
         * data transmitted, ACK received
         *
         * Send the next byte in the message.
         */
        if (tx_eom > tx_buffer)
        {
            /*
             * Send another byte
             */
            if (tx_ptr < tx_eom - 1)
            {
                // send the next byte
                TWDR = *tx_ptr++;
                sbi(TWCR, TWEA);    // request an ACK
            }
            else
            {
                // send the last byte
                TWDR = *tx_ptr++;
                cbi(TWCR, TWEA);    // request an NACK

                // reset the transmit buffer
                tx_ptr = tx_eom = tx_buffer;
            }
        }
        else
        {
            /* XXX: should not occur... */
        }
    }
    else
    if (twi_status == TW_ST_DATA_NACK || twi_status == TW_ST_LAST_DATA)
    {
        /*
         * data transmitted, NACK received, or
         * last data byte transmitted, ACK received
         *
         * XXX: Mismatch between the amount of data we sent vs the amount that
         * was expected.
         */
        cbi(TWCR, TWSTA);
        cbi(TWCR, TWSTO);
        sbi(TWCR, TWEA);
    }
    else
    {
        /* XXX: should not occur... */
    }

    sbi(TWCR, TWINT);   // wait for next message
}

/***************************************************************************
 * Main program
 ***************************************************************************/


int
main(void)
{
    uint8_t         i;

    /*
     * Initialise all ports to default
     */
    sbi(DDRB, PB0); sbi(PORTB, PB0);
    sbi(DDRB, PB1); sbi(PORTB, PB1);
    sbi(DDRB, PB2); sbi(PORTB, PB2);
    sbi(DDRB, PB3); sbi(PORTB, PB3);
    sbi(DDRB, PB4); sbi(PORTB, PB4);
    sbi(DDRB, PB5); sbi(PORTB, PB5);

    sbi(DDRC, PC0); sbi(PORTC, PC0);
    sbi(DDRC, PC1); sbi(PORTC, PC1);
    sbi(DDRC, PC2); sbi(PORTC, PC2);
    sbi(DDRC, PC3); sbi(PORTC, PC3);
    sbi(DDRC, PC4); sbi(PORTC, PC4);
    sbi(DDRC, PC5); sbi(PORTC, PC5);

    sbi(DDRD, PD0); sbi(PORTD, PD0);
    sbi(DDRD, PD1); sbi(PORTD, PD1);
    sbi(DDRD, PD2); sbi(PORTD, PD2);
    sbi(DDRD, PD3); sbi(PORTD, PD3);
    sbi(DDRD, PD4); sbi(PORTD, PD4);
    sbi(DDRD, PD5); sbi(PORTD, PD5);
    sbi(DDRD, PD6); sbi(PORTD, PD6);
    sbi(DDRD, PD7); sbi(PORTD, PD7);


    /*
     * Initialise various modules
     */
    clock_init();

    /*
     * Initialise the TWI module
     */
    sbi(AVR_I2C_DDR, AVR_I2C_PORT_SCL);     // SCL pin in input mode
    sbi(AVR_I2C_PORT, AVR_I2C_PORT_SCL);    // enable internal pullup

    sbi(AVR_I2C_DDR, AVR_I2C_PORT_SDA);     // SDA pin in input mode
    sbi(AVR_I2C_PORT, AVR_I2C_PORT_SDA);    // enable internal pullup

    // set prescaler to 1
    TWSR = 0;

    // set clock to 100kHz
    TWBR = ((F_CPU / (long)AVR_I2C_CLOCK_HZ) - 16) / 2;

    // set our address (ignore general call)
    TWAR = 0x41 << 1;

    // initialise I2C for listening
    TWCR = (1<<TWEN) | (1<<TWEA) | (1<<TWIE);

    /*
     * Initialise the connection to the wireless receiver
     */
    wireless_init(&DDRC, &PORTC, &PINC, PC0);
    // XXX: wireless_init(&DDRD, &PORTD, &PIND, PD7);

    /*
     * Set up the watchdog timer in case something hangs somewhere
     */
    sbi(WDTCSR, WDCE);
    sbi(WDTCSR, WDE);
    wdt_enable(WDTO_8S);
    sbi(WDTCSR, WDIE);

    /* DEBUG */
    /*
    sbi(DDRA, PA0); cbi(PORTA, PA0);
    sbi(DDRA, PA1); cbi(PORTA, PA1);
    sbi(DDRA, PA2); cbi(PORTA, PA2);
    sbi(DDRA, PA3); cbi(PORTA, PA3);
    sbi(DDRA, PA4); cbi(PORTA, PA4);
    sbi(DDRA, PA5); cbi(PORTA, PA5);
    sbi(DDRA, PA6); cbi(PORTA, PA6);
    sbi(DDRA, PA7); cbi(PORTA, PA7);
    */

    tx_ptr = tx_eom = tx_buffer;

    sei();

    for (;;)
    {
        /*
         * Received a message from the wireless receiver?
         */
        if (msg_pending)
        {
            uint8_t     n   = 0xff;

            /*
             * Find the slot in stations[] or allocate a new one
             * 
             * XXX: if there is a stale entry, use that
             */
            for (i = 0; i < MAX_STATIONS; i++)
            {
                if
                (
                    WL_SENSOR_MSG_STATION_ID(stations[i].msg)
                    ==
                    WL_SENSOR_MSG_STATION_ID(msg_buffer)
                )
                {
                    n = i;
                    break;
                }
                else
                if (WL_SENSOR_MSG_STATION_ID(stations[i].msg) == 0)
                    n = i;
            }

            /*
             * Copy the message into stations[]. If there was no slot found,
             * just drop the message.
             */
            if (n != 0xff)
            {
                for (i = 0; i < WL_SENSOR_MSG_MAX_SIZE; i++)
                    stations[n].msg[i] = msg_buffer[i];

                stations[n].timestamp = clock_time();
            }

            msg_pending = 0;
        }

        wdt_reset();
    }

    return 0;
}
