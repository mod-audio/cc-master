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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "msg.h"
#include "handshake.h"
#include "device.h"
#include "assignment.h"
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

cc_msg_t* cc_msg_new(void)
{
    cc_msg_t *msg = calloc(1, sizeof(cc_msg_t));
    msg->header = calloc(1, CC_DATA_BUFFER_SIZE);
    msg->data = &msg->header[CC_MSG_HEADER_SIZE];

    return msg;
}

void cc_msg_delete(cc_msg_t *msg)
{
    if (msg)
    {
        free(msg->header);
        free(msg);
    }
}

void cc_msg_parser(const cc_msg_t *msg, void *data_struct)
{
    if (msg->command == CC_CMD_HANDSHAKE)
    {
        cc_handshake_dev_t *handshake = data_struct;
        uint8_t *pdata = msg->data;

        // old versions of protocol (before v0.4) used to sent the URI during the handshake
        // assume the URI is within the hanshake if data size > 7 bytes
        if (msg->data_size > 7)
        {
            uint32_t size;
            handshake->uri = string_deserialize(pdata, &size);
            pdata += size;
        }
        else
        {
            handshake->uri = 0;
        }

        // random id
        handshake->random_id = *((uint16_t *) pdata);
        pdata += sizeof(uint16_t);

        // device protocol version
        handshake->protocol.major = *pdata++;
        handshake->protocol.minor = *pdata++;
        handshake->protocol.micro = 0;

        // device firmware version
        handshake->firmware.major = *pdata++;
        handshake->firmware.minor = *pdata++;
        handshake->firmware.micro = *pdata++;
    }
    else if (msg->command == CC_CMD_DEV_DESCRIPTOR)
    {
        cc_device_t *device = data_struct;
        uint8_t *pdata = msg->data;
        uint32_t i = 0;

        // URI was added to device descriptor starting from v0.4
        if (device->protocol.major > 0 || device->protocol.minor >= 4)
        {
            // URI
            device->uri = string_deserialize(pdata, &i);
            pdata += i;
        }

        // device label
        device->label = string_deserialize(pdata, &i);
        pdata += i;

        // device channel
        device->channel = cc_device_count(device->uri->text);

        // number of actuators
        device->actuators = 0;
        device->actuators_count = *pdata++;

        // list of actuators
        if (device->actuators_count > 0)
        {
            device->actuators = malloc(sizeof(cc_actuator_t *) * device->actuators_count);
            
            for (int j = 0; j < device->actuators_count; j++)
            {
                device->actuators[j] = malloc(sizeof(cc_actuator_t));
                cc_actuator_t *actuator = device->actuators[j];

                // actuator id
                actuator->id = j;

                // actuator name
                actuator->name = string_deserialize(pdata, &i);
                pdata += i;

                // actuator supported modes
                actuator->supported_modes = *((uint32_t *) pdata);
                pdata += sizeof(uint32_t);

                // actuator assignments counter and maximum value
                actuator->max_assignments = *pdata++;
                actuator->assignments_count = 0;

                // set actuatorgroup flag
                actuator->grouped = 0;
            }
        }

        if (device->protocol.major > 0 || device->protocol.minor >= 7)
        {
            // number of actuatorgroups
            device->actuatorgroups = 0;
            device->actuatorgroups_count = *pdata++;

            //list of actuatorgroups
            if (device->actuatorgroups_count > 0)
            {
                device->actuatorgroups = malloc(sizeof(cc_actuatorgroup_t *) * device->actuatorgroups_count);

                for (int j = 0; j < device->actuatorgroups_count; j++)
                {
                    int actuatorgroup_id = device->actuators_count++;

                    device->actuatorgroups[j] = malloc(sizeof(cc_actuatorgroup_t));
                    cc_actuatorgroup_t *actuatorgroup = device->actuatorgroups[j];

                    // actuatorgroup id
                    actuatorgroup->id = actuatorgroup_id;

                    //actuatorgroup name
                    actuatorgroup->name = string_deserialize(pdata, &i);
                    pdata += i;

                    //actuators in actuatorgroup
                    actuatorgroup->actuators_in_actuatorgroup[0] = *pdata++;
                    actuatorgroup->actuators_in_actuatorgroup[1] = *pdata++;

                    actuatorgroup_id++;
                }

            }

        }

    }
    else if (msg->command == CC_CMD_DATA_UPDATE)
    {
        cc_update_list_t **updates = data_struct;
        *updates = cc_update_parse(msg->device_id, msg->data, 1);
    }
}

cc_msg_t* cc_msg_builder(int device_id, int command, const void *data_struct)
{
    cc_msg_t *msg = cc_msg_new();

    uint8_t *pdata = msg->data;
    msg->device_id = device_id;
    msg->command = command;

    if (command == CC_CMD_HANDSHAKE)
    {
        const cc_handshake_mod_t *handshake = data_struct;

        // random id
        uint16_t *random_id = (uint16_t *) pdata;
        *random_id = handshake->random_id;
        pdata += 2;

        // status, device id
        *pdata++ = handshake->status;
        *pdata++ = handshake->device_id;
    }
    else if (command == CC_CMD_DEV_CONTROL)
    {
        const int *control = data_struct;
        *pdata++ = *control;
    }
    else if (command == CC_CMD_ASSIGNMENT)
    {
        const cc_assignment_t *assignment = data_struct;

        // device id
        msg->device_id = assignment->device_id;

        // assignment id, actuator id
        *pdata++ = assignment->id;
        *pdata++ = assignment->actuator_id;

        // assignment label
        if (assignment->label)
        {
            int size = strlen(assignment->label);
            *pdata++ = size;
            memcpy(pdata, assignment->label, size);
            pdata += size;
        }
        else
        {
            *pdata++ = 0;
        }

        // value, min, max, def
        pdata += float_to_bytes(assignment->value, pdata);
        pdata += float_to_bytes(assignment->min, pdata);
        pdata += float_to_bytes(assignment->max, pdata);
        pdata += float_to_bytes(assignment->def, pdata);

        // mode
        uint32_t *mode = (uint32_t *) pdata;
        *mode = assignment->mode;
        pdata += 4;

        // steps
        uint16_t *steps = (uint16_t *) pdata;
        *steps = assignment->steps;
        pdata += 2;

        // unit
        if (assignment->unit)
        {
            int size = strlen(assignment->unit);
            *pdata++ = size;
            memcpy(pdata, assignment->unit, size);
            pdata += size;
        }
        else
        {
            *pdata++ = 0;
        }

        // list count
        *pdata++ = assignment->list_count;
        
        // list items
        for (int i = 0; i < assignment->list_count; i++)
        {
            cc_item_t *item = assignment->list_items[i];

            // item label
            int size = strlen(item->label);
            *pdata++ = size;
            memcpy(pdata, item->label, size);
            pdata += size;

            // item value
            pdata += float_to_bytes(item->value, pdata);
        }
    }
    else if (command == CC_CMD_UNASSIGNMENT)
    {
        const cc_assignment_key_t *assignment = data_struct;

        // device id
        msg->device_id = assignment->device_id;

        // assignment id
        *pdata++ = assignment->id;
    }
    else if (msg->command == CC_CMD_SET_VALUE)
    {
        const cc_set_value_t *update = data_struct;

        // device id
        msg->device_id = update->device_id;

        // assignment id, actuator id
        *pdata++ = update->assignment_id;
        *pdata++ = update->actuator_id;
        
        // value
        pdata += float_to_bytes(update->value, pdata);
    }

    msg->data_size = (pdata - msg->data);

    return msg;
}

void cc_msg_print(const char *header, const cc_msg_t *msg)
{
    static int debug = -1;

    // enable msg debug print if debug level is greater than 1
    if (debug == -1)
    {
        char *dbg = getenv("LIBCONTROLCHAIN_DEBUG");
        debug = (!dbg || atoi(dbg) <= 1) ? 0 : 1;
    }

    if (debug == 0)
        return;

    static const char *commands[] = {"sync", "handshake", "device control", "device descriptor",
        "assignment", "data update", "unassignment"};

    if (msg->command == CC_CMD_CHAIN_SYNC)
        return;

    // print header information
    printf("%s: device: %i, command: %s, data size: %i\n",
        header, msg->device_id, commands[msg->command], msg->data_size);

    // print data in hexadecimal
    printf("      data:");
    for (int i = 0, j = 0; i < msg->data_size; i++)
    {
        printf(" %02X", msg->data[i]);
        if (++j == 32 && (i != msg->data_size - 1))
        {
            printf("\n           ");
            j = 0;
        }
    }

    // print data in ASCII
    printf("\n      text: ");
    for (int i = 0; i < msg->data_size; i++)
    {
        if (msg->data[i] < 32 || msg->data[i] > 126)
            continue;

        printf("%c", msg->data[i]);
    }

    printf("\n---\n");
}
