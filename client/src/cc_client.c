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
#include <pthread.h>
#include <semaphore.h>
#include <jansson.h>

#include "cc_client.h"
#include "request.h"
#include "sockcli.h"
#include "base64.h"


/*
****************************************************************************************************
*       INTERNAL MACROS
****************************************************************************************************
*/

// buffer size in kB
#define READ_BUFFER_SIZE    128*1024


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

typedef struct cc_client_t {
    sockcli_t *socket;
    char *buffer;
    void (*device_status_cb)(void *arg);
    void (*data_update_cb)(void *arg);
    pthread_t read_thread;
    sem_t waiting_reply;
    pthread_mutex_t request_lock;
} cc_client_t;


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

static void *reader(void *arg)
{
    cc_client_t *client = arg;
    char buffer[READ_BUFFER_SIZE];
    uint8_t raw_data[READ_BUFFER_SIZE];

    while (1)
    {
        int n = sockcli_read(client->socket, buffer, READ_BUFFER_SIZE);

        // peer has gracefully disconnected
        if (n == 0)
            continue;

        // error
        if (n < 0)
            break;

        // check if is an event
        if (strstr(buffer, "\"event\""))
        {
            json_error_t error;
            json_t *root = json_loads(buffer, 0, &error);
            json_t *event = json_object_get(root, "event");

            if (event)
            {
                const char *event_name = json_string_value(event);
                json_t *data = json_object_get(root, "data");

                if (strcmp(event_name, "device_status") == 0)
                {
                    cc_device_t device;

                    // unpack json
                    json_unpack(data, CC_DEV_STATUS_EVENT_FORMAT,
                        "device_id", &device.id,
                        "status", &device.status);

                    // device status callback
                    if (client->device_status_cb)
                        client->device_status_cb(&device);
                }
                else if (strcmp(event_name, "data_update") == 0)
                {
                    int device_id;
                    const char *encode;

                    // unpack json
                    json_unpack(data, CC_DATA_UPDATE_EVENT_FORMAT,
                        "device_id", &device_id,
                        "raw_data", &encode);

                    // decode data
                    base64_decode(encode, strlen(encode), raw_data);

                    // update list callback
                    cc_update_list_t *updates = cc_update_parse(device_id, raw_data);
                    if (client->data_update_cb)
                        client->data_update_cb(updates);

                     cc_update_free(updates);
                }

                json_decref(root);
                continue;
            }
        }

        memcpy(client->buffer, buffer, n);
        sem_post(&client->waiting_reply);
    }

    return 0;
}

static json_t* cc_client_request(cc_client_t *client, const char *name, json_t *data)
{
    // build request: {"request":"name", "data": {...}}
    json_t *root = json_object();
    json_object_set_new(root, "request", json_string(name));
    json_object_set(root, "data", data);

    // dump json to string
    char *request_str = json_dumps(root, 0);

    // lock to request
    pthread_mutex_lock(&client->request_lock);

    json_t *reply = NULL;

    // send request and wait for reply
    int ret = sockcli_write(client->socket, request_str, strlen(request_str) + 1);
    if (ret > 0 && sem_wait(&client->waiting_reply) == 0)
    {
        json_error_t error;
        reply = json_loads(client->buffer, 0, &error);
    }

    // unlock
    pthread_mutex_unlock(&client->request_lock);

    // free memory
    json_decref(root);
    json_decref(data);
    free(request_str);

    return reply;
}


/*
****************************************************************************************************
*       GLOBAL FUNCTIONS
****************************************************************************************************
*/

cc_client_t *cc_client_new(const char *path)
{
    cc_client_t *client = malloc(sizeof(cc_client_t));
    client->socket = sockcli_init(path);

    if (!client->socket)
    {
        free(client);
        return NULL;
    }

    // allocate read buffer
    client->buffer = malloc(READ_BUFFER_SIZE);

    // semaphore and mutex
    sem_init(&client->waiting_reply, 0, 0);
    pthread_mutex_init(&client->request_lock, NULL);

    // set thread attributes
    pthread_attr_t attributes;
    pthread_attr_init(&attributes);
    pthread_attr_setinheritsched(&attributes, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setscope(&attributes, PTHREAD_SCOPE_PROCESS);

    // create thread
    pthread_create(&client->read_thread, &attributes, reader, (void *)client);
    pthread_attr_destroy(&attributes);

    return client;
}

void cc_client_delete(cc_client_t *client)
{
    pthread_cancel(client->read_thread);
    pthread_join(client->read_thread, NULL);
    sockcli_finish(client->socket);
    pthread_mutex_destroy(&client->request_lock);
    sem_destroy(&client->waiting_reply);
    free(client->buffer);
    free(client);
}

int cc_client_assignment(cc_client_t *client, cc_assignment_t *assignment)
{
    json_t *request_data = json_pack(CC_ASSIGNMENT_REQ_FORMAT,
        "device_id", assignment->device_id,
        "actuator_id", assignment->actuator_id,
        "label", assignment->label,
        "value", assignment->value,
        "min", assignment->min,
        "max", assignment->max,
        "def", assignment->def,
        "mode", assignment->mode,
        "steps", assignment->steps,
        "unit", assignment->unit);

    json_t *root = cc_client_request(client, "assignment", request_data);
    if (root)
    {
        json_t *data = json_object_get(root, "data");

        // unpack reply
        int assignment_id;
        json_unpack(data, CC_ASSIGNMENT_REPLY_FORMAT, "assignment_id", &assignment_id);
        json_decref(root);

        return assignment_id;
    }

    return -1;
}

void cc_client_unassignment(cc_client_t *client, cc_assignment_key_t *assignment)
{
    json_t *request_data = json_pack(CC_UNASSIGNMENT_REQ_FORMAT,
        "id", assignment->id,
        "device_id", assignment->device_id);

    json_t *root = cc_client_request(client, "unassignment", request_data);
    if (root)
    {
        // reply is null

        json_decref(root);
    }
}

int* cc_client_device_list(cc_client_t *client)
{
    json_t *request_data = json_pack(CC_DEV_LIST_REQ_FORMAT);

    json_t *root = cc_client_request(client, "device_list", request_data);
    if (root)
    {
        json_t *array = json_object_get(root, "data");

        // create list
        int i, count = json_array_size(array);
        int *device_list = malloc((count + 1) * sizeof(int));
        for (i = 0; i < count; i++)
        {
            json_t *value = json_array_get(array, i);
            device_list[i] = json_integer_value(value);
        }
        device_list[i] = 0;

        json_decref(root);
        return device_list;
    }

    return 0;
}

char *cc_client_device_descriptor(cc_client_t *client, int device_id)
{
    json_t *request_data = json_pack(CC_DEV_DESCRIPTOR_REQ_FORMAT,
        "device_id", device_id);

    json_t *root = cc_client_request(client, "device_descriptor", request_data);
    if (root)
    {
        json_t *data = json_object_get(root, "data");
        char *descriptor = json_dumps(data, 0);
        json_decref(root);
        return descriptor;
    }

    return 0;
}

void cc_client_device_disable(cc_client_t *client, int device_id)
{
    int enable = 0;
    json_t *request_data = json_pack(CC_DEV_CONTROL_REQ_FORMAT, "device_id", device_id,
        "enable", enable);

    json_t *root = cc_client_request(client, "device_control", request_data);
    if (root)
    {
        // reply is null

        json_decref(root);
    }
}

void cc_client_device_status_cb(cc_client_t *client, void (*callback)(void *arg))
{
    client->device_status_cb = callback;

    int enable = callback ? 1 : 0;
    json_t *request_data = json_pack(CC_DEV_STATUS_REQ_FORMAT, "enable", enable);

    json_t *root = cc_client_request(client, "device_status", request_data);
    if (root)
    {
        // reply is null

        json_decref(root);
    }
}

void cc_client_data_update_cb(cc_client_t *client, void (*callback)(void *arg))
{
    client->data_update_cb = callback;

    int enable = callback ? 1 : 0;
    json_t *request_data = json_pack(CC_DATA_UPDATE_REQ_FORMAT, "enable", enable);

    json_t *root = cc_client_request(client, "data_update", request_data);
    if (root)
    {
        // reply is null

        json_decref(root);
    }
}
