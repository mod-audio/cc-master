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

cc_update_list_t *cc_update_parse(int device_id, uint8_t *raw_data)
{
    cc_update_list_t *updates = malloc(sizeof(cc_update_list_t));
    updates->device_id = device_id;
    updates->count = raw_data[0];
    updates->list = malloc(sizeof(cc_update_data_t) * updates->count);

    // parse data to struct
    int j = 1;
    for (int i = 0; i < updates->count; i++)
    {
        cc_update_data_t *data = &updates->list[i];
        data->assignment_id = raw_data[j++];
        float *value = (float *) &raw_data[j];
        data->value = *value;
        j += sizeof (float);
    }

    // store raw data
    updates->raw_data = malloc(j);
    updates->raw_size = j;
    memcpy(updates->raw_data, raw_data, j);

    return updates;
}

void cc_update_free(cc_update_list_t *updates)
{
    free(updates->list);
    free(updates->raw_data);
    free(updates);
}
