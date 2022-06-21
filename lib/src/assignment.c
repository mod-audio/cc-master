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

    int return_val = -1;

    if (!device)
        return return_val;

    // if is the first time, create list of assignments
    if (!device->assignments)
        device->assignments = calloc(CC_MAX_ASSIGNMENTS, sizeof(cc_assignment_t *));
    // check the amount of assignments supported by the actuator
    cc_actuator_t *actuator = device->actuators[assignment->actuator_id];
    if (actuator->assignments_count >= actuator->max_assignments)
        return return_val;

    // store assignment
    for (int i = 0; i < CC_MAX_ASSIGNMENTS; i++)
    {
        if (!device->assignments[i])
        {
            // set assignment id
            assignment->id = i;

            // duplicate assignment
            cc_assignment_t *copy = malloc(sizeof(cc_assignment_t));
            memcpy(copy, assignment, sizeof(cc_assignment_t));

            if (assignment->label)
                copy->label = strdup(assignment->label);
            if (assignment->unit)
                copy->unit = strdup(assignment->unit);

            if (assignment->list_count != 0)
            {
                copy->list_items = malloc(assignment->list_count * sizeof(cc_item_t *));

                for (int j = 0; j < assignment->list_count; j++)
                {
                    cc_item_t *item = malloc(sizeof(cc_item_t));
                    item->label = strdup(assignment->list_items[j]->label);
                    item->value = assignment->list_items[j]->value;
                    copy->list_items[j] = item;
                }
            }

            device->assignments[i] = copy;

            // increment actuator assignments counter
            cc_actuator_t *actuator = device->actuators[assignment->actuator_id];
            actuator->assignments_count++;

            return_val = i;
            break;
        }
    }

    return return_val;
}

int cc_assignment_remove(cc_assignment_key_t *assignment)
{
    if (!cc_assignment_check(assignment))
        return 0;

    cc_device_t *device = cc_device_get(assignment->device_id);

    int id = assignment->id;
    if (device->assignments)
    {
        cc_assignment_t *assignment = device->assignments[id];
        int actuator_id = assignment->actuator_id;

        // free assignment memory and its list position
        for (int i = 0; i < assignment->list_count; i++)
            free((void*)assignment->list_items[i]->label);
        free(assignment->list_items);
        free((void*)assignment->label);
        free((void*)assignment->unit);
        free(assignment);
        device->assignments[id] = NULL;

        // decrement actuator assignments counter
        cc_actuator_t *actuator = device->actuators[actuator_id];
        actuator->assignments_count--;
    }

    return id;
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

cc_assignment_t *cc_assignment_get(cc_assignment_key_t *assignment)
{
    cc_device_t *device = cc_device_get(assignment->device_id);

    if (!device || !device->assignments)
        return NULL;

    for (int i = 0; i < CC_MAX_ASSIGNMENTS; i++)
    {
        if(!device->assignments[i])
            continue;

        if(device->assignments[i]->id == assignment->id)
        {
            for (int j = 0; j < device->assignments[i]->list_count; j++)
            {
                cc_item_t *item = device->assignments[i]->list_items[j];

                if (!item)
                    continue;
            }

           return device->assignments[i];
        }
    }

    return NULL;
}

cc_assignment_t *cc_assignment_get_by_actuator(int device_id, int actuator_id)
{
    cc_device_t *device = cc_device_get(device_id);

    if (!device || !device->assignments)
        return NULL;

    for (int i = 0; i < CC_MAX_ASSIGNMENTS; i++)
    {
        if(!device->assignments[i])
            continue;

        if(device->assignments[i]->actuator_id == actuator_id)
        {
            for (int j = 0; j < device->assignments[i]->list_count; j++)
            {
                cc_item_t *item = device->assignments[i]->list_items[j];

                if (!item)
                    continue;
            }

           return device->assignments[i];
        }
    }

    return NULL;
}

void cc_assignment_update_list(cc_assignment_t *assignment, float index)
{
    cc_device_t *device = cc_device_get(assignment->device_id);
    int enumeration_frame_half = (int) device->enumeration_frame_item_count/2;

    assignment->list_index = index;

    assignment->enumeration_frame_min = assignment->list_index - enumeration_frame_half;
    assignment->enumeration_frame_max = assignment->list_index + enumeration_frame_half;

    if (assignment->enumeration_frame_min < 0) {
        assignment->enumeration_frame_min = 0;
        if (assignment->list_count < device->enumeration_frame_item_count - 1)
            assignment->enumeration_frame_max = assignment->list_count;
        else
            assignment->enumeration_frame_max = device->enumeration_frame_item_count - 1;
    }

    if (assignment->enumeration_frame_max >= assignment->list_count) {
        assignment->enumeration_frame_max = assignment->list_count-1;
        assignment->enumeration_frame_min = assignment->enumeration_frame_max - (device->enumeration_frame_item_count - 1);

        if (assignment->enumeration_frame_min < 0)
            assignment->enumeration_frame_min = 0;
    }

    assignment->list_index -= assignment->enumeration_frame_min;
}

int cc_assignment_set_pair_id(cc_assignment_key_t *assignment)
{
    cc_device_t *device = cc_device_get(assignment->device_id);

    if (!device || !device->assignments)
        return 0;

    for (int i = 0; i < CC_MAX_ASSIGNMENTS; i++)
    {
        if(!device->assignments[i])
            continue;

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
