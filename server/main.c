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

#define MAX_CLIENTS_EVENTS  10

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

typedef struct clients_events_t {
    int client_fd;
    int event_id;
} clients_events_t;


/*
****************************************************************************************************
*       INTERNAL GLOBAL VARIABLES
****************************************************************************************************
*/

static sockser_t *g_server;
static clients_events_t g_client_events[MAX_CLIENTS_EVENTS];


/*
****************************************************************************************************
*       INTERNAL FUNCTIONS
****************************************************************************************************
*/

static void event_off(int client_fd)
{
    for (int i = 0; i < MAX_CLIENTS_EVENTS; i++)
    {
        if (g_client_events[i].client_fd == client_fd)
        {
            g_client_events[i].client_fd = 0;
        }
    }
}

static void event_set(int client_fd, int event_id, int event_enable)
{
    int index = -1;

    for (int i = 0; i < MAX_CLIENTS_EVENTS; i++)
    {
        // store position of first free spot
        if (g_client_events[i].client_fd == 0 && index < 0)
        {
            index = i;
        }

        // check if event already exists for this client
        if (g_client_events[i].client_fd == client_fd &&
            g_client_events[i].event_id == event_id)
        {
            if (!event_enable)
            {
                g_client_events[i].client_fd = 0;
                g_client_events[i].event_id = event_id;
            }

            return;
        }
    }

    // add event
    if (index >= 0 && event_enable)
    {
        g_client_events[index].client_fd = client_fd;
        g_client_events[index].event_id = event_id;
    }
}

static void device_status_cb(void *arg)
{
    cc_device_t *device = arg;
    char request = CC_DEVICE_STATUS;

    int status[2];
    status[0] = device->id;
    status[1] = device->status;

    for (int i = 0; i < MAX_CLIENTS_EVENTS; i++)
    {
        if (g_client_events[i].client_fd == 0)
            continue;

        if (g_client_events[i].event_id == request)
        {
            sockser_data_t data;
            data.client_fd = g_client_events[i].client_fd;

            // request id
            data.buffer = &request;
            data.size = sizeof(request);
            sockser_write(&data);

            // device status
            data.buffer = status;
            data.size = sizeof(status);
            sockser_write(&data);
        }
    }
}

static void data_update_cb(void *arg)
{
    cc_update_list_t *updates = arg;
    char request = CC_DATA_UPDATE;

    for (int i = 0; i < MAX_CLIENTS_EVENTS; i++)
    {
        if (g_client_events[i].client_fd == 0)
            continue;

        if (g_client_events[i].event_id == request)
        {
            sockser_data_t data;
            data.client_fd = g_client_events[i].client_fd;

            // request id
            data.buffer = &request;
            data.size = sizeof(request);
            sockser_write(&data);

            // device id
            int device_id = updates->device_id;
            data.buffer = &device_id;
            data.size = sizeof(device_id);
            sockser_write(&data);

            // update list
            data.buffer = updates->raw_data;
            data.size = updates->raw_size;
            sockser_write(&data);
        }
    }
}

static void client_event_cb(void *arg)
{
    sockser_event_t *event = arg;

    if (event->id == SOCKSER_CLIENT_DISCONNECTED)
    {
        event_off(event->client_fd);
    }
}


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

    // set callback for clients events
    sockser_client_event_cb(g_server, client_event_cb);

    // init control chain
    cc_handle_t *handle = cc_init(SERIAL_PORT, SERIAL_BAUDRATE);
    if (!handle)
    {
        printf("can't initiate control chain using %s\n", SERIAL_PORT);
        return -1;
    }

    printf("control chain started\n");

    // set control chain callbacks
    cc_device_status_cb(handle, device_status_cb);
    cc_data_update_cb(handle, data_update_cb);

    char read_buffer[4096];
    sockser_data_t data;

    while (1)
    {
        data.buffer = read_buffer;
        int n = sockser_read(g_server, &data);

        char *buffer = data.buffer;
        char *pbuffer = data.buffer;

        while ((pbuffer - buffer) < n)
        {
            char request = *pbuffer++;

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
                pbuffer += sizeof(cc_assignment_t);

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
                pbuffer += sizeof(cc_unassignment_t);

                cc_unassignment(handle, &unassignment);
            }
            else if (request == CC_DEVICE_STATUS || request == CC_DATA_UPDATE)
            {
                char event_enable = *pbuffer++;
                event_set(data.client_fd, request, event_enable);
            }
        }
    }

    cc_finish(handle);
    sockser_finish(g_server);

    return 0;
}

// TODO: daemonize
