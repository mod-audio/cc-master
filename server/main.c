/*
****************************************************************************************************
*       INCLUDE FILES
****************************************************************************************************
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jansson.h>
#include <cc/control_chain.h>
#include <cc/cc_client.h>

#include "sockser.h"
#include "base64.h"


/*
****************************************************************************************************
*       INTERNAL MACROS
****************************************************************************************************
*/

#define MAX_CLIENTS_EVENTS  10
#define BUFFER_SIZE         8*1024

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

enum {CC_DEVICE_STATUS_EV, CC_DATA_UPDATE_EV};

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

static void send_reply(const char *reply, json_t *data, sockser_data_t *output)
{
    // build reply: {"reply":"name", "data": {...}}
    json_t *root = json_object();
    json_object_set_new(root, "reply", json_string(reply));
    json_object_set(root, "data", data);

    // dump json and send reply
    output->buffer = json_dumps(root, 0);
    output->size = strlen(output->buffer) + 1;
    sockser_write(output);

    // free memory
    json_decref(root);
    json_decref(data);
    free(output->buffer);
}

static void send_event(int client_fd, const char *event, const char *data)
{
    sockser_data_t output;
    char buffer[BUFFER_SIZE];

    // build json event
    sprintf(buffer, "{\"event\":\"%s\",\"data\":%s}", event, data);

    output.client_fd = client_fd;
    output.buffer = buffer;
    output.size = strlen(buffer) + 1;
    sockser_write(&output);
}

static void device_status_cb(void *arg)
{
    cc_device_t *device = arg;
    char buffer[BUFFER_SIZE];

    for (int i = 0; i < MAX_CLIENTS_EVENTS; i++)
    {
        if (g_client_events[i].client_fd == 0)
            continue;

        if (g_client_events[i].event_id == CC_DEVICE_STATUS_EV)
        {
            // build json event data
            sprintf(buffer, "{\"device_id\":%i,\"status\":%i}", device->id, device->status);

            // send event
            int client_fd = g_client_events[i].client_fd;
            send_event(client_fd, "device_status", buffer);
        }
    }
}

static void data_update_cb(void *arg)
{
    cc_update_list_t *updates = arg;
    char buffer[BUFFER_SIZE];
    char raw_data[BUFFER_SIZE];

    for (int i = 0; i < MAX_CLIENTS_EVENTS; i++)
    {
        if (g_client_events[i].client_fd == 0)
            continue;

        if (g_client_events[i].event_id == CC_DATA_UPDATE_EV)
        {
            // encode data
            base64_encode(updates->raw_data, updates->raw_size, raw_data);

            // build json event data
            sprintf(buffer, "{\"device_id\":%i,\"raw_data\":\"%s\"}",
                updates->device_id, raw_data);

            // send event
            int client_fd = g_client_events[i].client_fd;
            send_event(client_fd, "data_update", buffer);
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

    char read_buffer[BUFFER_SIZE];
    sockser_data_t read_data;

    while (1)
    {
        read_data.buffer = read_buffer;
        sockser_read_string(g_server, &read_data);

        json_error_t error;
        json_t *root = json_loads(read_data.buffer, 0, &error);

        const char *request = json_string_value(json_object_get(root, "request"));
        json_t *data = json_object_get(root, "data");

        if (strcmp(request, "device_list") == 0)
        {
            // list only devices with descriptor
            int *devices_id = cc_device_list(CC_DEVICE_LIST_REGISTERED);

            // create json array
            json_t *array = json_array();
            int n_devices = 0;
            while (devices_id[n_devices])
            {
                json_array_append_new(array, json_integer(devices_id[n_devices]));
                n_devices++;
            }

            // send reply
            send_reply(request, array, &read_data);

            free(devices_id);
        }
        else if (strcmp(request, "device_descriptor") == 0)
        {
            int device_id;
            json_unpack(data, CC_DEV_DESCRIPTOR_REQ_FORMAT, "device_id", &device_id);

            char *descriptor = cc_device_descriptor(device_id);
            sprintf(read_data.buffer, "{\"reply\":\"%s\",\"data\":%s}", request, descriptor);

            read_data.size = strlen(read_data.buffer) + 1;
            sockser_write(&read_data);

            free(descriptor);
        }
        else if (strcmp(request, "device_status") == 0)
        {
            int enable;
            json_unpack(data, CC_DEV_STATUS_REQ_FORMAT, "enable", &enable);

            event_set(read_data.client_fd, CC_DEVICE_STATUS_EV, enable);

            // pack data and send reply
            json_t *data = json_pack(CC_DEV_STATUS_REPLY_FORMAT);
            send_reply(request, data, &read_data);
        }
        else if (strcmp(request, "assignment") == 0)
        {
            cc_assignment_t assignment;

            json_unpack(data, CC_ASSIGNMENT_REQ_FORMAT,
                "device_id", &assignment.device_id,
                "actuator_id", &assignment.actuator_id,
                "value", &assignment.value,
                "min", &assignment.min,
                "max", &assignment.max,
                "def", &assignment.def,
                "mode", &assignment.mode);

            int assignment_id = cc_assignment(handle, &assignment);

            // pack data and send reply
            json_t *data = json_pack(CC_ASSIGNMENT_REPLY_FORMAT,
                "assignment_id", assignment_id);
            send_reply(request, data, &read_data);
        }
        else if (strcmp(request, "unassignment") == 0)
        {
            cc_unassignment_t unassignment;

            json_unpack(data, CC_UNASSIGNMENT_REQ_FORMAT,
                "device_id", &unassignment.device_id,
                "assignment_id", &unassignment.assignment_id);

            cc_unassignment(handle, &unassignment);

            // pack data and send reply
            json_t *data = json_pack(CC_UNASSIGNMENT_REPLY_FORMAT);
            send_reply(request, data, &read_data);
        }
        else if (strcmp(request, "data_update") == 0)
        {
            int enable;
            json_unpack(data, CC_DATA_UPDATE_REQ_FORMAT, "enable", &enable);

            event_set(read_data.client_fd, CC_DATA_UPDATE_EV, enable);

            // pack data and send reply
            json_t *data = json_pack(CC_DATA_UPDATE_REPLY_FORMAT);
            send_reply(request, data, &read_data);
        }
    }

    cc_finish(handle);
    sockser_finish(g_server);

    return 0;
}

// TODO: daemonize
