/*
 * Copyright 2013-2023 Chris Rhodin <chris@notav8.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "project.h"

#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

#include "timer.h"
#include "pinmap.h"
#include "tm1638.h"


#define TM1638_DELAY_US          (1)

/* TM1638 commands                  */
#define TM1638_CMD_DATA         0x40
#define TM1638_CMD_ADDRESS      0xC0
#define TM1638_CMD_DISPLAY      0x80

/* TM1638 data command bitfields    */
#define TM1638_DATA_WRITE       0x00
#define TM1638_DATA_READ        0x02
#define TM1638_DATA_INCR        0x00
#define TM1638_DATA_FIXED       0x04

/* TM1638 address command bitfields */
#define TM1638_ADDRESS_MASK     0x0F

/* TM1638 display command bitfields */
#define TM1638_DISPLAY_BRIGHT   0x07
#define TM1638_DISPLAY_ON       0x08


/* command identifier */
#define TM1638_IDLE              (0)
#define TM1638_WRITE_CONFIG   _BV(0)
#define TM1638_READ_KEYS      _BV(1)
#define TM1638_WRITE_SEGMENTS _BV(2)


/*
 * segments buffer for LED display
 */
static uint8_t segments_buffer[16];

/*
 * keys buffer for keyboard
 */
static uint32_t keys_buffer = 0;

/*
 * default to display on at 1/2 maximum brightness
 */
static uint8_t _config = TM1638_CMD_DISPLAY
                       | TM1638_DISPLAY_ON
                       | (TM1638_MAX_BRIGHTNESS / 2);

/*
 * command state variables
 */
static uint8_t pending_command = TM1638_IDLE;
static uint8_t active_command = TM1638_IDLE;
static uint8_t state;
static uint8_t * data;


static void __TM1638_write_config(void)
{
    if (!(GPIOR0 & TM1638_EV_BUSY))
    {
        GPIOR0 |= TM1638_EV_BUSY;
        pending_command &= ~TM1638_WRITE_CONFIG;

        _delay_us(TM1638_DELAY_US);
        TM1638_STB_LOW();
        _delay_us(TM1638_DELAY_US);

        /* write the first byte */
        SPDR = _config;

        /* set the command */
        active_command = TM1638_WRITE_CONFIG;
        state = 0;

        /* enable SPI interrupt */
        SPCR |= _BV(SPIE);
    }
}

static void __TM1638_read_keys(void)
{
    if (!(GPIOR0 & TM1638_EV_BUSY))
    {
        GPIOR0 |= TM1638_EV_BUSY;
        pending_command &= ~TM1638_READ_KEYS;

        _delay_us(TM1638_DELAY_US);
        TM1638_STB_LOW();
        _delay_us(TM1638_DELAY_US);

        /* write the first byte */
        SPDR = TM1638_CMD_DATA | TM1638_DATA_READ | TM1638_DATA_INCR;

        /* set the command */
        active_command = TM1638_READ_KEYS;
        state = 0;
        data = (uint8_t *) &keys_buffer;

        /* enable SPI interrupt */
        SPCR |= _BV(SPIE);
    }
}

static void __TM1638_write_segments(void)
{
    if (!(GPIOR0 & TM1638_EV_BUSY))
    {
        GPIOR0 |= TM1638_EV_BUSY;
        pending_command &= ~TM1638_WRITE_SEGMENTS;

        _delay_us(TM1638_DELAY_US);
        TM1638_STB_LOW();
        _delay_us(TM1638_DELAY_US);

        /* write the first byte */
        SPDR = TM1638_CMD_DATA | TM1638_DATA_WRITE | TM1638_DATA_INCR;

        /* set the command */
        active_command = TM1638_WRITE_SEGMENTS;
        state = 0;
        data = &segments_buffer[0];

        /* enable SPI interrupt */
        SPCR |= _BV(SPIE);
    }
}

static void __TM1638_command_dispatch(void)
{
    if (!(GPIOR0 & TM1638_EV_BUSY))
    {
        _delay_us(TM1638_DELAY_US);
        TM1638_STB_HIGH();

        SPCR &= ~_BV(SPIE);
        active_command = pending_command & ~(pending_command - 1);
        pending_command &= ~active_command;

        switch (active_command)
        {
        case TM1638_WRITE_CONFIG:
            __TM1638_write_config();
            break;

        case TM1638_READ_KEYS:
            __TM1638_read_keys();
            break;

        case TM1638_WRITE_SEGMENTS:
            __TM1638_write_segments();
            break;
        }
    }
}


ISR(SPI_STC_vect)
{
    switch (active_command)
    {
    case TM1638_WRITE_CONFIG:
        GPIOR0 &= ~TM1638_EV_BUSY;
        break;

    case TM1638_READ_KEYS:
        if      (0 == state)  /* command state 0 */
        {
            pinmap_dir(PINMAP_MOSI, 0);
            SPDR = 0xFF;
        }
        else if (4 > state)   /* command state 1 thru 3 */
        {
            *data++ = SPDR;
            SPDR = 0xFF;
        }
        else                  /* command state 4 */
        {
            *data++ = SPDR;
            pinmap_dir(0, PINMAP_MOSI);

            GPIOR0 &= ~TM1638_EV_BUSY;
        }
        break;

    case TM1638_WRITE_SEGMENTS:
        if      (0 == state)  /* command state 0 */
        {
            _delay_us(TM1638_DELAY_US);
            TM1638_STB_HIGH();
            _delay_us(TM1638_DELAY_US);
            TM1638_STB_LOW();
            _delay_us(TM1638_DELAY_US);
            SPDR = TM1638_CMD_ADDRESS | (0 & TM1638_ADDRESS_MASK);
        }
        else if (16 >= state) /* command state 1 thru 16 */
        {
            SPDR = *data++;
        }
        else                  /* command state 17 */
        {
            GPIOR0 &= ~TM1638_EV_BUSY;
        }
        break;

    default:
        GPIOR0 &= ~TM1638_EV_BUSY;
        break;
    }

    state++;

    if (!(GPIOR0 & TM1638_EV_BUSY))
    {
        _delay_us(TM1638_DELAY_US);
        TM1638_STB_HIGH();

        SPCR &= ~_BV(SPIE);
        active_command = pending_command & ~(pending_command - 1);
        pending_command &= ~active_command;

        switch (active_command)
        {
        case TM1638_WRITE_CONFIG:
            __TM1638_write_config();
            break;

        case TM1638_READ_KEYS:
            __TM1638_read_keys();
            break;

        case TM1638_WRITE_SEGMENTS:
            __TM1638_write_segments();
            break;
        }
    }
}


static void TM1638_write_config(void)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        pending_command |= TM1638_WRITE_CONFIG;
        __TM1638_write_config();
    }
}

void TM1638_read_keys(void)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        pending_command |= TM1638_READ_KEYS;
        __TM1638_read_keys();
    }
}

void TM1638_write_segments(void)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        pending_command |= TM1638_WRITE_SEGMENTS;
        __TM1638_write_segments();
    }
}

uint32_t TM1638_get_keys(void)
{
    return keys_buffer;
}

void TM1638_enable(uint8_t const enable)
{
    _config = (_config & ~TM1638_DISPLAY_ON)
            | (enable ? TM1638_DISPLAY_ON : 0);

    TM1638_write_config();
}

void TM1638_brightness(uint8_t const brightness)
{
    _config = (_config & ~TM1638_DISPLAY_BRIGHT)
            | (brightness & TM1638_DISPLAY_BRIGHT);

    TM1638_write_config();
}


/*
 * timer events for periodic key scanning and display updates
 */
tbtick_t keys_update_interval;
tbtick_t segments_update_interval;

static int8_t keys_update_handler(struct timer_event * this_timer_event)
{
    /* set pending command bit */
    pending_command |= TM1638_READ_KEYS;

    __TM1638_read_keys();

    /* advance this timer one second */
    this_timer_event->tbtick += keys_update_interval;

    /* reschedule this timer */
    return 1;
}

static int8_t segments_update_handler(struct timer_event * this_timer_event)
{
    /* set pending command bit */
    pending_command |= TM1638_WRITE_SEGMENTS;

    __TM1638_write_segments();

    /* advance this timer one second */
    this_timer_event->tbtick += segments_update_interval;

    /* reschedule this timer */
    return 1;
}

static struct timer_event keys_update_event = {
    .next = &keys_update_event,
    .handler = keys_update_handler,
};

static struct timer_event segments_update_event = {
    .next = &segments_update_event,
    .handler = segments_update_handler,
};


void TM1638_init(uint8_t keys_update_ms, uint8_t segments_update_ms)
{
    GPIOR0 &= ~TM1638_EV_BUSY;
    pending_command = TM1638_IDLE;
    active_command = TM1638_IDLE;

    TM1638_write_config();

    for (uint8_t i = 0; i < ARRAY_SIZE(segments_buffer); i++)
    {
        segments_buffer[i] = 0x00;
    }

    TM1638_write_segments();

    keys_update_interval = TBTICKS_FROM_MS(keys_update_ms);
    segments_update_interval = TBTICKS_FROM_MS(segments_update_ms);

    /*
     * schedule key scan
     */
    if (0 != keys_update_ms)
    {
        keys_update_event.tbtick = keys_update_interval;
        schedule_timer_event(&keys_update_event, NULL);
    }

    /*
     * schedule display update, offset by 1/2 key scan
     */
    if (0 != segments_update_ms)
    {
        segments_update_event.tbtick = keys_update_interval / 2;
        schedule_timer_event(&segments_update_event, &keys_update_event);
    }
}


/*
 * Display segments
 *
 *      --a--
 *     |     |
 *     f     b
 *     |     |
 *      --g--
 *     |     |
 *     e     c
 *     |     |
 *      --d-- * dp
 *
 *   Display Bits
 *
 *    a:  _BV(0)
 *    b:  _BV(1)
 *    c:  _BV(2)
 *    d:  _BV(3)
 *    e:  _BV(4)
 *    f:  _BV(5)
 *    g:  _BV(6)
 *    dp: _BV(7)
 *
 * Example segment configurations:
 * - for character 'H', segments=bcefg, bits=0b01110110
 * - for character '-', segments=g,     bits=0b01000000
 * - etc.
 */
static const uint8_t PROGMEM _digit_segments[] =
{
    0x3F, // 0
    0x06, // 1
    0x5B, // 2
    0x4F, // 3
    0x66, // 4
    0x6D, // 5
    0x7D, // 6
    0x07, // 7
    0x7F, // 8
    0x6F, // 9
    0x77, // A
    0x7C, // b
    0x39, // C
    0x5E, // d
    0x79, // E
    0x71, // F
};

void TM1638_write_digit(uint8_t const digit, int8_t const value)
{
    if (digit <= TM1638_MAX_DIGIT)
    {
        uint16_t const digit_mask = 0x0001 << digit;
        uint8_t segments;

        if ((value < 0) || (value > TM1638_MAX_VALUE))
        {
            segments = 0x00;
        }
        else
        {
            segments = pgm_read_word(&_digit_segments[value]);
        }

        for (uint8_t i = 0; i < ARRAY_SIZE(segments_buffer); i += 2)
        {
            uint16_t * segment_word = (uint16_t *) &segments_buffer[i];

            if (segments & 0x01)
            {
                *segment_word |= digit_mask;
            }
            else
            {
                *segment_word &= ~digit_mask;
            }

            segments >>= 1;
        }
    }
}

