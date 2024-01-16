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
#ifndef _PROJECT_H_
#define _PROJECT_H_

#include "pinmap.h"

#define UDIV_FLOOR(a,b) ((a) / (b))
#define UDIV_ROUND(a,b) (((a) + ((b) / 2)) / (b))
#define UDIV_CEILING(a,b) (((a) + (b) - 1) / (b))

#define min(a,b) ({              \
        __typeof__ (a) _a = (a); \
        __typeof__ (b) _b = (b); \
       _a < _b ? _a : _b;        \
})

#define max(a,b) ({              \
        __typeof__ (a) _a = (a); \
        __typeof__ (b) _b = (b); \
       _a > _b ? _a : _b;        \
})

/* lo <= n <= hi */
#define limit_range(lo,n,hi) min(max(lo,(n)),hi)

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))


/*
 * console interface
 */
#ifndef BAUD
#define BAUD (9600UL)
#endif

#define TX_BUF_SIZE (128)


/*
 * Project pin assignments.
 */

/* Tick pin */
#define SPEAKER_OUT PINMAP_D4

/* TM1638 pins */
#define TM1638_STB PINMAP_D2

#define TM1638_STB_HIGH()   pinmap_set(TM1638_STB)
#define TM1638_STB_LOW()    pinmap_clear(TM1638_STB)

/* Servo PWM output */
#define SERVO_OUT PINMAP_OC1A


/*
 * timebase timer, 0, 1 or 2
 */
#define TBTIMER 0

/*timebase timer prescaler */
#define TBTIMER_PRESCALER (256UL)

/* use the A compare */
#define TBTIMER_COMP A


#define TIMER1_PRESCALER (8UL)


/* GPIOR0 event bits */
#define TM1638_EV_BUSY           _BV(GPIOR00)


/*
 * initialize console before main
 */
extern void console_init(void) __attribute__((__constructor__));

/*
 * initialize timers before main
 */
extern void timers_init(void) __attribute__((__constructor__));

/*
 * these functions never return
 */
extern void main(void) __attribute__((__noreturn__));

#endif /* _PROJECT_H_ */
