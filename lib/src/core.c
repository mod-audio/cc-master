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

#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <libserialport.h>

#include "control_chain.h"
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

#define CC_SYNC_BYTE            0xA7
#define CC_SYNC_TIMEOUT         500     // in ms
#define CC_HEADER_TIMEOUT       10      // in ms
#define CC_DATA_TIMEOUT         1000    // in ms

#define CC_CHAIN_SYNC_INTERVAL  10000   // in us
#define CC_RESPONSE_TIMEOUT     100     // in ms

#define CC_REQUESTS_PERIOD      2       // in sync cycles
#define CC_HANDSHAKE_PERIOD     20      // in sync cycles
#define CC_DEVICE_TIMEOUT       100     // in sync cycles

// debug macro
#define DEBUG_MSG(...)      do { if (g_debug) fprintf(stderr, "[cc-lib] " __VA_ARGS__); } while (0)


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
    int baudrate, serial_enabled;
    struct sp_port *sp;
    void (*data_update_cb)(void *arg);
    void (*device_status_cb)(void *arg);
    pthread_t receiver_thread, chain_sync_thread;
    pthread_mutex_t running, sending;
    sem_t waiting_response;
    pthread_mutex_t request_lock;
    pthread_cond_t request_cond;
    atomic_bool request_sync;
    cc_msg_t *msg_rx;
};


/*
****************************************************************************************************
*       INTERNAL GLOBAL VARIABLES
****************************************************************************************************
*/

static int g_debug;


/*
****************************************************************************************************
*       INTERNAL FUNCTIONS
****************************************************************************************************
*/

static int serial_setup(cc_handle_t *handle)
{
    // wait until path show up
    struct stat stbuf;
    while (1)
    {
        if (lstat(handle->port_name, &stbuf))
        {
            // check for "no such file or directory" error
            if (errno == ENOENT)
                sleep(1);
            else
                return -1;
        }
        else
            break;
    }

    // set default port path
    char port_path[64];
    strcpy(port_path, handle->port_name);

    // check for symbolic link
    if (S_ISLNK(stbuf.st_mode))
    {
        // try to access file
        for (int i = 0; i < 10; i++)
        {
            // read link and add its content after "/dev/"
            int len = readlink(handle->port_name, &port_path[5], sizeof(port_path)-5);
            port_path[len+5] = 0;

            // check for "permission denied" error
            // after the usb-serial is plugged in, the system might take
            // some time until it set the correct permissions to the file
            if (access(port_path, R_OK | W_OK))
            {
                if (errno == EACCES)
                    usleep(100000);
                else
                    return -1;
            }
            else
            {
                // user has access to the file
                break;
            }
        }
    }

    enum sp_return ret;

    // get serial port
    ret = sp_get_port_by_name(port_path, &handle->sp);
    if (ret != SP_OK)
        return ret;

    // open serial port
    ret = sp_open(handle->sp, SP_MODE_READ_WRITE);
    if (ret != SP_OK)
        return ret;

    // disable XON/XOFF flow control
    sp_set_xon_xoff(handle->sp, SP_XONXOFF_DISABLED);

    // configure serial port
    sp_set_baudrate(handle->sp, handle->baudrate);

    // sleeps some seconds if it's an Arduino connected (initialization)
    char *manufacturer = sp_get_port_usb_manufacturer(handle->sp);
    if (manufacturer && strstr(manufacturer, "Arduino"))
        sleep(3);

    handle->serial_enabled = 1;

    return 0;
}

static void send(cc_handle_t *handle, const cc_msg_t *msg)
{
    if (handle && msg && handle->serial_enabled)
    {
        uint8_t buffer[CC_DATA_BUFFER_SIZE];

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
        int ret = sp_nonblocking_write(handle->sp, buffer, i);

        // check for input/output error
        // this error may happen when the serial file descriptor isn't
        // valid anymore (e.g.: usb-serial device was been removed)
        if (ret == SP_ERR_FAIL && errno == EIO)
        {
            handle->serial_enabled = 0;
            sp_close(handle->sp);
            sp_free_port(handle->sp);
        }

        pthread_mutex_unlock(&handle->sending);

        // print message if debug is enabled
        cc_msg_print("SEND", msg);
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
    for (;;)
    {
        // TESTING DEBUG
        static const char *commands[] = {
            "sync", "handshake", "device control", "device descriptor",
            "assignment", "data update", "unassignment", "set value", "update list items", "request control page"
        };

        if (sem_timedwait(&handle->waiting_response, &timeout) == 0)
        {
            DEBUG_MSG("timedwait ok! %s\n", commands[msg->command]);
            return 0;
        }

        int e = errno;
        DEBUG_MSG("timedwait error %d for %s\n", e, commands[msg->command]);

        if (e != EINTR)
            return 1;
    }
}

static int request(cc_handle_t *handle, const cc_msg_t *msg)
{
    // lock and wait for request cycle
    pthread_mutex_lock(&handle->request_lock);
    while (!atomic_load(&handle->request_sync))
        pthread_cond_wait(&handle->request_cond, &handle->request_lock);

    // send message, only wait if a reply is expected
    int ret;
    switch (msg->command)
    {
    case CC_CMD_DEV_DESCRIPTOR:
        ret = send_and_wait(handle, msg);
        break;
    default:
        send(handle, msg);
        ret = 0;
        break;
    }

    // unlock for next request
    atomic_store(&handle->request_sync, false);
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

#define UPDATE_DATA_SIZE (sizeof(float) + 1)

static void parse_data_update(cc_handle_t *handle)
{
    const cc_msg_t *msg = handle->msg_rx;

    // parse message to update list
    cc_update_list_t *updates;
    cc_msg_parser(msg, &updates);

    DEBUG_MSG("updates received (device_id: %i, count: %i)\n", updates->device_id, updates->count);

    for (int i = 0; i < updates->count; i++)
    {
        cc_assignment_key_t assignment_key = {
            .id = updates->list[i].assignment_id,
            .device_id = updates->device_id,
            .pair_id = -1,
        };

        cc_assignment_t *assignment = cc_assignment_get(&assignment_key);

        if (!assignment)
        {
            DEBUG_MSG("updates for device_id %i, assignment_id %i is invalid\n", updates->device_id, updates->list[i].assignment_id);
            continue;
        }

        // change value
        assignment->value = updates->list[i].value;

        // also change value of paired assignment
        cc_assignment_t *pair_assignment = NULL;

        cc_assignment_key_t assignment_pair_key = {
            .id = assignment->assignment_pair_id,
            .device_id = assignment->device_id,
            .pair_id = -1,
        };

        if (assignment_pair_key.id != -1)
        {
            pair_assignment = cc_assignment_get(&assignment_pair_key);

            if (pair_assignment)
                pair_assignment->value = assignment->value;
        }

        // we need to update list assignments
        if ((assignment->mode & CC_MODE_OPTIONS) && assignment->enumeration_frame_max)
        {
            DEBUG_MSG("sending list update for assignment: %i %i\n", assignment->id, assignment->assignment_pair_id);

            cc_assignment_update_list(assignment, assignment->value);

            cc_msg_t *msg_enum = cc_msg_builder(updates->device_id, CC_CMD_UPDATE_ENUMERATION, assignment);
            request(handle, msg_enum);
            cc_msg_delete(msg_enum);

            if (pair_assignment)
            {
                cc_assignment_update_list(pair_assignment, pair_assignment->value);

                cc_msg_t *msg_enum_r = cc_msg_builder(updates->device_id, CC_CMD_UPDATE_ENUMERATION, pair_assignment);
                request(handle, msg_enum_r);
                cc_msg_delete(msg_enum_r);
            }
        }
    }

    if (updates->count > 0 && handle->data_update_cb)
        handle->data_update_cb(updates);

    cc_update_free(updates);
}

static void parser(cc_handle_t *handle)
{
    cc_msg_t *msg = handle->msg_rx;

    cc_msg_print("RECV", msg);

    // reset device timeout
    for (int i = 0; i < CC_MAX_DEVICES; i++)
    {
        cc_device_t *devices = cc_device_get(i);
        if (devices)
            devices->timeout = 0;
    }

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
            if (device)
                response.device_id = device->id;
        }

        DEBUG_MSG("handshake received\n");
        DEBUG_MSG("  random id: %i\n", handshake.random_id);
        DEBUG_MSG("  protocol: v%i.%i\n", handshake.protocol.major, handshake.protocol.minor);
        DEBUG_MSG("  firmware: v%i.%i.%i\n",
            handshake.protocol.major, handshake.protocol.minor, handshake.protocol.micro);

        // create and send response message
        cc_msg_t *reply = cc_msg_builder(0, CC_CMD_HANDSHAKE, &response);
        send(handle, reply);
        cc_msg_delete(reply);
    }
    else if (msg->command == CC_CMD_DEV_DESCRIPTOR)
    {
        cc_device_t *device = cc_device_get(msg->device_id);
        if (device)
        {
            // parse message to device data
            cc_msg_parser(msg, device);

            DEBUG_MSG("device descriptor received\n");
            DEBUG_MSG("  id: %i, uri: %s\n", device->id, device->uri->text);
            DEBUG_MSG("  label: %s\n", device->label ? device->label->text : "null");
            DEBUG_MSG("  channel: %i\n", device->channel);
            DEBUG_MSG("  actuators count: %i\n", device->actuators_count);
            DEBUG_MSG("  actuatorgroups count: %i\n", device->actuatorgroups_count);
            DEBUG_MSG("  enumeration frame size: %i\n", device->enumeration_frame_item_count);
            DEBUG_MSG("  actuator pages: %i\n", device->amount_of_pages);
            DEBUG_MSG("  CC Chain ID: %i\n", device->chain_id);

            // device is ready to operate
            device->status = CC_DEVICE_CONNECTED;

            DEBUG_MSG("  sending response\n");

            // inform device that the device descriptor was received
            uint8_t dev_desc_msg_data = CC_DEVICE_DESC_ACK;
            cc_msg_t dev_desc_msg = {
                .device_id = device->id,
                .command = CC_CMD_DEV_DESCRIPTOR,
                .data_size = sizeof (dev_desc_msg_data),
                .data = &dev_desc_msg_data
            };
            send(handle, &dev_desc_msg);

            // message received and parsed
            sem_post(&handle->waiting_response);

            // proceed to callback if any
            if (handle->device_status_cb)
                handle->device_status_cb(device);
        }
        else
        {
            sem_post(&handle->waiting_response);
        }
    }
    else if (msg->command == CC_CMD_DATA_UPDATE)
    {
        parse_data_update(handle);
    }
    else if (msg->command == CC_CMD_REQUEST_CONTROL_PAGE)
    {
        DEBUG_MSG("  switching device %d control page to %d\n", msg->device_id, msg->data[0]);
        cc_control_page(handle, msg->device_id, msg->data[0] - 1);
    }
}

static void* receiver(void *arg)
{
    cc_handle_t *handle = (cc_handle_t *) arg;
    cc_msg_t *msg = handle->msg_rx;

    enum sp_return ret;

    while (running(handle))
    {
        if (handle->serial_enabled == 0)
            serial_setup(handle);

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
                    msg->command > CC_NUM_COMMANDS ||
                    msg->data_size > CC_DATA_BUFFER_SIZE - CC_MSG_HEADER_SIZE)
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

    uint8_t dev_desc_msg_data = CC_DEVICE_DESC_REQ;
    cc_msg_t dev_desc_msg = {
        .command = CC_CMD_DEV_DESCRIPTOR,
        .data_size = sizeof (dev_desc_msg_data),
        .data = &dev_desc_msg_data
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
                    DEBUG_MSG("device timeout (device id: %i)\n", device->id);

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
            }
            // other requests (assignment, unassignment, ...)
            else
            {
                pthread_mutex_lock(&handle->request_lock);
                atomic_store(&handle->request_sync, true);
                pthread_cond_signal(&handle->request_cond);
                pthread_mutex_unlock(&handle->request_lock);
            }

            free(device_list);
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

    // enable debug prints if required
    if (getenv("LIBCONTROLCHAIN_DEBUG"))
        g_debug = 1;

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

    atomic_init(&handle->request_sync, false);

    pthread_mutex_lock(&handle->running);

    // semaphores
    sem_init(&handle->waiting_response, 0, 0);

    //////// receiver thread setup

    // set thread attributes
    pthread_attr_t attributes;
    pthread_attr_init(&attributes);
    pthread_attr_setinheritsched(&attributes, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setscope(&attributes, PTHREAD_SCOPE_PROCESS);

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

    DEBUG_MSG("control chain started (port: %s, baud rate: %i)\n", port_name, baudrate);

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

        DEBUG_MSG("control chain finished\n");
    }
}

int cc_assignment(cc_handle_t *handle, cc_assignment_t *assignment, bool new_assignment)
{
    cc_device_t *device = cc_device_get(assignment->device_id);

    if (!device)
        return -1;

    if (new_assignment)
    {
        // check if we need to save enumeration stuff
        if ((assignment->mode & CC_MODE_OPTIONS) && device->enumeration_frame_item_count)
            cc_assignment_update_list(assignment, assignment->value);

        // set page id
        assignment->actuator_page_id = assignment->actuator_id / (device->actuators_count + device->actuatorgroups_count);

        // add assignment
        assignment->id = cc_assignment_add(assignment);
    }

    if (assignment->id < 0)
        return -1;

    // enforce initial value for momentary-mode assignments
    if (assignment->mode & CC_MODE_MOMENTARY)
        assignment->value = assignment->mode & CC_MODE_REVERSE ? assignment->max : assignment->min;

    // we only send the actuators of the current page
    if (device->current_page == assignment->actuator_page_id)
    {
        cc_msg_t *msg = cc_msg_builder(assignment->device_id, CC_CMD_ASSIGNMENT, assignment);
        if (request(handle, msg))
        {
            // TODO: if timeout, try at least one more time
            DEBUG_MSG("  assignment timeout (id: %i)\n", assignment->id);
        }
        else
        {
            DEBUG_MSG("  assignment done (id: %i)\n", assignment->id);
        }

        cc_msg_delete(msg);
    }

    return assignment->id;
}

void cc_unassignment(cc_handle_t *handle, cc_assignment_key_t *assignment_key)
{
    cc_device_t *device = cc_device_get(assignment_key->device_id);
    cc_assignment_t *assignment = cc_assignment_get(assignment_key);

    const cc_assignment_key_t assignment_pair_key = {
        assignment_key->pair_id, assignment_key->device_id, -1
    };

    const bool assignment_active = device && assignment && device->current_page == assignment->actuator_page_id;

    int ret = cc_assignment_remove(assignment_key);

    DEBUG_MSG("unassignment received (id: %i, ret: %i)\n", assignment_key->id, ret);

    if (ret < 0)
        return;

    if (assignment_pair_key.id != -1)
    {
        ret = cc_assignment_remove(&assignment_pair_key);

        DEBUG_MSG("group unassignment received (id: %i, ret: %i)\n", assignment_pair_key.id, ret);

        if (ret < 0)
            return;
    }

    if (!assignment_active)
        return;

    DEBUG_MSG("  requesting unassignment to device (id: %i)\n", assignment_key->id);

    // request unassignment
    cc_msg_t *msg = cc_msg_builder(assignment_key->device_id, CC_CMD_UNASSIGNMENT, assignment_key);
    if (request(handle, msg))
    {
        // TODO: unassignment failed. try again?
        DEBUG_MSG("  unassignment timeout (id: %i)\n", assignment_key->id);
    }
    else
    {
        DEBUG_MSG("  unassignment done (id: %i)\n", assignment_key->id);
    }

    cc_msg_delete(msg);

    if (assignment_pair_key.id != -1)
    {
        // request unassignment
        cc_msg_t *msg_unassignment = cc_msg_builder(assignment_pair_key.device_id, CC_CMD_UNASSIGNMENT, &assignment_pair_key);
        if (request(handle, msg_unassignment))
        {
            // TODO: unassignment failed. try again?
            DEBUG_MSG("  unassignment-2 timeout (id: %i)\n", assignment_pair_key.id);
        }
        else
        {
            DEBUG_MSG("  unassignment-2 done (id: %i)\n", assignment_pair_key.id);
        }

        cc_msg_delete(msg_unassignment);
    }
}

int cc_value_set(cc_handle_t *handle, cc_set_value_t *update)
{
    DEBUG_MSG("value_set received (id: %i, value: %f)\n", update->assignment_id, update->value);

    const int id = update->assignment_id;

    cc_device_t *device = cc_device_get(update->device_id);
    cc_assignment_t *assignment = cc_assignment_get_by_actuator(update->device_id, update->actuator_id);

    if (!device || !assignment)
        return id;

    assignment->value = update->value;

    if (device->current_page != assignment->actuator_page_id)
        return id;

    // request assignment
    cc_msg_t *msg = cc_msg_builder(update->device_id, CC_CMD_SET_VALUE, update);

    if (request(handle, msg))
    {
        // TODO: if timeout, try at least one more time
        DEBUG_MSG("  value_set timeout (id: %i)\n", id);

        // remove assignment, not working anyhow
        cc_assignment_key_t key = {update->device_id, id, -1};
        cc_assignment_remove(&key);

        return -1;
    }
    else
    {
        DEBUG_MSG("  value_set done (id: %i)\n", id);
    }

    cc_msg_delete(msg);

    return id;
}

void cc_control_page(cc_handle_t *handle, int device_id, int page)
{
    cc_device_t *device = cc_device_get(device_id);

    if (!device || page < 0 || page > device->amount_of_pages)
        return;

    device->current_page = page;

    const int actuators_page_offset = page * (device->actuators_count + device->actuatorgroups_count);

    for (int i = 0; i < device->actuators_count; i++)
    {
        cc_assignment_t *assignment = cc_assignment_get_by_actuator(device_id, actuators_page_offset + i);

        if (assignment)
            cc_assignment(handle, assignment, false);
    }
}

void cc_device_disable(cc_handle_t *handle, int device_id)
{
    int control = CC_DEVICE_DISABLE;

    DEBUG_MSG("device disable received (device id: %i)\n", device_id);
    DEBUG_MSG("  requesting device to disable (device id: %i)\n", device_id);

    // request device disable
    cc_msg_t *msg = cc_msg_builder(device_id, CC_CMD_DEV_CONTROL, &control);
    if (request(handle, msg))
        DEBUG_MSG("  request timeout (device id: %i)\n", device_id);
    else
        DEBUG_MSG("  request done (device id: %i)\n", device_id);

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
