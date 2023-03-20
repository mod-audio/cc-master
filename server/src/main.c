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
#include <jansson.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>

#include "config.h"
#include "control_chain.h"
#include "request.h"
#include "sockser.h"
#include "base64.h"


/*
****************************************************************************************************
*       INTERNAL MACROS
****************************************************************************************************
*/

#define MAX_CLIENTS_EVENTS  10
#define BUFFER_SIZE         8*1024

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
static char *g_serial;
static int g_baudrate, g_foreground;


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

static void send_reply(int client_fd,  const char *reply, json_t *data)
{
    // build reply: {"reply":"name", "data": {...}}
    json_t *root = json_object();
    json_object_set_new(root, "reply", json_string(reply));
    json_object_set(root, "data", data);

    // create output message
    sockser_data_t output;
    output.client_fd = client_fd;
    output.buffer = json_dumps(root, 0);
    output.size = strlen(output.buffer) + 1;
    sockser_write(&output);

    // free memory
    json_decref(root);
    json_decref(data);
    free(output.buffer);
}

static void send_event(int client_fd, const char *event, const char *data)
{
    sockser_data_t output;
    char buffer[BUFFER_SIZE+64];

    // build json event
    snprintf(buffer, sizeof(buffer)-1, "{\"event\":\"%s\",\"data\":%s}", event, data);
    buffer[sizeof(buffer)-1] = 0;

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
            snprintf(buffer, sizeof(buffer)-1, "{\"device_id\":%i,\"status\":%i}", device->id, device->status);
            buffer[sizeof(buffer)-1] = 0;

            // send event
            int client_fd = g_client_events[i].client_fd;
            send_event(client_fd, "device_status", buffer);
        }
    }
}

static void data_update_cb(void *arg)
{
    cc_update_list_t *updates = arg;
    char buffer[BUFFER_SIZE+32];
    char encoded[BUFFER_SIZE];

    for (int i = 0; i < MAX_CLIENTS_EVENTS; i++)
    {
        if (g_client_events[i].client_fd == 0)
            continue;

        if (g_client_events[i].event_id == CC_DATA_UPDATE_EV)
        {
            // encode data
            base64_encode(updates->raw_data, updates->raw_size, encoded);

            // build json event data
            snprintf(buffer, sizeof(buffer), "{\"device_id\":%i,\"raw_data\":\"%s\"}",
                updates->device_id, encoded);

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

static void print_usage(int status)
{
    printf("Usage: " SERVER_NAME " <serial> [-bVh]\n");
    printf("  -b    define baud rate\n");
    printf("  -f    run server on foreground\n");
    printf("  -V,   display version information and exit\n");
    printf("  -h,   display this help and exit\n");

    exit(status);
}

static void print_version(void)
{
    printf(SERVER_NAME " " SERVER_VERSION " <https://github.com/moddevices/cc-master>\n");
    printf("License LGPLv3: <https://www.gnu.org/licenses/lgpl.html>.\n");
    printf("This is free software: you are free to change and redistribute it.\n");
    printf("There is NO WARRANTY, to the extent permitted by law.\n");

    exit(EXIT_SUCCESS);
}

static void parse_cmd_line(int argc, char **argv)
{
    if (argc < 2)
        print_usage(EXIT_FAILURE);

    g_serial = argv[1];
    g_baudrate = SERIAL_BAUDRATE;

    int opt;
    while ((opt = getopt(argc, argv, "bfVh")) != -1)
    {
        switch (opt)
        {
            case 'b':
                g_baudrate = atoi(argv[optind]);
                break;

            case 'f':
                g_foreground = 1;
                break;

            case 'V':
                print_version();
                break;

            case 'h':
                print_usage(EXIT_SUCCESS);
                break;
        }
    }
}

static void daemonize(void)
{
    // fork process
    pid_t pid = fork();
    if (pid < 0)
    {
        fprintf(stderr, "failed forking process\n");
        exit(EXIT_FAILURE);
    }
    else if (pid > 0)
    {
        // this is the parent process
        exit(EXIT_SUCCESS);
    }

    // unmask the file mode
    umask(0);

    // set new session
    pid_t sid = setsid();
    if (sid < 0)
    {
        fprintf(stderr, "failed creating new session\n");
        exit(EXIT_FAILURE);
    }

    // change current directory
    if ((chdir("/")) < 0)
    {
        fprintf(stderr, "failed changing current directory\n");
        exit(EXIT_FAILURE);
    }

    // close out the standard file descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}


/*
****************************************************************************************************
*       GLOBAL FUNCTIONS
****************************************************************************************************
*/

int main(int argc, char **argv)
{
    parse_cmd_line(argc, argv);

    if (!g_foreground)
        daemonize();

    // open syslog
    openlog(SERVER_NAME, LOG_PID, LOG_DAEMON);
    syslog(LOG_INFO, "daemon started");

    // open socket
    g_server = sockser_init("/tmp/control-chain.sock");
    if (!g_server)
    {
        syslog(LOG_ERR, "error when opening socket");
        return -1;
    }

    // set callback for socket clients events
    sockser_client_event_cb(g_server, client_event_cb);

    // init control chain
    cc_handle_t *handle = cc_init(g_serial, g_baudrate);
    if (!handle)
    {
        syslog(LOG_ERR, "error starting control chain using serial '%s'", g_serial);
        return -1;
    }

    syslog(LOG_INFO, "control chain library loaded");

    // set control chain callbacks
    cc_device_status_cb(handle, device_status_cb);
    cc_data_update_cb(handle, data_update_cb);

    char read_buffer[BUFFER_SIZE];
    sockser_data_t read_data;

    while (1)
    {
        read_data.buffer = read_buffer;
        int ret = sockser_read_string(g_server, &read_data);

        if (ret <= 0)
            continue;

        // client file descriptor
        int client_fd = read_data.client_fd;

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
            send_reply(client_fd, request, array);

            free(devices_id);
        }
        else if (strcmp(request, "device_descriptor") == 0)
        {
            int device_id;
            json_unpack(data, CC_DEV_DESCRIPTOR_REQ_FORMAT, "device_id", &device_id);

            char *descriptor = cc_device_descriptor(device_id);
            snprintf(read_buffer, sizeof(read_buffer)-1, "{\"reply\":\"%s\",\"data\":%s}", request, descriptor);
            read_buffer[sizeof(read_buffer)-1] = 0;

            read_data.buffer = read_buffer;
            read_data.size = strlen(read_buffer) + 1;
            sockser_write(&read_data);

            free(descriptor);
        }
        else if (strcmp(request, "device_control") == 0)
        {
            int device_id, enable;
            json_unpack(data, CC_DEV_CONTROL_REQ_FORMAT, "device_id", &device_id,
                "enable", &enable);

            cc_device_disable(handle, device_id);

            // pack data and send reply
            json_t *data = json_pack(CC_DEV_CONTROL_REPLY_FORMAT);
            send_reply(client_fd, request, data);
        }
        else if (strcmp(request, "device_status") == 0)
        {
            int enable;
            json_unpack(data, CC_DEV_STATUS_REQ_FORMAT, "enable", &enable);

            event_set(read_data.client_fd, CC_DEVICE_STATUS_EV, enable);

            // pack data and send reply
            json_t *data = json_pack(CC_DEV_STATUS_REPLY_FORMAT);
            send_reply(client_fd, request, data);
        }
        else if (strcmp(request, "assignment") == 0)
        {
            cc_assignment_t assignment;
            double value, min, max, def;
            json_t *options;

            json_unpack(data, CC_ASSIGNMENT_REQ_FORMAT,
                "device_id", &assignment.device_id,
                "actuator_id", &assignment.actuator_id,
                "label", &assignment.label,
                "value", &value,
                "min", &min,
                "max", &max,
                "def", &def,
                "mode", &assignment.mode,
                "steps", &assignment.steps,
                "unit", &assignment.unit,
                "options", &options);

            // double to float
            assignment.value = value;
            assignment.min = min;
            assignment.max = max;
            assignment.def = def;

            // options list
            assignment.list_count = json_array_size(options);
            assignment.list_items = 0;
            if (assignment.list_count > 0)
            {
                assignment.list_items = malloc(assignment.list_count * sizeof(cc_item_t *));

                for (int i = 0; i < assignment.list_count; i++)
                {
                    cc_item_t *item = calloc(1, sizeof(cc_item_t));
                    assignment.list_items[i] = item;

                    const char *key;
                    json_t *value;

                    json_object_foreach(json_array_get(options, i), key, value)
                    {
                        item->label = strndup(key, 16);
                        item->value = json_real_value(value);
                    }
                }
            }

            int assignment_id, assignment_pair_id, actuator_pair_id;
            cc_device_t *device = cc_device_get(assignment.device_id);

            //get the page of the actuator to assign
            uint8_t actuators_in_page = device->actuators_count + device->actuatorgroups_count;
            assignment.actuator_page_id = (assignment.actuator_id / actuators_in_page) + 1;

            // special handling if assigning to group
            if (device && ((assignment.actuator_id - (actuators_in_page * (assignment.actuator_page_id-1))) >= device->actuators_count))
            {
                cc_actuatorgroup_t *actuatorgroup = device->actuatorgroups[assignment.actuator_id - (actuators_in_page * (assignment.actuator_page_id-1)) - device->actuators_count];
                //actuator 0 in a group always becomes the "master" actuator
                actuator_pair_id = actuatorgroup->actuators_in_actuatorgroup[1];

                int actuator_id = actuatorgroup->actuators_in_actuatorgroup[0] + (actuators_in_page * (assignment.actuator_page_id-1));
                int actuator_pair_id = actuatorgroup->actuators_in_actuatorgroup[1] + (actuators_in_page * (assignment.actuator_page_id-1));

                // real assignment
                assignment.actuator_id = actuator_id;
                assignment.actuator_pair_id = actuator_pair_id;
                assignment.assignment_pair_id = -1;
                assignment.mode |= CC_MODE_GROUP|CC_MODE_REVERSE;
                assignment_id = cc_assignment(handle, &assignment, 1);

                // paired assignment
                assignment.actuator_id = actuator_pair_id;
                assignment.actuator_pair_id = actuator_id;
                assignment.assignment_pair_id = assignment_id;
                assignment.mode &= ~CC_MODE_REVERSE;
                assignment_pair_id = cc_assignment(handle, &assignment, 1);

                cc_assignment_key_t key;
                key.id = assignment_id;
                key.pair_id = assignment_pair_id;
                key.device_id = assignment.device_id;
                cc_assignment_set_pair_id(&key);
            }
            else
            {
                assignment.assignment_pair_id = -1;
                assignment_id = cc_assignment(handle, &assignment, 1);
                assignment_pair_id = -1;
                actuator_pair_id = -1;
            }

            // pack data and send reply
            json_t *data = json_pack(CC_ASSIGNMENT_REPLY_FORMAT,
                "assignment_id", assignment_id,
                "assignment_pair_id", assignment_pair_id,
                "actuator_pair_id", actuator_pair_id);
            send_reply(client_fd, request, data);

            // free memory
            for (int i = 0; i < assignment.list_count; i++)
                free(assignment.list_items[i]);

            free(assignment.list_items);
        }
        else if (strcmp(request, "unassignment") == 0)
        {
            cc_assignment_key_t assignment;

            json_unpack(data, CC_UNASSIGNMENT_REQ_FORMAT,
                "assignment_id", &assignment.id,
                "assignment_pair_id", &assignment.pair_id,
                "device_id", &assignment.device_id);

            cc_unassignment(handle, &assignment);

            // pack data and send reply
            json_t *data = json_pack(CC_UNASSIGNMENT_REPLY_FORMAT);
            send_reply(client_fd, request, data);
        }
        else if (strcmp(request, "value_set") == 0)
        {
            cc_set_value_t update;
            double value;

            json_unpack(data, CC_VALUE_SET_REQ_FORMAT,
                "device_id", &update.device_id,
                "actuator_id", &update.actuator_id,
                "assignment_id", &update.assignment_id,
                "value", &value);

            // double to float
            update.value = value;

            cc_value_set(handle, &update);

            // pack data and send reply
            json_t *data = json_pack(CC_VALUE_SET_REPLY_FORMAT);
            send_reply(client_fd, request, data);

        }
        else if (strcmp(request, "data_update") == 0)
        {
            int enable;
            json_unpack(data, CC_DATA_UPDATE_REQ_FORMAT, "enable", &enable);

            event_set(read_data.client_fd, CC_DATA_UPDATE_EV, enable);

            // pack data and send reply
            json_t *data = json_pack(CC_DATA_UPDATE_REPLY_FORMAT);
            send_reply(client_fd, request, data);
        }

        json_decref(root);
    }

    cc_finish(handle);
    sockser_finish(g_server);
    closelog();

    return 0;
}
