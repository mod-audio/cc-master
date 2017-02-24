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
            cc_assignment_t *copy = malloc(sizeof(cc_assignment_t));
            device->assignments[i] = copy;
            memcpy(copy, assignment, sizeof(cc_assignment_t));

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
    if (!cc_assignment_check(assignment))
        return -1;

    cc_device_t *device = cc_device_get(assignment->device_id);

    int id = assignment->id;
    if (device->assignments)
    {
        cc_assignment_t *assignment = device->assignments[id];
        int actuator_id = assignment->actuator_id;

        // free assignment memory and its list position
        free(assignment);
        device->assignments[id] = 0;

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
