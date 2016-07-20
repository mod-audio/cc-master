
/*
************************************************************************************************************************
*       INCLUDE FILES
************************************************************************************************************************
*/

#include <stdlib.h>
#include "device.h"


/*
************************************************************************************************************************
*       INTERNAL MACROS
************************************************************************************************************************
*/

// device status
enum {DEV_WAITING_HANDSHAKE, DEV_WAITING_DESCRIPTOR, DEV_WAITING_ASSIGNMENT};


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

typedef struct device_t {
    int id, status;
    cc_dev_descriptor_t *descriptor;
} device_t;


/*
************************************************************************************************************************
*       INTERNAL GLOBAL VARIABLES
************************************************************************************************************************
*/

static device_t g_devices[CC_MAX_DEVICES];


/*
************************************************************************************************************************
*       INTERNAL FUNCTIONS
************************************************************************************************************************
*/

cc_actuator_t *cc_actuator_create(const uint8_t *data, uint32_t *written)
{
    cc_actuator_t *actuator = malloc(sizeof(cc_actuator_t));
    actuator->id = *data++;

    *written = 1;
    return actuator;
}

void cc_actuator_destroy(cc_actuator_t *actuator)
{
    if (actuator)
    {
        free(actuator);
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

cc_dev_descriptor_t* cc_device_add(uint8_t device_id, const uint8_t *data)
{
    int idx = device_id - 1;
    device_t *dev = &g_devices[idx];

    if (dev->status == DEV_WAITING_DESCRIPTOR)
    {
        dev->status++;

        // create descriptor
        cc_dev_descriptor_t *desc = malloc(sizeof(cc_dev_descriptor_t));
        dev->descriptor = desc;

        desc->id = device_id;

        uint32_t n;

        // create label
        desc->label = string_create(data, &n);
        data += n;

        // create actuators list
        desc->actuators_count = *data++;
        desc->actuators = malloc(sizeof(cc_actuator_t *) * desc->actuators_count);

        // create each actuator
        for (int i = 0; i < desc->actuators_count; i++)
        {
            desc->actuators[i] = cc_actuator_create(data, &n);
            data += n;
        }

        return desc;
    }

    return NULL;
}

void cc_device_remove(int device_id)
{
    int idx = device_id - 1;

    if (g_devices[idx].id > 0)
    {
        cc_dev_descriptor_t *desc = g_devices[idx].descriptor;
        if (desc)
        {
            if (desc->label)
                string_destroy(desc->label);

            if (desc->actuators)
            {
                for (int i = 0; i < desc->actuators_count; i++)
                {
                    cc_actuator_destroy(desc->actuators[i]);
                }
                free(desc->actuators);
            }

            free(desc);
        }

        g_devices[idx].id = 0;
        g_devices[idx].status = 0;
    }
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
