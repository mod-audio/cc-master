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

cc_msg_t *cc_msg_new(void)
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

void* cc_msg_parser(const cc_msg_t *msg)
{
    if (msg->command == CC_CMD_HANDSHAKE)
    {
        uint32_t i;
        cc_handshake_dev_t *handshake = malloc(sizeof(cc_handshake_dev_t));

        // URI
        handshake->uri = string_deserialize(msg->data, &i);

        // random id
        handshake->random_id = *((uint16_t *) &msg->data[i]);
        i += 2;

        // device protocol version
        handshake->protocol.major = msg->data[i++];
        handshake->protocol.minor = msg->data[i++];
        handshake->protocol.micro = 0;

        // device firmware version
        handshake->firmware.major = msg->data[i++];
        handshake->firmware.minor = msg->data[i++];
        handshake->firmware.micro = msg->data[i++];

        return handshake;
    }
    else if (msg->command == CC_CMD_DEV_DESCRIPTOR)
    {
        uint32_t i;
        cc_dev_descriptor_t *descriptor = malloc(sizeof(cc_dev_descriptor_t));
        uint8_t *pdata = msg->data;

        // device label
        descriptor->label = string_deserialize(pdata, &i);
        pdata += i;

        // number of actuators
        descriptor->actuators = 0;
        descriptor->actuators_count = *pdata++;

        // list of actuators
        if (descriptor->actuators_count > 0)
        {
            descriptor->actuators = malloc(sizeof(cc_actuator_t *) * descriptor->actuators_count);
            for (int j = 0; j < descriptor->actuators_count; j++)
            {
                descriptor->actuators[j] = malloc(sizeof(cc_actuator_t));
                cc_actuator_t *actuator = descriptor->actuators[j];

                actuator->id = *pdata++;
            }
        }

        return descriptor;
    }
    else if (msg->command == CC_CMD_DATA_UPDATE)
    {
        cc_update_list_t *updates = malloc(sizeof(cc_update_list_t));

        // store raw data
        updates->raw_data = malloc(msg->data_size);
        updates->raw_size = msg->data_size;
        memcpy(updates->raw_data, msg->data, msg->data_size);

        // parse data to struct
        updates->count = msg->data[0];
        updates->list = malloc(sizeof(cc_data_t) * updates->count);

        for (int i = 0, j = 1; i < updates->count; i++)
        {
            cc_data_t *data = &updates->list[i];
            data->assignment_id = msg->data[j++];
            float *value = (float *) &msg->data[j];
            data->value = *value;
            j += sizeof (float);
        }

        return updates;
    }

    return 0;
}

void cc_msg_builder(int command, const void *data_struct, cc_msg_t *msg)
{
    msg->command = command;

    if (command == CC_CMD_HANDSHAKE)
    {
        const cc_handshake_mod_t *handshake = data_struct;
        uint8_t *pdata = msg->data;

        // random id
        uint16_t *random_id = (uint16_t *) pdata;
        *random_id = handshake->random_id;
        pdata += 2;

        // status, frame, channel
        *pdata++ = handshake->status;
        *pdata++ = handshake->address;
        *pdata++ = handshake->channel;

        msg->data_size = (pdata - msg->data);
    }
    else if (command == CC_CMD_ASSIGNMENT)
    {
        const cc_assignment_t *assignment = data_struct;
        uint8_t *pdata = msg->data;

        // device address
        msg->dev_address = assignment->device_id;

        // actuator id
        *pdata++ = assignment->actuator_id;

        // value, min, max, def
        pdata += float_to_bytes(assignment->value, pdata);
        pdata += float_to_bytes(assignment->min, pdata);
        pdata += float_to_bytes(assignment->max, pdata);
        pdata += float_to_bytes(assignment->def, pdata);

        // mode
        uint32_t *mode = (uint32_t *) pdata;
        *mode = assignment->mode;
        pdata += 4;

        msg->data_size = (pdata - msg->data);
    }
    else if (command == CC_CMD_UNASSIGNMENT)
    {
        const cc_unassignment_t *unassignment = data_struct;
        uint8_t *pdata = msg->data;

        // device address
        msg->dev_address = unassignment->device_id;

        // assignment id
        *pdata++ = unassignment->assignment_id;

        msg->data_size = (pdata - msg->data);
    }
}
