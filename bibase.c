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
#include <stdint.h>

#include "bibase.h"


//                      R24:xx      R22:xx              R20:R21         R18:xx
uint8_t bibase(uint8_t n_digit, uint8_t bi, uint8_t * const str, uint8_t const nbase) __attribute__((__naked__));
uint8_t bibase(uint8_t n_digit, uint8_t bi, uint8_t * const str, uint8_t const nbase)
{
    __asm__ __volatile__ (
        "               ldi             r19, 8                               \n"
        "               ;                                                    \n"
        "               ; bit loop                                           \n"
        "               ;                                                    \n"
        "                                                                    \n"
        "1:             mov             r25, r24                             \n"
        "               movw            r30, r20                             \n"
        "               lsl             r22                                  \n"
        "                                                                    \n"
        "               tst             r25                                  \n"
        "               breq            4f                                   \n"
        "               ;                                                    \n"
        "               ; digit loop                                         \n"
        "               ;                                                    \n"
        "                                                                    \n"
        "2:             ld              r23, Z                               \n"
        "               rol             r23                                  \n"
        "               add             r23, r18                             \n"
        "               brcs            3f                                   \n"
        "               sub             r23, r18                             \n"
        "3:             st              Z+, r23                              \n"
        "                                                                    \n"
        "               ;                                                    \n"
        "               ; digit loop                                         \n"
        "               ;                                                    \n"
        "               dec             r25                                  \n"
        "               brne            2b                                   \n"
        "                                                                    \n"
        "4:             brcc            5f                                   \n"
        "               ldi             r23, 1                               \n"
        "               st              Z, r23                               \n"
        "               inc             r24                                  \n"
        "5:                                                                  \n"
        "                                                                    \n"
        "               ;                                                    \n"
        "               ; bit loop                                           \n"
        "               ;                                                    \n"
        "               dec             r19                                  \n"
        "               brne            1b                                   \n"
        "                                                                    \n"
        "               ret                                                  \n"
        :
        :
        : "r19", "r23", "r25", "r30", "r31", "memory"
    );

    return n_digit;
}


