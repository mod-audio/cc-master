/*
****************************************************************************************************
*       INCLUDE FILES
****************************************************************************************************
*/

#include <stdlib.h>
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

static device_t g_devices[CC_MAX_DEVICES];
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
    device_t *device = cc_device_get(device_id);
    device->id = -1;

    // destroy assigments list
    if (device->assignments)
        lili_destroy(device->assignments);

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

void cc_device_descriptor(int device_id, cc_dev_descriptor_t *descriptor)
{
    device_t* device = cc_device_get(device_id);
    device->descriptor = descriptor;
}

device_t** cc_device_list(int filter)
{
    int count = 0;
    static device_t *devices_list[CC_MAX_DEVICES+1];

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
                devices_list[count++] = &g_devices[i];
            }
        }
    }

    devices_list[count] = 0;
    return devices_list;
}

device_t* cc_device_get(int device_id)
{
    for (int i = 0; i < CC_MAX_DEVICES; i++)
    {
        if (g_devices[i].id == device_id)
            return &g_devices[i];
    }

    return 0;
}
