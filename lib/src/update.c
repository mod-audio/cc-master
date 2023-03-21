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

/*
****************************************************************************************************
*       INCLUDE FILES
****************************************************************************************************
*/

#include <stdlib.h>
#include <string.h>
#include "update.h"
#include "assignment.h"


/*
****************************************************************************************************
*       INTERNAL MACROS
****************************************************************************************************
*/


/*
****************************************************************************************************
*       INTERNAL CONSTANTS
****************************************************************************************************
*/


/*
****************************************************************************************************
*       INTERNAL DATA TYPES
****************************************************************************************************
*/


/*
****************************************************************************************************
*       INTERNAL GLOBAL VARIABLES
****************************************************************************************************
*/


/*
****************************************************************************************************
*       INTERNAL FUNCTIONS
****************************************************************************************************
*/


/*
****************************************************************************************************
*       GLOBAL FUNCTIONS
****************************************************************************************************
*/

#define UPDATE_DATA_SIZE (sizeof(float) + 1)

cc_update_list_t *cc_update_parse(int device_id, uint8_t *raw_data, bool check_assignments)
{
    const uint8_t count = *raw_data++;

    // create and initialize update list
    cc_update_list_t *updates = malloc(sizeof(cc_update_list_t));
    updates->device_id = device_id;
    updates->count = 0;
    updates->list = malloc(sizeof(cc_update_data_t) * count);
    updates->raw_data = malloc(UPDATE_DATA_SIZE * count + 1);

    // parse data to struct
    for (uint8_t i = 0, k = 1; i < count; i++)
    {
        cc_assignment_key_t assignment;
        assignment.id = raw_data[0];
        assignment.device_id = device_id;
        assignment.pair_id = -1;

        if (check_assignments || cc_assignment_check(&assignment))
        {
            cc_update_data_t *data = &updates->list[i];

            // update id
            data->assignment_id = assignment.id;

            // update value
            float *value = (float *) (raw_data + 1);
            data->value = *value;

            // copy raw data
            memcpy(updates->raw_data + k, raw_data, UPDATE_DATA_SIZE);
            k += UPDATE_DATA_SIZE;

            updates->count++;
        }

        // increment by update data size
        raw_data += UPDATE_DATA_SIZE;
    }

    // update count and size of raw_data
    updates->raw_data[0] = updates->count;
    updates->raw_size = updates->count * UPDATE_DATA_SIZE + 1;

    return updates;
}

void cc_update_free(cc_update_list_t *updates)
{
    free(updates->list);
    free(updates->raw_data);
    free(updates);
}
