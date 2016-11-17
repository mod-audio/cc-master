/*
****************************************************************************************************
*       INCLUDE FILES
****************************************************************************************************
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cc/control_chain.h>
#include <cc/cc_client.h>

#include "sockser.h"


/*
****************************************************************************************************
*       INTERNAL MACROS
****************************************************************************************************
*/

#define SERIAL_PORT         "/dev/ttyACM0"
#define SERIAL_BAUDRATE     115200


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

static sockser_t *g_server;



/*
****************************************************************************************************
*       INTERNAL FUNCTIONS
****************************************************************************************************
*/
#if 0
static void device_status_cb(void *arg)
{
    cc_device_t *device = arg;
    char request = CC_DEVICE_STATUS;
}

static void data_update_cb(void *arg)
{
    cc_update_list_t *updates = arg;
    char request = CC_DATA_UPDATE;
}
#endif

/*
****************************************************************************************************
*       GLOBAL FUNCTIONS
****************************************************************************************************
*/

int main(void)
{
    // init server
    g_server = sockser_init("/tmp/control-chain.sock");
    printf("server is running\n");

    // init control chain
    cc_handle_t *handle = cc_init(SERIAL_PORT, SERIAL_BAUDRATE);
    if (!handle)
    {
        printf("can't initiate control chain using %s\n", SERIAL_PORT);
        return -1;
    }

    printf("control chain started\n");

    // set control chain callbacks
    //cc_device_status_cb(handle, device_status_cb);
    //cc_data_update_cb(handle, data_update_cb);

    char read_buffer[4096];
    sockser_data_t data;

    while (1)
    {
        data.buffer = read_buffer;
        sockser_read(g_server, &data);

        char *pbuffer = data.buffer;
        char request = pbuffer[0];
        pbuffer++;

        if (request == CC_DEVICE_LIST)
        {
            // list only devices with descriptor
            int *devices_id = cc_device_list(CC_DEVICE_LIST_REGISTERED);

            // count number of devices
            int n_devices = 0;
            while (devices_id[n_devices++]);

            // request id
            data.buffer = &request;
            data.size = sizeof(request);
            sockser_write(&data);

            // devices count
            data.buffer = &n_devices;
            data.size = sizeof(n_devices);
            sockser_write(&data);

            // devices list
            data.buffer = devices_id;
            data.size = sizeof(int) * n_devices;
            sockser_write(&data);
        }
        else if (request == CC_DEVICE_DESCRIPTOR)
        {
            int device_id = (*(int *) pbuffer);
            pbuffer += sizeof(int);
            char *descriptor = cc_device_descriptor(device_id);

            // request id
            data.buffer = &request;
            data.size = sizeof(request);
            sockser_write(&data);

            // device descriptor
            data.buffer = descriptor;
            data.size = strlen(descriptor);
            sockser_write(&data);

            // free descriptor
            free(descriptor);
        }
        else if (request == CC_ASSIGNMENT)
        {
            cc_assignment_t assignment;
            memcpy(&assignment, pbuffer, sizeof(cc_assignment_t));
            int assignment_id = cc_assignment(handle, &assignment);

            // request id
            data.buffer = &request;
            data.size = sizeof(request);
            sockser_write(&data);

            // assignment id
            data.buffer = &assignment_id;
            data.size = sizeof(assignment_id);
            sockser_write(&data);
        }
        else if (request == CC_UNASSIGNMENT)
        {
            cc_unassignment_t unassignment;
            memcpy(&unassignment, pbuffer, sizeof(cc_unassignment_t));
            cc_unassignment(handle, &unassignment);
        }
    }

    cc_finish(handle);
    sockser_finish(g_server);

    return 0;
}

// TODO: daemonize
