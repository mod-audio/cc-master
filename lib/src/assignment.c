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

#include "assignment.h"
#include "device.h"
#include "utils.h"

#include <stdlib.h>
#include <string.h>


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

int cc_assignment_add(cc_assignment_t *assignment)
{
    cc_device_t *device = cc_device_get(assignment->device_id);

    if (!device)
        return -1;

    // if is the first time, create list of assignments
    if (!device->assignments)
        device->assignments = calloc(CC_MAX_ASSIGNMENTS, sizeof(cc_assignment_t *));

    // check the amount of assignments supported by the actuator
    cc_actuator_t *actuator = device->actuators[assignment->actuator_id];
    if (actuator->assignments_count >= actuator->max_assignments)
        return -1;

    // store assignment
    for (int i = 0; i < CC_MAX_ASSIGNMENTS; i++)
    {
        if (!device->assignments[i])
        {
            // set assignment id
            assignment->id = i;

            // duplicate assignment
            device->assignments[i] = cc_assignment_dup(assignment);

            // increment actuator assignments counter
            cc_actuator_t *actuator = device->actuators[assignment->actuator_id];
            actuator->assignments_count++;

            return i;
        }
    }

    return -1;
}

int cc_assignment_remove(cc_assignment_key_t *assignment)
{
    cc_device_t *device = cc_device_get(assignment->device_id);

    if (device && device->assignments)
    {
        const int id = assignment->id;

        for (int i = 0; i < CC_MAX_ASSIGNMENTS; i++)
        {
            if (device->assignments[i])
            {
                if (device->assignments[i]->id == id)
                {
                    const int actuator_id = device->assignments[i]->actuator_id;

                    // free assignment memory and its list position
                    cc_assignment_free(device->assignments[i]);
                    device->assignments[i] = NULL;

                    // decrement actuator assignments counter
                    cc_actuator_t *actuator = device->actuators[actuator_id];
                    actuator->assignments_count--;

                    return id;
                }
            }
        }
    }

    return -1;
}

int cc_assignment_check(cc_assignment_key_t *assignment)
{
    cc_device_t *device = cc_device_get(assignment->device_id);

    if (!device || !device->assignments)
        return 0;

    for (int i = 0; i < CC_MAX_ASSIGNMENTS; i++)
    {
        if (device->assignments[i])
        {
            if (device->assignments[i]->id == assignment->id)
                return 1;
        }
    }

    return 0;
}

int cc_assignment_set_pair_id(cc_assignment_key_t *assignment)
{
    cc_device_t *device = cc_device_get(assignment->device_id);

    if (!device || !device->assignments)
        return 0;

    for (int i = 0; i < CC_MAX_ASSIGNMENTS; i++)
    {
        if (device->assignments[i])
        {
            if (device->assignments[i]->id == assignment->id)
            {
                device->assignments[i]->assignment_pair_id = assignment->pair_id;
                return 1;
            }
        }
    }

    return 0;
}

cc_assignment_t *cc_assignment_dup(const cc_assignment_t *assignment)
{
    cc_assignment_t *copy = malloc(sizeof(cc_assignment_t));
    memcpy(copy, assignment, sizeof(cc_assignment_t));

    if (assignment->label)
        copy->label = strdup(assignment->label);

    if (assignment->unit)
        copy->unit = strdup(assignment->unit);

    if (assignment->list_count != 0)
    {
        copy->list_items = malloc(assignment->list_count * sizeof(cc_item_t *));

        for (int i = 0; i < assignment->list_count; i++)
        {
            cc_item_t *item = malloc(sizeof(cc_item_t));
            copy->list_items[i] = item;
            item->label = strdup(assignment->list_items[i]->label);
            item->value = assignment->list_items[i]->value;
        }
    }

    return copy;
}

void cc_assignment_free(cc_assignment_t *assignment)
{
    for (int i = 0; i < assignment->list_count; i++)
    {
        free((void*)assignment->list_items[i]->label);
        free(assignment->list_items[i]);
    }
    free(assignment->list_items);
    free((void*)assignment->label);
    free((void*)assignment->unit);
    free(assignment);
}
