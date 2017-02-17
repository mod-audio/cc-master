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

#define DATA_BUFFER_SIZE    256


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
    msg->header = calloc(1, DATA_BUFFER_SIZE);
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

        // URI
        uint32_t size;
        handshake->uri = string_deserialize(pdata, &size);
        pdata += size;

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

        // device label
        device->label = string_deserialize(pdata, &i);
        pdata += i;

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

                // actuator maximum assignments
                actuator->max_assignments = *pdata++;
            }
        }
    }
    else if (msg->command == CC_CMD_DATA_UPDATE)
    {
        cc_update_list_t **updates = data_struct;
        *updates = cc_update_parse(msg->device_id, msg->data);
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

        // status, frame, channel
        *pdata++ = handshake->status;
        *pdata++ = handshake->device_id;
        *pdata++ = handshake->channel;
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
    }
    else if (command == CC_CMD_UNASSIGNMENT)
    {
        const cc_unassignment_t *unassignment = data_struct;

        // device id
        msg->device_id = unassignment->device_id;

        // assignment id
        *pdata++ = unassignment->assignment_id;
    }

    msg->data_size = (pdata - msg->data);

    return msg;
}
