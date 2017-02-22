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

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <semaphore.h>
#include <libserialport.h>

#ifdef DEBUG
#include <stdio.h>
#endif

#include "core.h"
#include "utils.h"
#include "msg.h"
#include "handshake.h"
#include "device.h"
#include "assignment.h"
#include "update.h"


/*
****************************************************************************************************
*       INTERNAL MACROS
****************************************************************************************************
*/

#define CC_SERIAL_BUFFER_SIZE   2048
#define CC_SYNC_BYTE            0xA7
#define CC_SYNC_TIMEOUT         500     // in ms
#define CC_HEADER_TIMEOUT       1000      // in ms // FIXME: check if timeout is properly working
#define CC_DATA_TIMEOUT         1000      // in ms // FIXME: check if timeout is properly working

#define CC_CHAIN_SYNC_INTERVAL  10000   // in us
#define CC_RESPONSE_TIMEOUT     100     // in ms

#define CC_REQUESTS_PERIOD      2       // in sync cycles
#define CC_HANDSHAKE_PERIOD     50      // in sync cycles
#define CC_DEVICE_TIMEOUT       100     // in sync cycles


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

// receiver status
enum {WAITING_SYNCING, WAITING_HEADER, WAITING_DATA, WAITING_CRC};

// sync message cycles definition
enum {CC_SYNC_SETUP_CYCLE, CC_SYNC_REGULAR_CYCLE, CC_SYNC_HANDSHAKE_CYCLE};

// control chain handle struct
struct cc_handle_t {
    int state;
    const char *port_name;
    int baudrate;
    struct sp_port *sp;
    void (*data_update_cb)(void *arg);
    void (*device_status_cb)(void *arg);
    pthread_t receiver_thread, chain_sync_thread;
    pthread_mutex_t running, sending;
    sem_t waiting_response;
    pthread_mutex_t request_lock;
    pthread_cond_t request_cond;
    int request_sync;
    cc_msg_t *msg_rx;
};


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

static int serial_setup(cc_handle_t *handle)
{
    enum sp_return ret;

    // wait until serial port show up
    while (1)
    {
        ret = sp_get_port_by_name(handle->port_name, &handle->sp);
        if (ret == SP_ERR_ARG && errno == ENOENT)
            sleep(1);
        else if (ret == SP_OK)
            break;
        else
            return ret;
    }

    // open serial port
    ret = sp_open(handle->sp, SP_MODE_READ_WRITE);
    if (ret != SP_OK)
        return ret;

    // configure serial port
    sp_set_baudrate(handle->sp, handle->baudrate);

    // sleeps some seconds if it's an Arduino connected (initialization)
    char *manufacturer = sp_get_port_usb_manufacturer(handle->sp);
    if (manufacturer && strstr(manufacturer, "Arduino"))
        sleep(3);

    return 0;
}

static void send(cc_handle_t *handle, const cc_msg_t *msg)
{
    if (handle && msg)
    {
        uint8_t buffer[CC_SERIAL_BUFFER_SIZE];

        // sync byte
        buffer[0] = CC_SYNC_BYTE;

        // header
        int i = 1;
        buffer[i++] = msg->device_id;
        buffer[i++] = msg->command;
        buffer[i++] = (msg->data_size >> 0) & 0xFF;
        buffer[i++] = (msg->data_size >> 8) & 0xFF;

        // data
        if (msg->data_size > 0)
        {
            memcpy(&buffer[i], msg->data, msg->data_size);
            i += msg->data_size;
        }

        // calculate crc (skip sync byte)
        buffer[i++] = crc8(&buffer[1], CC_MSG_HEADER_SIZE + msg->data_size);

        pthread_mutex_lock(&handle->sending);
        sp_nonblocking_write(handle->sp, buffer, i);
        pthread_mutex_unlock(&handle->sending);

#ifdef DEBUG
        if (msg->command == CC_CMD_CHAIN_SYNC)
            return;

        printf("SEND: device: %i, command: %i\n", msg->device_id, msg->command);
        printf("      data size: %i, data:", msg->data_size);
        for (int i = 0; i < msg->data_size; i++)
            printf(" %02X", msg->data[i]);

        printf("\n---\n");
#endif
    }
}

static int send_and_wait(cc_handle_t *handle, const cc_msg_t *msg)
{
    send(handle, msg);

    // set timeout
    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += ((CC_RESPONSE_TIMEOUT * 1000000) / 1000000000);
    timeout.tv_nsec += ((CC_RESPONSE_TIMEOUT * 1000000) % 1000000000);

    if (timeout.tv_nsec >= 1000000000)
    {
        timeout.tv_sec += 1;
        timeout.tv_nsec -= 1000000000;
    }

    // only one request must be done per time
    // because all devices share the same serial line
    return sem_timedwait(&handle->waiting_response, &timeout);
}

static int request(cc_handle_t *handle, const cc_msg_t *msg)
{
    // lock and wait for request cycle
    pthread_mutex_lock(&handle->request_lock);
    while (handle->request_sync == 0)
        pthread_cond_wait(&handle->request_cond, &handle->request_lock);

    // send message
    int ret = send_and_wait(handle, msg);

    // unlock for next request
    handle->request_sync = 0;
    pthread_mutex_unlock(&handle->request_lock);

    return ret;
}

static int running(cc_handle_t *handle)
{
    switch (pthread_mutex_trylock(&handle->running))
    {
        case 0:
            pthread_mutex_unlock(&handle->running);
            return 0;

        case EBUSY:
            return 1;
    }

    return 0;
}

static void parse_data_update(cc_handle_t *handle)
{
    cc_msg_t *msg = handle->msg_rx;

    // TODO: check if assignment id is valid

    // parse message to update list
    cc_update_list_t *updates;
    cc_msg_parser(msg, &updates);

    if (handle->data_update_cb)
        handle->data_update_cb(updates);

    cc_update_free(updates);
}

static void parser(cc_handle_t *handle)
{
    cc_msg_t *msg = handle->msg_rx;

#ifdef DEBUG
    if (msg->command != CC_CMD_CHAIN_SYNC)
    {
        printf("RECV: device: %i, command: %i\n", msg->device_id, msg->command);
        printf("      data size: %i, data:", msg->data_size);
        for (int i = 0; i < msg->data_size; i++)
            printf(" %02X", msg->data[i]);

        printf("\n---\n");
    }
#endif

    // reset device timeout
    cc_device_t *device = cc_device_get(msg->device_id);
    if (device)
        device->timeout = 0;

    if (msg->command == CC_CMD_HANDSHAKE)
    {
        cc_handshake_dev_t handshake;
        cc_handshake_mod_t response;

        // parse message to handshake data
        cc_msg_parser(msg, &handshake);

        int status = cc_handshake_check(&handshake, &response);
        if (status != CC_UPDATE_REQUIRED)
        {
            // create a new device
            cc_device_t *device = cc_device_create(&handshake);
            response.device_id = device->id;
        }

        // create and send response message
        cc_msg_t *reply = cc_msg_builder(0, CC_CMD_HANDSHAKE, &response);
        send(handle, reply);
        cc_msg_delete(reply);
    }
    else if (msg->command == CC_CMD_DEV_DESCRIPTOR)
    {
        sem_post(&handle->waiting_response);

        cc_device_t *device = cc_device_get(msg->device_id);
        if (device)
        {
            // parse message to device data
            cc_msg_parser(msg, device);

            // set device status
            device->status = CC_DEVICE_CONNECTED;

           // proceed to callback if any
           if (handle->device_status_cb)
               handle->device_status_cb(device);
        }
    }
    else if (msg->command == CC_CMD_ASSIGNMENT ||
             msg->command == CC_CMD_UNASSIGNMENT)
    {
        // TODO: don't post if there is no request
        sem_post(&handle->waiting_response);
    }
    else if (msg->command == CC_CMD_DATA_UPDATE)
    {
        parse_data_update(handle);
    }
}

static void* receiver(void *arg)
{
    cc_handle_t *handle = (cc_handle_t *) arg;
    cc_msg_t *msg = handle->msg_rx;

    enum sp_return ret;

    while (running(handle))
    {
        // waiting sync byte
        if (handle->state == WAITING_SYNCING)
        {
            uint8_t sync;
            ret = sp_blocking_read(handle->sp, &sync, 1, CC_SYNC_TIMEOUT);
            if (ret > 0)
            {
                if (sync == CC_SYNC_BYTE)
                    handle->state = WAITING_HEADER;
            }

            // take the shortcut
            continue;
        }

        // waiting header
        else if(handle->state == WAITING_HEADER)
        {
            ret = sp_blocking_read(handle->sp, msg->header, CC_MSG_HEADER_SIZE, CC_HEADER_TIMEOUT);
            if (ret == CC_MSG_HEADER_SIZE)
            {
                msg->device_id = msg->header[0];
                msg->command = msg->header[1];
                msg->data_size = *((uint16_t *) &msg->header[2]);

                if (msg->device_id > CC_MAX_DEVICES ||
                    msg->command > CC_NUM_COMMANDS)
                    handle->state = WAITING_SYNCING;
                else if (msg->data_size == 0)
                    handle->state = WAITING_CRC;
                else
                    handle->state = WAITING_DATA;
            }
            else
            {
                handle->state = WAITING_SYNCING;
            }
        }

        // waiting data
        else if (handle->state == WAITING_DATA)
        {
            ret = sp_blocking_read(handle->sp, msg->data, msg->data_size, CC_DATA_TIMEOUT);
            if (ret == msg->data_size)
                handle->state = WAITING_CRC;
            else
                handle->state = WAITING_SYNCING;
        }

        // waiting crc
        else if (handle->state == WAITING_CRC)
        {
            uint8_t crc;
            ret = sp_blocking_read(handle->sp, &crc, 1, CC_DATA_TIMEOUT);
            if (ret == 1)
            {
                if (crc8(msg->header, CC_MSG_HEADER_SIZE + msg->data_size) == crc)
                {
                    parser(handle);
                }
            }

            handle->state = WAITING_SYNCING;
        }
    }

    return NULL;
}

static void* chain_sync(void *arg)
{
    cc_handle_t *handle = (cc_handle_t *) arg;

    unsigned int cycles_counter = 0;

    uint8_t chain_sync_msg_data;
    cc_msg_t chain_sync_msg = {
        .device_id = 0,
        .command = CC_CMD_CHAIN_SYNC,
        .data_size = sizeof (chain_sync_msg_data),
        .data = &chain_sync_msg_data
    };

    cc_msg_t dev_desc_msg = {
        .command = CC_CMD_DEV_DESCRIPTOR,
        .data_size = 0,
        .data = 0
    };

    // this is the setup sync cycle message
    // when a device receive this message it must reset to its initial state
    // such message can be seen as a software reset
    chain_sync_msg.data[0] = CC_SYNC_SETUP_CYCLE;
    send(handle, &chain_sync_msg);

    while (running(handle))
    {
        // period between sync messages
        usleep(CC_CHAIN_SYNC_INTERVAL);

        // device timeout checking
        // list only devices in "waiting for request" state
        int *device_list = cc_device_list(CC_DEVICE_LIST_REGISTERED);
        for (int i = 0; device_list[i]; i++)
        {
            cc_device_t *device = cc_device_get(device_list[i]);
            if (device)
            {
                device->timeout++;
                if (device->timeout >= CC_DEVICE_TIMEOUT)
                {
                    device->status = CC_DEVICE_DISCONNECTED;

                   // proceed to callback if any
                   if (handle->device_status_cb)
                       handle->device_status_cb(device);

                    cc_device_destroy(device_list[i]);
                }
            }
        }
        free(device_list);

        cycles_counter++;

        // default sync message is regular cycle
        chain_sync_msg.data[0] = CC_SYNC_REGULAR_CYCLE;

        // handshake cycle
        if ((cycles_counter % CC_HANDSHAKE_PERIOD) == 0)
        {
            chain_sync_msg.data[0] = CC_SYNC_HANDSHAKE_CYCLE;
        }
        // requests cycle
        else if ((cycles_counter % CC_REQUESTS_PERIOD) == 0)
        {
            // device descriptor request
            int *device_list = cc_device_list(CC_DEVICE_LIST_UNREGISTERED);
            if (*device_list)
            {
                for (int i = 0; device_list[i]; i++)
                {
                    dev_desc_msg.device_id = device_list[i];

                    pthread_mutex_lock(&handle->request_lock);

                    // request device descriptor
                    if (send_and_wait(handle, &dev_desc_msg))
                        cc_device_destroy(device_list[i]);

                    pthread_mutex_unlock(&handle->request_lock);
                }
                free(device_list);
            }
            // other requests (assignment, unassignment, ...)
            else
            {
                pthread_mutex_lock(&handle->request_lock);
                handle->request_sync = 1;
                pthread_cond_signal(&handle->request_cond);
                pthread_mutex_unlock(&handle->request_lock);
            }
        }

        // each control chain frame starts with a sync message
        // devices must only send 'data update' messages after receive a sync message
        send(handle, &chain_sync_msg);
    }

    return NULL;
}


/*
****************************************************************************************************
*       GLOBAL FUNCTIONS
****************************************************************************************************
*/

cc_handle_t* cc_init(const char *port_name, int baudrate)
{
    cc_handle_t *handle = (cc_handle_t *) malloc(sizeof (cc_handle_t));

    if (handle == NULL)
        return NULL;

    // init handle with null data
    memset(handle, 0, sizeof (cc_handle_t));

    // create a message object for receiving data
    handle->msg_rx = cc_msg_new();

    // serial setup
    handle->baudrate = baudrate;
    handle->port_name = port_name;
    if (serial_setup(handle))
    {
        cc_finish(handle);
        return NULL;
    }

    // create mutexes
    pthread_mutex_init(&handle->sending, NULL);
    pthread_mutex_init(&handle->running, NULL);
    pthread_mutex_init(&handle->request_lock, NULL);
    pthread_cond_init(&handle->request_cond, NULL);

    pthread_mutex_lock(&handle->running);

    // semaphores
    sem_init(&handle->waiting_response, 0, 0);

    //////// receiver thread setup

    // set thread attributes
    pthread_attr_t attributes;
    pthread_attr_init(&attributes);
    pthread_attr_setinheritsched(&attributes, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setscope(&attributes, PTHREAD_SCOPE_PROCESS);
    //pthread_attr_setschedpolicy(&attributes, SCHED_FIFO);

    // create thread
    int ret_val = pthread_create(&handle->receiver_thread, &attributes, receiver, (void*) handle);

    if (ret_val != 0)
    {
        cc_finish(handle);
        return NULL;
    }

    //////// chain sync thread setup

    // use same attributes as before

    // create thread
    ret_val = pthread_create(&handle->chain_sync_thread, &attributes, chain_sync, (void*) handle);
    pthread_attr_destroy(&attributes);

    if (ret_val != 0)
    {
        cc_finish(handle);
        return NULL;
    }

    return handle;
}

void cc_finish(cc_handle_t *handle)
{
    if (handle)
    {
        // destroy all devices
        int *device_list = cc_device_list(CC_DEVICE_LIST_ALL);
        for (int i = 0; device_list[i]; i++)
            cc_device_destroy(device_list[i]);

        free(device_list);

        pthread_mutex_unlock(&handle->running);

        if (handle->receiver_thread)
            pthread_join(handle->receiver_thread, NULL);

        if (handle->chain_sync_thread)
            pthread_join(handle->chain_sync_thread, NULL);

        if (handle->sp)
        {
            sp_close(handle->sp);
            sp_free_port(handle->sp);
        }

        cc_msg_delete(handle->msg_rx);
        free(handle);
    }
}

int cc_assignment(cc_handle_t *handle, cc_assignment_t *assignment)
{
    int id = cc_assignment_add(assignment);

    if (id < 0)
        return id;

    // request assignment
    cc_msg_t *msg = cc_msg_builder(assignment->device_id, CC_CMD_ASSIGNMENT, assignment);
    if (request(handle, msg))
    {
        // TODO: if timeout, try at least one more time
        cc_unassignment_t unassignment;
        unassignment.device_id = assignment->device_id;
        unassignment.assignment_id = id;
        cc_assignment_remove(&unassignment);
        id = -1;
    }

    cc_msg_delete(msg);

    return id;
}

void cc_unassignment(cc_handle_t *handle, cc_unassignment_t *unassignment)
{
    int ret = cc_assignment_remove(unassignment);

    if (ret < 0)
        return;

    // request unassignment
    cc_msg_t *msg = cc_msg_builder(unassignment->device_id, CC_CMD_UNASSIGNMENT, unassignment);
    request(handle, msg);

    cc_msg_delete(msg);
}

void cc_device_disable(cc_handle_t *handle, int device_id)
{
    int control = CC_DEVICE_DISABLE;

    // request device disable
    cc_msg_t *msg = cc_msg_builder(device_id, CC_CMD_DEV_CONTROL, &control);
    request(handle, msg);

    cc_msg_delete(msg);
}

void cc_data_update_cb(cc_handle_t *handle, void (*callback)(void *arg))
{
    handle->data_update_cb = callback;
}

void cc_device_status_cb(cc_handle_t *handle, void (*callback)(void *arg))
{
    handle->device_status_cb = callback;
}
