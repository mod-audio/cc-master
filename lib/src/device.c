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

cc_device_t* cc_device_create(cc_handshake_dev_t *handshake)
{
    for (int i = 0; i < CC_MAX_DEVICES; i++)
    {
        if (g_devices[i].id == 0)
        {
            // device id cannot be zero
            g_devices[i].id = i + 1;

            // set device label to null
            g_devices[i].label = 0;

            // store handshake info
            g_devices[i].uri = handshake->uri;
            g_devices[i].firmware.major = handshake->firmware.major;
            g_devices[i].firmware.minor = handshake->firmware.minor;
            g_devices[i].firmware.micro = handshake->firmware.micro;

            return &g_devices[i];
        }
    }

    return 0;
}

void cc_device_destroy(int device_id)
{
    cc_device_t *device = cc_device_get(device_id);

    if (!device)
        return;

    // destroy assigments list
    if (device->assignments)
    {
        free(device->assignments);
        device->assignments = 0;
    }

    // destroy actuators
    if (device->actuators)
    {
        for (int i = 0; i < device->actuators_count; i++)
            free(device->actuators[i]);

        free(device->actuators);
    }

    // destroy URI and label
    string_destroy(device->uri);
    string_destroy(device->label);
    device->label = 0;

    // reset status and id
    device->status = CC_DEVICE_DISCONNECTED;
    device->id = 0;
}

char* cc_device_descriptor(int device_id)
{
    cc_device_t* device = cc_device_get(device_id);

    if (!device)
        return 0;

    json_t *root = json_object();

    // label
    json_t *label = json_stringn(device->label->text, device->label->size);
    json_object_set_new(root, "label", label);

    // firmware version
    char buffer[16];
    sprintf(buffer, "%i.%i.%i",
        device->firmware.major, device->firmware.minor, device->firmware.micro);
    json_t *version = json_stringn(buffer, strlen(buffer));
    json_object_set_new(root, "version", version);

    // actuators
    json_t *actuators = json_array();
    json_object_set_new(root, "actuators", actuators);

    // populate actuators list
    for (int i = 0; i < device->actuators_count; i++)
    {
        json_t *actuator = json_object();

        // actuator id
        json_t *id = json_integer(device->actuators[i]->id);
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

    for (int i = 0; i < CC_MAX_DEVICES; i++)
    {
        if (!g_devices[i].id)
            continue;

        if (filter == CC_DEVICE_LIST_ALL ||
           (filter == CC_DEVICE_LIST_REGISTERED && g_devices[i].label) ||
           (filter == CC_DEVICE_LIST_UNREGISTERED && !g_devices[i].label))
        {
            devices_list[count++] = g_devices[i].id;
        }
    }

    devices_list[count] = 0;
    return devices_list;
}

cc_device_t* cc_device_get(int device_id)
{
    if (device_id)
    {
        for (int i = 0; i < CC_MAX_DEVICES; i++)
        {
            if (g_devices[i].id == device_id)
                return &g_devices[i];
        }
    }

    return 0;
}
