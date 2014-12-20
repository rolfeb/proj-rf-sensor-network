/*
 * Wireless Transmitter for DS1820 temperature sensor
 *
 * Copyright: Rolfe Bozier, rolfe@pobox.com, 2012
 */
#include <avr/io.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <avr/sleep.h>
#include <util/delay.h>
#include <util/crc16.h>
#include MCU_H

#include "avr-common.h"

#include "one-wire.h"
#include "ds1820.h"
#include "wireless.h"

#define MSG_SIZE_DS1820     (WL_SENSOR_MSG_HDR_LEN + 2*WL_SENSOR_MSG_VALUE_LEN)

#define PIN_TX      PB1
#define PIN_SENSOR  PB2

static void inline
state_delay(void)
{
    _delay_us(50);
    _delay_us(50);
    _delay_us(50);
    _delay_us(50);
    _delay_us(10);
}

static void
send_bit(uint8_t v)
{
    if (v)
    {
        sbi(PORTB, PIN_TX);
        state_delay();

        cbi(PORTB, PIN_TX);
        state_delay();
    }
    else
    {
        cbi(PORTB, PIN_TX);
        state_delay();

        sbi(PORTB, PIN_TX);
        state_delay();
    }
}

static void
send_byte(uint8_t v)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        send_bit(v & 0x01);
        v >>= 1;
    }
}

static void
send_preamble(void)
{
    for (uint8_t i = 0; i < 32; i++)
        send_bit(0);

    send_bit(1);
    send_bit(1);
}

static void
tx_message(char *text, uint8_t len)
{
    uint8_t     crc = 0;

    sbi(DDRB, PIN_TX);
    cbi(PORTB, PIN_TX);

    send_preamble();

    // send sync
    send_byte(0xc4);

    // send msg size
    send_byte(len);
    crc = _crc_ibutton_update(crc, len);

    // send msg
    for (uint8_t i = 0; i < len; i++)
    {
        send_byte(text[i]);
        crc = _crc_ibutton_update(crc, text[i]);
    }

    // send CRC
    send_byte(crc);

    cbi(DDRB, PIN_TX);
    cbi(PORTB, PIN_TX);
}

static volatile int16_t intr_counter;
static volatile int16_t msg_counter;
static uint8_t          station_id;

/*
 * Take a temperature measurement and and transmit it
 */
static void
send_measurement(void)
{
    uint8_t temp, fract;
    int16_t t;

    ds1820_get_temperature(&temp, &fract);
    t = temp * 10 + fract * 5;

    char msg[MSG_SIZE_DS1820];

    WL_SENSOR_MSG_STATION_ID(msg) = station_id;
    WL_SENSOR_MSG_NUM_VALUES(msg) = 2;
    WL_SENSOR_MSG_TYPE(msg, 0)  = WL_SENSOR_TYPE_TEMPERATURE;
    WL_SENSOR_MSG_VALUE(msg, 0) = t;
    WL_SENSOR_MSG_TYPE(msg, 1)  = WL_SENSOR_TYPE_COUNTER;
    WL_SENSOR_MSG_VALUE(msg, 1) = msg_counter;

    tx_message(msg, MSG_SIZE_DS1820);

    if (++msg_counter < 0)
        msg_counter = 0;
}

/*
 * Watchdog interrupt handler, called every 8 seconds
 */
ISR(WDT_vect)
{
    if (++intr_counter == 8)
    {
        send_measurement();
        intr_counter = 0;
    }

    sbi(WDTCR, WDIE);
}

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

int
main(void)
{
    sbi(DDRB, PIN_TX);
    cbi(PORTB, PIN_TX);

    /*
     * Read our station ID from the EEPROM
     */
    station_id = eeprom_read_byte(0x00);

    /*
     * Initialise 1-wire connection to the DS1820
     */
    onewire_init(&DDRB, &PORTB, &PINB, PIN_SENSOR);

    power_adc_disable();
    power_usi_disable();

    /*
     * Set the watchdog timer to generate an interrupt after 8s
     */
    wdt_enable(WDTO_8S);
    sbi(WDTCR, WDIE);

    sei();

    /*
     * Set sleep modes for low power consumption
     */
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();

    /*
     * Busy loop where we just put ourselves to sleep
     */
    for (;;)
        sleep_mode();
}
