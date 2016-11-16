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
#define CC_RESPONSE_TIMEOUT     10      // in ms

#define CC_HANDSHAKE_NUM_CYCLES 10


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
enum {CC_SYNC_REGULAR_CYCLE, CC_SYNC_HANDSHAKE_CYCLE};

// control chain handle struct
struct cc_handle_t {
    int state;
    struct sp_port *sp;
    void (*data_update_cb)(void *arg);
    void (*device_status_cb)(void *arg);
    pthread_t receiver_thread, chain_sync_thread;
    pthread_mutex_t running, sending;
    sem_t waiting_response;
    cc_msg_t *msg_rx, *msg_tx;
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

static void send(cc_handle_t *handle, const cc_msg_t *msg)
{
    if (handle && msg)
    {
        uint8_t *buffer = handle->msg_tx->header;

        // header
        int i = 0;
        buffer[i++] = msg->dev_address;
        buffer[i++] = msg->command;
        buffer[i++] = (msg->data_size >> 0) & 0xFF;
        buffer[i++] = (msg->data_size >> 8) & 0xFF;

        // data
        if (msg->data_size > 0)
        {
            if (msg != handle->msg_tx)
            {
                for (int j = 0; j < msg->data_size; j++)
                    buffer[i++] = msg->data[j];
            }
            else
            {
                i += msg->data_size;
            }
        }

        // calculate crc
        buffer[i++] = crc8(buffer, CC_MSG_HEADER_SIZE + msg->data_size);

        const uint8_t sync_byte = CC_SYNC_BYTE;
        pthread_mutex_lock(&handle->sending);
        sp_nonblocking_write(handle->sp, &sync_byte, 1);
        sp_nonblocking_write(handle->sp, buffer, i);
        pthread_mutex_unlock(&handle->sending);

#ifdef DEBUG
        if (msg->command == CC_CMD_CHAIN_SYNC)
            return;

        printf("SEND: device: %i, command: %i\n", msg->dev_address, msg->command);
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

    // parse update list message to struct
    cc_update_list_t *updates = cc_msg_parser(msg);

    if (handle->data_update_cb)
        handle->data_update_cb(updates);

    cc_update_free(updates);
}

static void parser(cc_handle_t *handle)
{
    cc_msg_t *msg = handle->msg_rx;

#ifdef DEBUG
        printf("RECV: device: %i, command: %i\n", msg->dev_address, msg->command);
        printf("      data size: %i, data:", msg->data_size);
        for (int i = 0; i < msg->data_size; i++)
            printf(" %02X", msg->data[i]);

        printf("\n---\n");
#endif

    if (msg->command == CC_CMD_HANDSHAKE)
    {
        cc_handshake_dev_t *handshake = cc_msg_parser(msg);
        cc_handshake_mod_t *response = cc_handshake_check(handshake);

        if (response)
        {
            //TODO: check response status

            // create a new device using the passing the required address
            cc_device_create(response->address);

            // build and send response message
            cc_msg_builder(msg->command, response, handle->msg_tx);
            send(handle, handle->msg_tx);

            //destroy handshake structure
            cc_handshake_destroy(handshake);
        }
    }
    else if (msg->command == CC_CMD_DEV_DESCRIPTOR)
    {
        sem_post(&handle->waiting_response);

        // parse device descriptor message to struct
        cc_dev_descriptor_t *descriptor = cc_msg_parser(msg);

        // store device descriptor
        cc_device_t *device = cc_device_get(msg->dev_address);
        device->descriptor = descriptor;
        device->status = CC_DEVICE_CONNECTED;

        // proceed to callback if any
        if (handle->device_status_cb)
            handle->device_status_cb(device);
    }
    else if (msg->command == CC_CMD_ASSIGNMENT ||
             msg->command == CC_CMD_UNASSIGNMENT)
    {
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
                msg->dev_address = msg->header[0];
                msg->command = msg->header[1];
                msg->data_size = *((uint16_t *) &msg->header[2]);

                if (msg->data_size == 0)
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

    int cycles_counter = 0;

    uint8_t chain_sync_msg_data;
    cc_msg_t chain_sync_msg = {
        .dev_address = 0,
        .command = CC_CMD_CHAIN_SYNC,
        .data_size = sizeof (chain_sync_msg_data),
        .data = &chain_sync_msg_data
    };

    cc_msg_t dev_desc_msg = {
        .command = CC_CMD_DEV_DESCRIPTOR,
        .data_size = 0,
        .data = 0
    };

    while (running(handle))
    {
        // period between sync messages
        usleep(CC_CHAIN_SYNC_INTERVAL);

        // list devices without device descriptor
        int *device_list = cc_device_list(CC_DEVICE_LIST_UNREGISTERED);
        for (int i = 0; device_list[i]; i++)
        {
            dev_desc_msg.dev_address = device_list[i];

            // request device descriptor
            if (send_and_wait(handle, &dev_desc_msg))
            {
                cc_device_destroy(device_list[i]);
            }
        }

        cycles_counter++;

        // define the type of the sync message according the number of cycles
        chain_sync_msg.data[0] = CC_SYNC_REGULAR_CYCLE;
        if (cycles_counter >= CC_HANDSHAKE_NUM_CYCLES)
        {
            chain_sync_msg.data[0] = CC_SYNC_HANDSHAKE_CYCLE;
            cycles_counter = 0;
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

    // create a message objects
    handle->msg_rx = cc_msg_new();
    handle->msg_tx = cc_msg_new();

    //////// serial setup

    // get serial port
    enum sp_return ret;
    ret = sp_get_port_by_name(port_name, &handle->sp);
    if (ret != SP_OK)
    {
        cc_finish(handle);
        return NULL;
    }

    // open serial port
    ret = sp_open(handle->sp, SP_MODE_READ_WRITE);
    if (ret != SP_OK)
    {
        cc_finish(handle);
        return NULL;
    }

    // configure serial port
    sp_set_baudrate(handle->sp, baudrate);

    // if it's an Arduino connected sleeps some seconds for initialization
    char *manufacturer = sp_get_port_usb_manufacturer(handle->sp);
    if (strstr(manufacturer, "Arduino"))
        sleep(3);

    // create mutexes
    pthread_mutex_init(&handle->sending, NULL);
    pthread_mutex_init(&handle->running, NULL);
    pthread_mutex_lock(&handle->running);

    // semaphores
    sem_init(&handle->waiting_response, 0, 1);

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
        {
            cc_device_destroy(device_list[i]);
        }

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
        cc_msg_delete(handle->msg_tx);
        free(handle);
    }
}

int cc_assignment(cc_handle_t *handle, cc_assignment_t *assignment)
{
    int id = cc_assignment_add(assignment);

    // build and send assignment message
    cc_msg_builder(CC_CMD_ASSIGNMENT, assignment, handle->msg_tx);
    if (send_and_wait(handle, handle->msg_tx))
    {
        // TODO: if timeout, try at least one more time
        cc_unassignment_t unassignment;
        unassignment.device_id = assignment->device_id;
        unassignment.assignment_id = id;
        cc_assignment_remove(&unassignment);
        return -1;
    }

    return id;
}

void cc_unassignment(cc_handle_t *handle, cc_unassignment_t *unassignment)
{
    cc_assignment_remove(unassignment);

    // build and send unassignment message
    cc_msg_builder(CC_CMD_UNASSIGNMENT, unassignment, handle->msg_tx);
    send_and_wait(handle, handle->msg_tx);
}

void cc_data_update_cb(cc_handle_t *handle, void (*callback)(void *arg))
{
    handle->data_update_cb = callback;
}

void cc_device_status_cb(cc_handle_t *handle, void (*callback)(void *arg))
{
    handle->device_status_cb = callback;
}


// TODO: timeout to receive device descriptor (release frame of handshake)
// TODO: detect if device has disconnected
