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
#include <jansson.h>
#include "device.h"


/*
****************************************************************************************************
*       INTERNAL MACROS
****************************************************************************************************
*/

// device status
enum {DEV_WAITING_HANDSHAKE, DEV_WAITING_DESCRIPTOR, DEV_WAITING_ASSIGNMENT};


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

static cc_device_t g_devices[CC_MAX_DEVICES];
static int g_devices_initialized;


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

void cc_device_create(int device_id)
{
    if (!g_devices_initialized)
    {
        for (int i = 0; i < CC_MAX_DEVICES; i++)
            g_devices[i].id = -1;

        g_devices_initialized = 1;
    }

    static int count;
    g_devices[count++].id = device_id;
}

void cc_device_destroy(int device_id)
{
    cc_device_t *device = cc_device_get(device_id);

    if (!device)
        return;

    // reset id and status values
    device->id = -1;
    device->status = CC_DEVICE_DISCONNECTED;

    // destroy assigments list
    if (device->assignments)
        free(device->assignments);

    // return if device hasn't descriptor
    if (!device->descriptor)
        return;

    cc_dev_descriptor_t *descriptor = device->descriptor;
    string_destroy(descriptor->label);

    if (descriptor->actuators)
    {
        for (int i = 0; i < descriptor->actuators_count; i++)
            free(descriptor->actuators[i]);

        free(descriptor->actuators);
    }

    free(device->descriptor);
    device->descriptor = 0;
}

char* cc_device_descriptor(int device_id)
{
    cc_device_t* device = cc_device_get(device_id);

    if (!device)
        return 0;

    cc_dev_descriptor_t *descriptor = device->descriptor;
    json_t *root = json_object();

    // label
    json_t *label = json_stringn(descriptor->label->text, descriptor->label->size);
    json_object_set_new(root, "label", label);

    // actuators
    json_t *actuators = json_array();
    json_object_set_new(root, "actuators", actuators);

    // populate actuators list
    for (int i = 0; i < descriptor->actuators_count; i++)
    {
        json_t *actuator = json_object();

        // actuator id
        json_t *id = json_integer(descriptor->actuators[i]->id);
        json_object_set_new(actuator, "id", id);

        // add to list
        json_array_append_new(actuators, actuator);
    }

    char *str = json_dumps(root, 0);

    // free json object
    json_decref(root);

    return str;
}

int* cc_device_list(int filter)
{
    int count = 0;
    int *devices_list = malloc((CC_MAX_DEVICES + 1) * sizeof(int));

    if (g_devices_initialized)
    {
        for (int i = 0; i < CC_MAX_DEVICES; i++)
        {
            if (g_devices[i].id < 0)
                continue;

            if (filter == CC_DEVICE_LIST_ALL ||
               (filter == CC_DEVICE_LIST_REGISTERED && g_devices[i].descriptor) ||
               (filter == CC_DEVICE_LIST_UNREGISTERED && !g_devices[i].descriptor))
            {
                devices_list[count++] = g_devices[i].id;
            }
        }
    }

    devices_list[count] = 0;
    return devices_list;
}

cc_device_t* cc_device_get(int device_id)
{
    for (int i = 0; i < CC_MAX_DEVICES; i++)
    {
        if (g_devices[i].id == device_id)
            return &g_devices[i];
    }

    return 0;
}
