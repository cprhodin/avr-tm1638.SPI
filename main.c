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


#define PWM_FREQ    100U
#define MIN_PULSE   500U  // in us
#define MAX_PULSE   2500U  // in us

#define PWM_COUNTS  20000U
#define MIN_COUNTS  1000U
#define MAX_COUNTS  5000U

/* 1000 to 2000 us */
static uint16_t pulse_us = (MAX_PULSE + MIN_PULSE) / 2;


static uint32_t buttons = 0UL;

static uint32_t process_buttons(void)
{
    uint32_t const last_buttons = buttons;

    buttons = TM1638_get_keys();

    return (last_buttons ^ buttons) & buttons;
}


void set_servo(uint16_t pulse_us)
{
    uint16_t pulse_counts;

    /* interpolate pulse width */
    pulse_counts = (uint16_t) (MIN_COUNTS + (((uint32_t) (pulse_us - MIN_PULSE)
                                            * (uint32_t) (MAX_COUNTS - MIN_COUNTS))
                                             / (uint32_t) (MAX_PULSE - MIN_PULSE)));

    OCR1A = 19999 - pulse_counts;
}


static void update_servo(void)
{
    /* process button pushes */
    uint32_t changed_buttons = process_buttons();

    if (0x00020000 & changed_buttons)
    {
        pulse_us -= 1000;
    }

    if (0x00000002 & changed_buttons)
    {
        pulse_us += 1000;
    }

    if (0x00200000 & changed_buttons)
    {
        pulse_us -= 100;
    }

    if (0x00000020 & changed_buttons)
    {
        pulse_us += 100;
    }

    if (0x02000000 & changed_buttons)
    {
        pulse_us -= 10;
    }

    if (0x00000200 & changed_buttons)
    {
        pulse_us += 10;
    }

    if (0x20000000 & changed_buttons)
    {
        pulse_us -= 1;
    }

    if (0x00002000 & changed_buttons)
    {
        pulse_us += 1;
    }

    pulse_us = limit_range(MIN_PULSE, pulse_us, MAX_PULSE);

    if (pulse_us < 1000)
    {
        TM1638_write_digit(3, -1);
    }
    else
    {
        TM1638_write_digit(3, (pulse_us / 1000) % 10);
    }

    if (pulse_us < 100)
    {
        TM1638_write_digit(2, -1);
    }
    else
    {
        TM1638_write_digit(2, (pulse_us / 100) % 10);
    }

    if (pulse_us < 10)
    {
        TM1638_write_digit(1, -1);
    }
    else
    {
        TM1638_write_digit(1, (pulse_us / 10) % 10);
    }

    TM1638_write_digit(0, pulse_us % 10);

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

    TM1638_init(10, 20);

    for (;;)
    {
        /* read buttons and update servo */
        update_servo();
    }
}

