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
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <util/atomic.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

#include "timer.h"
#include "tick.h"
#include "tm1638.h"
#include "bibase.h"


#define PWM_FREQ    100U
#define MIN_PULSE   500U  // in us
#define MAX_PULSE   2500U // in us

#define PWM_COUNTS  20000U
#define MIN_COUNTS  (2 * MIN_PULSE)
#define MAX_COUNTS  (2 * MAX_PULSE)

/* 1000 to 2000 us */
static uint16_t pulse_us = (MAX_PULSE + MIN_PULSE) / 2;

static uint8_t brightness = TM1638_MAX_BRIGHTNESS / 2;

static uint32_t keys = 0UL;

static uint32_t process_keys(void)
{
    uint32_t const new_keys     = TM1638_get_keys();
    uint32_t const keys_changed = new_keys ^ keys;

    keys = new_keys;

    uint32_t keys_down = keys_changed & keys;
    uint32_t keys_up   = keys_changed & ~keys;

    return keys_down;
}


void set_servo(uint16_t pulse_us)
{
    uint16_t pulse_counts;

    if (0U == pulse_us)
    {
        pulse_counts = 0U;
    }
    else
    {
	    /* interpolate pulse width */
	    pulse_counts = (uint16_t) (MIN_COUNTS + (((uint32_t) (pulse_us - MIN_PULSE)
	                               * (uint32_t) (MAX_COUNTS - MIN_COUNTS))
	                               / (uint32_t) (MAX_PULSE - MIN_PULSE)));
    }

    OCR1A = (PWM_COUNTS - 1) - pulse_counts;
}


static void update_servo(void)
{
    /* process button pushes */
    uint32_t changed_buttons = process_keys();

#if 0
    if (0 != changed_buttons)
    {
        printf("Button Down: %08lX\n", changed_buttons);
    }
#endif

    if (0x00000004 & changed_buttons)
    {
        /* ON */
        TM1638_enable(1);
    }

    if (0x00040000 & changed_buttons)
    {
        /* OFF */
        TM1638_enable(0);
    }

    if (0x40000000 & changed_buttons)
    {
        /* dimmer */
        if (brightness > 0)
        {
            brightness--;
        }

        TM1638_brightness(brightness);
    }

    if (0x00004000 & changed_buttons)
    {
        /* brighter */
        if (brightness < TM1638_MAX_BRIGHTNESS)
        {
            brightness++;
        }

        TM1638_brightness(brightness);
    }

    uint16_t new_pulse_us = pulse_us;

    if      (0x22220000 & changed_buttons)
    {
        /*
         * down button pressed
         */

        if (0x00020000 & changed_buttons)
        {
            new_pulse_us = pulse_us - 1000;
        }

        if (0x00200000 & changed_buttons)
        {
            new_pulse_us = pulse_us - 100;
        }

        if (0x02000000 & changed_buttons)
        {
            new_pulse_us = pulse_us - 10;
        }

        if (0x20000000 & changed_buttons)
        {
            new_pulse_us = pulse_us - 1;
        }

        /*
         * if adjusted RPM is less than minimum or underflowed
         */
        if ((new_pulse_us > pulse_us) || (new_pulse_us < MIN_PULSE))
        {
            new_pulse_us = 0;
        }
    }
    else if (0x00002222 & changed_buttons)
    {
        /*
         * up button pressed
         */

        if (pulse_us == 0)
        {
            new_pulse_us = MIN_PULSE;
        }
        else
        {
            if (0x00000002 & changed_buttons)
            {
                new_pulse_us = pulse_us + 1000;
            }

            if (0x00000020 & changed_buttons)
            {
                new_pulse_us = pulse_us + 100;
            }

            if (0x00000200 & changed_buttons)
            {
                new_pulse_us = pulse_us + 10;
            }

            if (0x00002000 & changed_buttons)
            {
                new_pulse_us = pulse_us + 1;
            }

            /*
             * if adjusted RPM is greater than maximum or overflowed
             */
            if ((new_pulse_us < pulse_us) || (new_pulse_us > MAX_PULSE))
            {
                new_pulse_us = MAX_PULSE;
            }
        }
    }

    pulse_us = new_pulse_us;

    uint8_t dec[4] = { 0, 0, 0, 0 };

    uint8_t n_digit = bibase(0, pulse_us >> 8, dec, 246);
    n_digit = bibase(n_digit, pulse_us, dec, 246);

    TM1638_write_digit(3, (n_digit > 3) ? dec[3] : -1);
    TM1638_write_digit(2, (n_digit > 2) ? dec[2] : -1);
    TM1638_write_digit(1, (n_digit > 1) ? dec[1] : -1);
    TM1638_write_digit(0, dec[0]);

    set_servo(pulse_us);
}

void servo_init(void)
{
    /* initialize servo output pin */
    pinmap_clear(SERVO_OUT);
    pinmap_dir(0, SERVO_OUT);

    // Timer 1, Fast PWM mode 14, WGM = 1:1:1:0, clk/8,
    TCCR1A = 0xC2;  // COM1A1 = 1, COM1A0 = 0, WGM11 = 1, WGM10 = 0
    TCCR1B = 0x1A;  // WGM13 = 1, WGM12 = 1, CS = 2
    TCCR1C = 0x00;
    ICR1   = PWM_COUNTS - 1;
    OCR1A  = (MAX_COUNTS - MIN_COUNTS) / 2;
}


void main(void)
{
    /*
     * initialize
     */
    ATOMIC_BLOCK(ATOMIC_FORCEON)
    {
        tbtick_init();
        tick_init();
        servo_init();
    }
    /* interrupts are enabled */

    /* initialize and enable the TM1638 */
    TM1638_init(10);
    TM1638_enable(1);

    for (;;)
    {
        /* read keys and update servo */
        update_servo();
    }
}

