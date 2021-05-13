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

/*
#include <stdlib.h>
#include <string.h>
#include "update.h"
#include "device.h"
#include "assignment.h"
*/

/*
****************************************************************************************************
*       INTERNAL MACROS
****************************************************************************************************
*/
//TODO DELETE



#include "core.h"
#include "utils.h"
#include "msg.h"
#include "handshake.h"
#include "device.h"
#include "assignment.h"
#include "update.h"


// debug macro
#define DEBUG_MSG(...)      do { fprintf(stderr, "[cc-lib] " __VA_ARGS__); } while (0)


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

cc_update_list_t *cc_update_parse(int device_id, uint8_t *raw_data, int check_assignments)
{
    const int update_data_size = (sizeof(float) + 1);
    int count = raw_data[0];

    // create and initialize update list
    cc_update_list_t *updates = malloc(sizeof(cc_update_list_t));
    updates->count = 0;
    updates->device_id = device_id;
    updates->list = malloc(sizeof(cc_update_data_t) * count);
    updates->raw_data = malloc(update_data_size * count + 1);

    // parse data to struct
    int j = 1, k = 1;
    for (int i = 0; i < count; i++)
    {
        cc_assignment_key_t assignment;
        assignment.device_id = device_id;
        assignment.id = raw_data[j];

        DEBUG_MSG("checking update thingy\n");

        if (check_assignments == 0 || cc_assignment_check(&assignment))
        {
            cc_update_data_t *data = &updates->list[i];

            // update id
            DEBUG_MSG("get device\n");
            cc_device_t *device = cc_device_get(device_id);
            //group assignemnt, check if we need to change ID

            DEBUG_MSG("start checking actuator groups\n");
            if (device->actuators[cc_assignement_actuator_get(device_id, assignment.id)]->grouped)
            {
                DEBUG_MSG("we are grouped, find match\n");
                for (int q = 0; q < device->actuatorgroups_count; q++)
                {
                    if (device->actuatorgroups[q]->actuators_in_actuatorgroup[1] == device->assignments[assignment.id]->actuator_id)
                    {
                        DEBUG_MSG("found match, in second act in group\n");
                        data->assignment_id = cc_assignement_id_get(device_id, device->actuatorgroups[q]->actuators_in_actuatorgroup[0]);
                        raw_data[j] = data->assignment_id;
                        DEBUG_MSG("changed id to:  %i\n", data->assignment_id);
                        break;
                    }
                    else if (device->actuatorgroups[q]->actuators_in_actuatorgroup[0] == device->assignments[assignment.id]->actuator_id)
                    {
                        DEBUG_MSG("found match in first act, keep id\n");
                        data->assignment_id = assignment.id;
                        break;
                    }
                }
            }
            else
                data->assignment_id = assignment.id;

            // update value
            float *value = (float *) &raw_data[j + 1];
            data->value = *value;

            // copy raw data
            memcpy(&updates->raw_data[k], &raw_data[j], update_data_size);
            k += update_data_size;

            updates->count++;
        }

        // increment by update data size
        j += update_data_size;
    }

    // update count and size of raw_data
    updates->raw_data[0] = updates->count;
    updates->raw_size = k;

    return updates;
}

void cc_update_free(cc_update_list_t *updates)
{
    free(updates->list);
    free(updates->raw_data);
    free(updates);
}
