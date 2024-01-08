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


#define PWM_FREQ    100UL

#define PWM_PERIOD  20000UL
#define MIN_FAN_RPM 840U
#define MAX_FAN_RPM 2200U
#define MIN_PULSE   1900UL  // 10%
#define MAX_PULSE   14900UL // 75%


//  q = (x >> 3)
//    - (x >> 16)
//    + (x >> 19)
//    - (x >> 22)
//    + (x >> 25)
//    - (x >> 28)
//
//  r = x - (q * 10)
//  return q, r


/* 840 slow, 2200 fast */
static uint16_t rpm = 0U;

static uint32_t buttons = 0U;

static uint32_t process_buttons(void)
{
    uint32_t const last_buttons = buttons;

    buttons = TM1638_get_keys();

    return (last_buttons ^ buttons) & buttons;
}


void setRPM(uint16_t rpm)
{
    uint16_t pulse;

    if (0U == rpm)
    {
        pulse = 0U;
    }
    else
    {
        /* limit RPM to supported range */
        rpm = limit_range(MIN_FAN_RPM, rpm, MAX_FAN_RPM);

        /* interpolate pulse width */
        pulse = (uint16_t) (MIN_PULSE +
                            (((uint32_t) (rpm - MIN_FAN_RPM) * (uint32_t) (MAX_PULSE - MIN_PULSE)) /
                             (uint32_t) (MAX_FAN_RPM - MIN_FAN_RPM))
                           );
    }

    OCR1A = 19999 - pulse;
}


static void update_rpm(void)
{
    uint16_t pw = rpm;

    /* break up pulse width into decimal digits */
    uint8_t ones   = pw % 10;
    pw /= 10;
    uint8_t tens    = pw % 10;
    pw /= 10;
    uint8_t hundreds = pw % 10;
    pw /= 10;
    uint8_t thousands = pw % 10;
    pw /= 10;

    /* process button pushes */
    uint32_t changed_buttons = process_buttons();

    if (0x0002 & changed_buttons)
    {
        thousands = (thousands + 1) % 10;
    }

    if (0x0020 & changed_buttons)
    {
        hundreds = (hundreds + 1) % 10;
    }

    if (0x0200 & changed_buttons)
    {
        tens = (tens + 1) % 10;
    }

    if (0x2000 & changed_buttons)
    {
        ones = (ones + 1) % 10;
    }

    /* assemble pulse width from decimal digits */
    rpm = (thousands  * 1000)
        + (hundreds * 100)
        + (tens   * 10)
        + (ones * 1);

    if (rpm < 1000)
    {
        TM1638_write_digit(3, -1);
    }
    else
    {
        TM1638_write_digit(3, thousands);
    }

    if (rpm < 100)
    {
        TM1638_write_digit(2, -1);
    }
    else
    {
        TM1638_write_digit(2, hundreds);
    }

    if (rpm < 10)
    {
        TM1638_write_digit(1, -1);
    }
    else
    {
        TM1638_write_digit(1, tens);
    }

    TM1638_write_digit(0, ones);

    setRPM(rpm);
}

void fan_init(void)
{
    /* initialize fan output pin */
    pinmap_clear(FAN_OUT);
    pinmap_dir(0, FAN_OUT);

    // Timer 1, Fast PWM mode 14, WGM = 1:1:1:0, clk/8,
    TCCR1A = 0xC2;  // COM1A1 = 1, COM1A0 = 0, WGM11 = 1, WGM10 = 0
    TCCR1B = 0x1A;  // WGM13 = 1, WGM12 = 1, CS = 2
    TCCR1C = 0x00;
    ICR1   = 19999;
    OCR1A  = 9999;
}

/*
 * interrupt disabled
 * enabled
 * LSb first
 * Master
 * Clk/64
 * CPOL=1
 * CPHA=1
 */

void spi_init(void)
{
    pinmap_set(PINMAP_MISO | PINMAP_SCK | PINMAP_MOSI | PINMAP_SS);
    pinmap_dir(PINMAP_MISO, PINMAP_SCK | PINMAP_MOSI | PINMAP_SS);

    SPCR = _BV(SPE)  | _BV(DORD) | _BV(MSTR)
         | _BV(CPOL) | _BV(CPHA) | _BV(SPR1);
    SPSR = _BV(SPI2X);
}


void main(void)
{
    /*
     * initialize
     */
    ATOMIC_BLOCK(ATOMIC_FORCEON)
    {
        GPIOR0 = 0;
        tbtick_init();
        tick_init();
        fan_init();
        spi_init();
    }
    /* interrupts are enabled */

    TM1638_init(10, 20);

    for (;;)
    {
        /* read buttons and update rpm */
        update_rpm();
    }

    /*
     * run command line interface
     */
    cmdline();
}

