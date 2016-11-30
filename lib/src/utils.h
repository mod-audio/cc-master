/*
 * This file is part of the control chain project
 *
 * Copyright (C) 2016 Ricardo Crudo <ricardo.crudo@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CC_UTILS_H
#define CC_UTILS_H

/*
****************************************************************************************************
*       INCLUDE FILES
****************************************************************************************************
*/

#include <stdint.h>


/*
****************************************************************************************************
*       MACROS
****************************************************************************************************
*/


/*
****************************************************************************************************
*       CONFIGURATION
****************************************************************************************************
*/


/*
****************************************************************************************************
*       DATA TYPES
****************************************************************************************************
*/

typedef struct string_t {
    uint8_t size;
    char *text;
} string_t;

typedef struct version_t {
    uint8_t major, minor, micro;
} version_t;


/*
****************************************************************************************************
*       FUNCTION PROTOTYPES
****************************************************************************************************
*/

/*
 http://stackoverflow.com/a/15171925/1283578
 8-bit CRC with polynomial x^8+x^6+x^3+x^2+1, 0x14D.
 Chosen based on Koopman, et al. (0xA6 in his notation = 0x14D >> 1):
 http://www.ece.cmu.edu/~koopman/roses/dsn04/koopman04_crc_poly_embedded.pdf
*/
uint8_t crc8(const uint8_t *data, uint32_t len);

string_t *string_create(const char *str);
uint8_t string_serialize(const string_t *str, uint8_t *buffer);
string_t *string_deserialize(const uint8_t *data, uint32_t *written);
void string_destroy(string_t *str);

int float_to_bytes(const float value, uint8_t *array);


/*
****************************************************************************************************
*       CONFIGURATION ERRORS
****************************************************************************************************
*/


#endif
