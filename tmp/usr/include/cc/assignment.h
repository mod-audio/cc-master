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

#ifndef CC_ASSIGNMENT_H
#define CC_ASSIGNMENT_H


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

#define CC_MODE_TOGGLE      0x01
#define CC_MODE_TRIGGER     0x02
#define CC_MODE_OPTIONS     0x04
#define CC_MODE_REAL        0x10
#define CC_MODE_INTEGER     0x20
#define CC_MODE_TAP_TEMPO   0x40
#define CC_MODE_MOMENTARY   0x80

//list defines
#define LIST_MODE_LED_CYCLING   0x01
#define LIST_MODE_SCROLL_DIR    0x02
#define LIST_MODE_GROUPED       0x04

/*
****************************************************************************************************
*       CONFIGURATION
****************************************************************************************************
*/

#define CC_MAX_ASSIGNMENTS  256


/*
****************************************************************************************************
*       DATA TYPES
****************************************************************************************************
*/

typedef struct cc_item_t {
    const char *label;
    float value;
} cc_item_t;

typedef struct cc_assignment_t {
    int id, device_id, actuator_id;
    const char *label;
    float value, min, max, def;
    uint32_t mode;
    uint16_t steps;
    const char *unit;
    int list_count;
    uint8_t list_bitmask;
    cc_item_t **list_items;
} cc_assignment_t;

typedef struct cc_assignment_key_t {
    int id, device_id;
} cc_assignment_key_t;


/*
****************************************************************************************************
*       FUNCTION PROTOTYPES
****************************************************************************************************
*/

int cc_assignment_add(cc_assignment_t *assignment);
int cc_assignment_remove(cc_assignment_key_t *assignment);
int cc_assignment_check(cc_assignment_key_t *assignment);


/*
****************************************************************************************************
*       CONFIGURATION ERRORS
****************************************************************************************************
*/


#endif
