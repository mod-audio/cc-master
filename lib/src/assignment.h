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
#define CC_MODE_TOGGLE      0x001
#define CC_MODE_TRIGGER     0x002
#define CC_MODE_OPTIONS     0x004
#define CC_MODE_TAP_TEMPO   0x008
#define CC_MODE_REAL        0x010
#define CC_MODE_INTEGER     0x020
#define CC_MODE_LOGARITHMIC 0x040
#define CC_MODE_COLOURED    0x100
#define CC_MODE_MOMENTARY   0x200
#define CC_MODE_REVERSE     0x400
#define CC_MODE_GROUP       0x800


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
    cc_item_t **list_items;
    int actuator_pair_id;
    int assignment_pair_id;
    /* variables below this point are only used for the server and should be ignored by clients
     * they should be treated as private API */
    int list_index, enumeration_frame_min, enumeration_frame_max;
    int actuator_page_id;
} cc_assignment_t;

typedef struct cc_assignment_key_t {
    int id, device_id, pair_id;
} cc_assignment_key_t;


/*
****************************************************************************************************
*       FUNCTION PROTOTYPES
****************************************************************************************************
*/

int cc_assignment_add(const cc_assignment_t *assignment);
int cc_assignment_remove(const cc_assignment_key_t *assignment);
int cc_assignment_check(const cc_assignment_key_t *assignment);
int cc_assignment_set_pair_id(cc_assignment_key_t *assignment);

cc_assignment_t *cc_assignment_get(const cc_assignment_key_t *assignment);
cc_assignment_t *cc_assignment_get_by_actuator(int device_id, int actuator_id);
void cc_assignment_update_list(cc_assignment_t *assignment, float index);

cc_assignment_t *cc_assignment_dup(const cc_assignment_t *assignment);
void cc_assignment_free(cc_assignment_t *assignment);

/*
****************************************************************************************************
*       CONFIGURATION ERRORS
****************************************************************************************************
*/


#endif
