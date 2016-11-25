/*
 * Copyright (c) 2016 Ricardo Crudo <ricardo.crudo@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
****************************************************************************************************
*       INCLUDE FILES
****************************************************************************************************
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "sockser.h"
#include "loribu.h"


/*
****************************************************************************************************
*       INTERNAL MACROS
****************************************************************************************************
*/

#define READ_BUFFER_SIZE    100*1024
#define MAX_CLIENTS         10


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

typedef struct client_t {
    int conn_fd, index;
    pthread_t read_thread;
    sockser_t *server;
} client_t;

typedef struct sockser_t {
    int sock_fd;
    pthread_t conn_thread;
    pthread_mutex_t rb_mutex;
    loribu_t *rb;
    sem_t receive_data;
    void (*client_event_cb)(void *arg);
    client_t *clients[MAX_CLIENTS];
} sockser_t;


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

static void* process_client(void *arg)
{
    client_t *client = arg;
    sockser_t *server = client->server;

    uint8_t buffer[READ_BUFFER_SIZE];
    bzero(buffer, READ_BUFFER_SIZE);

    while (1)
    {
        // read socket
        int n = recv(client->conn_fd, buffer, sizeof(buffer), 0);
        if (n > 0)
        {
            pthread_mutex_lock(&server->rb_mutex);
            loribu_write(server->rb, (uint8_t *) &client->conn_fd, sizeof(int));
            loribu_write(server->rb, (uint8_t *) &n, sizeof(int));
            loribu_write(server->rb, buffer, n);
            pthread_mutex_unlock(&server->rb_mutex);
            sem_post(&server->receive_data);
            bzero(buffer, n);
        }
        else if (n < 0)
        {
            perror("ERROR reading from socket");
            return NULL;
        }
        else if (n == 0)
        {
            // connection closed
            break;
        }
    }

    // raise event
    if (server->client_event_cb)
    {
        sockser_event_t event;
        event.id = SOCKSER_CLIENT_DISCONNECTED;
        event.client_fd = client->conn_fd;
        server->client_event_cb(&event);
    }

    // set index negative to indicate thread is finished
    client->index = -1;

    return NULL;
}

static void* new_connections(void *arg)
{
    sockser_t *server = arg;
    struct sockaddr_un remote;
    socklen_t len = sizeof(remote);

    while (1)
    {
        int conn_fd = accept(server->sock_fd, (struct sockaddr *) &remote, &len);
        if (conn_fd < 0)
        {
            perror("ERROR on accept");
            exit(-1);
        }

        // allocate memory for the new client
        int index;
        client_t *client = 0;
        for (index = 0; index < MAX_CLIENTS; index++)
        {
            client = server->clients[index];
            if (!client)
            {
                client = calloc(1, sizeof(client_t));
                server->clients[index] = client;
                break;
            }
            else if (client->index == -1)
            {
                pthread_join(client->read_thread, NULL);
                break;
            }
        }

        // there is no space for more clients
        if (!client)
            continue;

        // fill up client information
        client->conn_fd = conn_fd;
        client->index = index;
        client->server = server;

        // thread attributes
        pthread_attr_t attributes;
        pthread_attr_init(&attributes);
        pthread_attr_setinheritsched(&attributes, PTHREAD_EXPLICIT_SCHED);
        pthread_attr_setscope(&attributes, PTHREAD_SCOPE_PROCESS);

        // create thread to new connected client
        pthread_create(&client->read_thread, &attributes, process_client, client);
        pthread_attr_destroy(&attributes);

        // raise event
        if (server->client_event_cb)
        {
            sockser_event_t event;
            event.id = SOCKSER_CLIENT_CONNECTED;
            event.client_fd = conn_fd;
            server->client_event_cb(&event);
        }
    }

    return NULL;
}


/*
****************************************************************************************************
*       GLOBAL FUNCTIONS
****************************************************************************************************
*/

sockser_t* sockser_init(const char *path)
{
    sockser_t *server = calloc(1, sizeof(sockser_t));

    // create socket endpoint
    server->sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server->sock_fd < 0)
    {
        perror("ERROR opening socket");
        return 0;
    }

    // configure address
    struct sockaddr_un local;
    strcpy(local.sun_path, path);
    unlink(local.sun_path);
    int len = strlen(local.sun_path) + sizeof(local.sun_family);
    local.sun_family = AF_UNIX;

    // bind host address
    if (bind(server->sock_fd, (struct sockaddr *) &local, len) < 0)
    {
        perror("ERROR on binding");
        return 0;
    }

    // start listening for the clients
    listen(server->sock_fd, 5);

    // init mutex, ring buffer and semaphore
    pthread_mutex_init(&server->rb_mutex, NULL);
    server->rb = loribu_create(LORIBU_CREATE_BUFFER, READ_BUFFER_SIZE);
    sem_init(&server->receive_data, 0, 0);

    // thread attributes
    pthread_attr_t attributes;
    pthread_attr_init(&attributes);
    pthread_attr_setinheritsched(&attributes, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setscope(&attributes, PTHREAD_SCOPE_PROCESS);

    // create thread to accept new connections
    pthread_create(&server->conn_thread, &attributes, new_connections, server);
    pthread_attr_destroy(&attributes);

    return server;
}

void sockser_finish(sockser_t *server)
{
    // cancel clients threads
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (server->clients[i])
        {
            client_t *client = server->clients[i];
            pthread_cancel(client->read_thread);
            pthread_join(client->read_thread, NULL);
            free(client);
        }
    }

    // cancel connections thread
    if (server->conn_thread)
    {
        pthread_cancel(server->conn_thread);
        pthread_join(server->conn_thread, NULL);
    }

    if (server->rb)
        loribu_destroy(server->rb);

    free(server);
}

int sockser_read(sockser_t *server, sockser_data_t *data)
{
    // if buffer is empty lock and wait for more data
    if (loribu_is_empty(server->rb))
        sem_wait(&server->receive_data);

    // read client file descriptor
    loribu_read(server->rb, (uint8_t *) &data->client_fd, sizeof(int));

    // read buffer size
    loribu_read(server->rb, (uint8_t *) &data->size, sizeof(int));

    // read data
    return loribu_read(server->rb, data->buffer, data->size);
}

int sockser_read_string(sockser_t *server, sockser_data_t *data)
{
    // if buffer is empty lock and wait for more data
    if (loribu_is_empty(server->rb))
        sem_wait(&server->receive_data);

    // read client file descriptor
    loribu_read(server->rb, (uint8_t *) &data->client_fd, sizeof(int));

    // read buffer size
    loribu_read(server->rb, (uint8_t *) &data->size, sizeof(int));

    // read data until find '\0'
    return loribu_read_until(server->rb, data->buffer, data->size, 0);
}

int sockser_write(sockser_data_t *data)
{
    return send(data->client_fd, data->buffer, data->size, 0);
}

void sockser_client_event_cb(sockser_t *server, void (*callback)(void *arg))
{
    server->client_event_cb = callback;
}
