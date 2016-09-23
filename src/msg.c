/*
****************************************************************************************************
*       INCLUDE FILES
****************************************************************************************************
*/

#include <stdlib.h>
#include "msg.h"
//#include "handshake.h"


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
    cc_msg_t *msg = malloc(sizeof(cc_msg_t));
    msg->header = malloc(DATA_BUFFER_SIZE);
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
#if 0
void cc_msg_parser(const cc_msg_t *msg, void *data_struct)
{
    if (msg->command == CC_CMD_HANDSHAKE)
    {
        uint32_t i;
        cc_handshake_dev_t *handshake = data_struct;

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
    }
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
        *pdata++ = handshake->frame;
        *pdata++ = handshake->channel;

        msg->data_size = (pdata - msg->data);
    }
}
#endif
