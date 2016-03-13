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
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <util/crc16.h>
#include MCU_H

#include "avr-common.h"

#include "one-wire.h"
#include "ds1820.h"
#include "../include/wireless.h"

/**
 * The size of a message for a DS1820-based sensor.
 */
#define MSG_SIZE_DS1820     (WL_SENSOR_MSG_HDR_LEN + 3*WL_SENSOR_MSG_VALUE_LEN)

#define PIN_TX      PB1
#define PIN_SENSOR  PB2

/**
 * The number of watchdog interrupts (every 8 seconds) before we should
 * take a new reading and send a message. Currently gives us 64s cycles.
 */
#define WATCHDOG_INTR_THRESOLD      8

/**
 * The number of message we send before we should include a battery check.
 */
#define BATTERY_CHECK_THRESHOLD     60

/**
 * A counter of the number of watchdog interrupts we have received.
 */
static volatile int16_t intr_counter;

/**
 * A counter that is incremented for each message. This is used by the
 * receiver to determine if our state has changed.
 */
static volatile int16_t msg_counter;

/**
 * A counter of the number of messages we have sent. Used to decide when to
 * perform a voltage measurement.
 */
static volatile uint8_t batt_counter;

/**
 * Our station ID; retrieved from EEPROM.
 */
static uint8_t          station_id;

/*
 * Lookup table to convert from ADC readings to battery voltage.
 * First entry is 3.4V, thereafter descending by 0.1V for each step.
 */
static const uint16_t   adc_lut[]   PROGMEM = {341, 351, 363, 375, 388, 401, 416, 432, 450, 468, 489, 511, 535, 562, 592, 625, 661, 703, 750, 803, 865, 937, 1023};

/**
 * Delay 210us.
 *
 * Don't do it on one call as there are frequency-dependent
 * limitations on the max delay.
 */
static void inline
state_delay(void)
{
    _delay_us(50);
    _delay_us(50);
    _delay_us(50);
    _delay_us(50);
    _delay_us(10);
}

/**
 * Send one bit using Machester encoding.
 *
 *  0 = 0 -> 1
 *  1 = 1 -> 0
 *
 *  @param[in]  bit     The bit value to transmit.
 */
static void
send_bit(uint8_t bit)
{
    if (bit)
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

/**
 * Transmit one byte of data.
 *
 *  @param[in]  byte   The byte value to transmit.
 */
static void
send_byte(uint8_t byte)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        send_bit(byte & 0x01);
        byte >>= 1;
    }
}

/**
 * Transmit the preamble bit pattern.
 */
static void
send_preamble(void)
{
    for (uint8_t i = 0; i < 32; i++)
        send_bit(0);

    send_bit(1);
    send_bit(1);
}

/**
 * Transmit a message to the receiver.
 *
 * A message consists of:
 *  - preamble to allow the receiver to lock on to the signal
 *  - sync byte marking the start of the message
 *  - message length
 *  - message data
 *  - a CRC of (message length + data)
 *
 *  @param[in]  data    The data to transmit.
 *  @param[in]  length  The length of the message payload.
 */
static void
tx_message(const char *data, uint8_t length)
{
    uint8_t     crc = 0;

    sbi(DDRB, PIN_TX);
    cbi(PORTB, PIN_TX);

    send_preamble();

    // send sync
    send_byte(0xc4);

    // send msg size
    send_byte(length);
    crc = _crc_ibutton_update(crc, length);

    // send msg
    for (uint8_t i = 0; i < length; i++)
    {
        send_byte(data[i]);
        crc = _crc_ibutton_update(crc, data[i]);
    }

    // send CRC
    send_byte(crc);

    cbi(DDRB, PIN_TX);
    cbi(PORTB, PIN_TX);
}

/**
 * Take a temperature measurement and and transmit it to the receiver.
 */
static void
send_measurement(void)
{
    uint8_t temp, fract;
    int16_t t;
    int16_t adc;
    int     i;
    int16_t battery             = 0;
    uint8_t do_battery_check    = 0;
    char    msg[MSG_SIZE_DS1820];

    /*
     * Kick off an ADC conversion to check the battery (every 60 messages)
     */
    if (batt_counter == 0)
    {
        power_adc_enable();
        sbi(ADCSRA, ADSC);
        do_battery_check = 1;
    }

    batt_counter++;
    if (batt_counter >= BATTERY_CHECK_THRESHOLD)
        batt_counter = 0;

    /*
     * Retrieve the temperature from the sensor
     */
    ds1820_get_temperature(&temp, &fract);
    t = (temp << 3) + (temp << 1) + (fract << 2) + fract;

    if (do_battery_check)
    {
        /*
         * Wait for the ADC conversion to complete
         */
        while ((ADCSRA & (1<<ADSC)) != 0)
            continue;

        adc = ADCL;
        adc += (ADCH & 0x3) << 8;

        /*
         * Vbg / Vcc = adc / 1023
         * => Vcc = Vbg * 1023 / adc
         */
        battery = 34;
        for (i = 0; i < sizeof(adc_lut); i++)
        {
            uint16_t   threshold    = pgm_read_dword(&adc_lut[i]);

            if (threshold >= adc)
                break;

            --battery;
        }
    }

    /*
     * Construct a message. The message contains:
     *  - the ID of this transmitter
     *  - the temperature measurement
     *  - the message sequence number
     */
    WL_SENSOR_MSG_STATION_ID(msg) = station_id;
    WL_SENSOR_MSG_TYPE(msg, 0)  = WL_SENSOR_TYPE_TEMPERATURE;
    WL_SENSOR_MSG_VALUE(msg, 0) = t;
    WL_SENSOR_MSG_TYPE(msg, 1)  = WL_SENSOR_TYPE_COUNTER;
    WL_SENSOR_MSG_VALUE(msg, 1) = msg_counter;
    if (do_battery_check)
    {
        WL_SENSOR_MSG_TYPE(msg, 2)  = WL_SENSOR_TYPE_BATTERY;
        WL_SENSOR_MSG_VALUE(msg, 2) = battery;
        WL_SENSOR_MSG_NUM_VALUES(msg) = 3;
    }
    else
        WL_SENSOR_MSG_NUM_VALUES(msg) = 2;

    tx_message(msg, MSG_SIZE_DS1820);

    if (++msg_counter < 0)
        msg_counter = 0;
}

/**
 * Watchdog interrupt handler.
 *
 * Called every 8 seconds. Every 8th time this is called (i.e. every 64 seconds)
 * transmit a temperature measurement.
 */
ISR(WDT_vect)
{
    if (++intr_counter == WATCHDOG_INTR_THRESOLD)
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


/**
 * Save the state of MCUSR (so we know if we had a reset).
 *
 * Also disable the watchdog timer. This is called before main().
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
    /*
     * Setup the GPIO pin connected to the RF transmitter
     */
    sbi(DDRB, PIN_TX);
    cbi(PORTB, PIN_TX);

    /*
     * Read our station ID from the EEPROM
     */
    station_id = eeprom_read_byte(0x00);

    /*
     * Set up the 1-wire protocol connection to the DS1820
     */
    onewire_init(&DDRB, &PORTB, &PINB, PIN_SENSOR);

    /*
     * Set up the ADC
     *
     * ADMUX.REFS[2:0] = 000    => reference is Vcc
     * ADMUX.MUX[3:0] = 1100    => input is Vbg (1.1v)
     * ADCSRA.ADEN = 1          => enable ADC
     */
    sbi(ADMUX, MUX3);
    sbi(ADMUX, MUX2);
    sbi(ADCSRA, ADEN);
    sbi(ADCSRA, ADSC);  // start 1st conversion

    /*
     * Turn off unnecessary features to save power
     */
    power_adc_disable();
    power_usi_disable();

    /*
     * Set the watchdog timer to generate an interrupt after 8s. Bascially
     * we spend most of our time in hibernation, using the watchdog timer
     * to wake up.
     */
    wdt_enable(WDTO_8S);
    sbi(WDTCR, WDIE);

    sei();

    /*
     * Set sleep modes for low power consumption.
     */
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();

    /*
     * Busy loop where we just put ourselves to sleep
     */
    for (;;)
        sleep_mode();
}
