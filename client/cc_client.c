/*
****************************************************************************************************
*       INCLUDE FILES
****************************************************************************************************
*/

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>

#include "cc_client.h"
#include "sockcli.h"


/*
****************************************************************************************************
*       INTERNAL MACROS
****************************************************************************************************
*/

// buffer size in kB
#define READ_BUFFER_SIZE    128*1024

// reply timeout in ms
#define REPLY_TIMEOUT       10


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
    uint8_t *buffer;
    void (*device_status_cb)(void *arg);
    void (*data_update_cb)(void *arg);
    pthread_t read_thread;
    sem_t waiting_reply;
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

    while (1)
    {
        int n = sockcli_read(client->socket, buffer, READ_BUFFER_SIZE);
        if (n == 0)
            continue;

        memcpy(client->buffer, buffer, n);

        int request = client->buffer[0];
        uint8_t *data = &client->buffer[1];

        if (request == CC_DATA_UPDATE)
        {
            cc_update_list_t *updates = cc_update_parse(data);
            if (client->data_update_cb)
                client->data_update_cb(updates);

             cc_update_free(updates);
        }

        sem_post(&client->waiting_reply);
    }

    return 0;
}

static uint8_t* wait_reply(cc_client_t *client, int request)
{
    // set timeout
    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += ((REPLY_TIMEOUT * 1000000) / 1000000000);
    timeout.tv_nsec += ((REPLY_TIMEOUT * 1000000) % 1000000000);

    if (timeout.tv_nsec >= 1000000000)
    {
        timeout.tv_sec += 1;
        timeout.tv_nsec -= 1000000000;
    }

    if (sem_timedwait(&client->waiting_reply, &timeout) == 0)
    {
        if (client->buffer[0] == request)
        {
            return &client->buffer[1];
        }
    }

    return 0;
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

    // allocate read buffer
    client->buffer = malloc(READ_BUFFER_SIZE);

    // semaphore
    sem_init(&client->waiting_reply, 0, 0);

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
    sockcli_finish(client->socket);
    free(client->buffer);
    free(client);
}

int cc_client_assignment(cc_client_t *client, cc_assignment_t *assignment)
{
    // send request
    char request = CC_ASSIGNMENT;
    sockcli_write(client->socket, &request, sizeof(request));
    sockcli_write(client->socket, assignment, sizeof(cc_assignment_t));

    uint8_t *data = wait_reply(client, request);
    if (data)
    {
        int id = *((int *)data);
        return id;
    }

    return -1;
}

void cc_client_unassignment(cc_client_t *client, cc_unassignment_t *unassignment)
{
    // send request
    char request = CC_UNASSIGNMENT;
    sockcli_write(client->socket, &request, sizeof(request));
    sockcli_write(client->socket, unassignment, sizeof(cc_unassignment_t));
}

int* cc_client_device_list(cc_client_t *client)
{
    // send request
    char request = CC_DEVICE_LIST;
    sockcli_write(client->socket, &request, sizeof(request));

    uint8_t *data = wait_reply(client, request);
    if (data)
    {
        // number of devices
        int n_devices = *((int *)data);
        data += sizeof(int);

        // list of devices id
        int *device_list = malloc(sizeof(int) * (n_devices + 1));
        for (int i = 0; i < n_devices; i++)
        {
            device_list[i] = *((int *)data);
            data += sizeof(int);
        }

        device_list[n_devices] = 0;

        return device_list;
    }

    return 0;
}

char *cc_client_device_descriptor(cc_client_t *client, int device_id)
{
    // send request
    char request = CC_DEVICE_DESCRIPTOR;
    sockcli_write(client->socket, &request, sizeof(request));
    sockcli_write(client->socket, &device_id, sizeof(int));

    char *data = (char *) wait_reply(client, request);
    if (data)
    {
        char *descriptor = malloc(strlen(data) + 1);
        strcpy(descriptor, data);
        return descriptor;
    }

    return 0;
}

void cc_client_device_status(cc_client_t *client)
{
    // send request
    char request = CC_DEVICE_STATUS;
    sockcli_write(client->socket, &request, sizeof(request));
}

void cc_client_data_update(cc_client_t *client)
{
    // send request
    char request = CC_DATA_UPDATE;
    sockcli_write(client->socket, &request, sizeof(request));
}

void cc_client_device_status_cb(cc_client_t *client, void (*callback)(void *arg))
{
    client->device_status_cb = callback;
}

void cc_client_data_update_cb(cc_client_t *client, void (*callback)(void *arg))
{
    client->data_update_cb = callback;
}
