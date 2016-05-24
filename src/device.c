
/*
************************************************************************************************************************
*       INCLUDE FILES
************************************************************************************************************************
*/

#include <stdlib.h>
#include <string.h>
#include "device.h"
#include "actuator.h"


/*
************************************************************************************************************************
*       INTERNAL MACROS
************************************************************************************************************************
*/

// device status
enum {DEV_WAITING_HANDSHAKE, DEV_WAITING_DESCRIPTOR};


/*
************************************************************************************************************************
*       INTERNAL CONSTANTS
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*       INTERNAL DATA TYPES
************************************************************************************************************************
*/

typedef struct string_t {
    uint8_t size;
    char *text;
} string_t;

typedef struct cc_dev_descriptor_t {
    string_t *label;
    uint8_t actuators_count;
    cc_actuator_t **actuators;
} cc_dev_descriptor_t;

typedef struct cc_device_t {
    int id, status;
    cc_dev_descriptor_t *descriptor;
} cc_device_t;


/*
************************************************************************************************************************
*       INTERNAL GLOBAL VARIABLES
************************************************************************************************************************
*/

static cc_device_t g_devices[CC_MAX_DEVICES];


/*
************************************************************************************************************************
*       INTERNAL FUNCTIONS
************************************************************************************************************************
*/

string_t *string_create(const uint8_t *data, uint32_t *written)
{
    string_t *str = malloc(sizeof(string_t));
    *written = 0;

    if (str)
    {
        str->size = *data++;
        str->text = malloc(str->size);
        if (str->text)
        {
            memcpy(str->text, data, str->size);
            *written = str->size;
        }
        else
        {
            free(str);
        }
    }

    return str;
}

void string_destroy(string_t *str)
{
    if (str)
    {
        if (str->text)
            free(str->text);

        free(str);
    }
}


/*
************************************************************************************************************************
*       GLOBAL FUNCTIONS
************************************************************************************************************************
*/

int cc_device_handshake(void)
{
    // get next free device
    for (int i = 0; i < CC_MAX_DEVICES; i++)
    {
        if (g_devices[i].status == DEV_WAITING_HANDSHAKE)
        {
            g_devices[i].id = i + 1;
            g_devices[i].status++;
            return g_devices[i].id;
        }
    }

    return -1;
}

int cc_device_add(cc_msg_t *msg)
{
    uint8_t *pdata = msg->data;
    int idx = msg->dev_address - 1;
    cc_device_t *dev = &g_devices[idx];

    if (dev->status == DEV_WAITING_DESCRIPTOR)
    {
        dev->status++;

        // create descriptor
        cc_dev_descriptor_t *desc = malloc(sizeof(cc_dev_descriptor_t));
        dev->descriptor = desc;

        uint32_t n;

        // create label
        desc->label = string_create(pdata, &n);
        pdata += n;

        // create actuators list
        desc->actuators_count = *pdata++;
        desc->actuators = malloc(sizeof(cc_actuator_t *) * desc->actuators_count);

        // create each actuator
        for (int i = 0; i < desc->actuators_count; i++)
        {
            desc->actuators[i] = cc_actuator_create(pdata, &n);
            pdata += n;
        }

        return g_devices[idx].id;
    }

    return -1;
}

void cc_device_remove(int device_id)
{
    g_devices[device_id].status = DEV_WAITING_HANDSHAKE;
}

int* cc_device_missing_descriptors(void)
{
    static int missing_descriptors[CC_MAX_DEVICES+1];

    int j = 0;
    for (int i = 0; i < CC_MAX_DEVICES; i++)
    {
        if (g_devices[i].status == DEV_WAITING_DESCRIPTOR)
        {
            missing_descriptors[j++] = g_devices[i].id;
        }
    }

    missing_descriptors[j] = 0;
    return missing_descriptors;
}
