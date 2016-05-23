
/*
************************************************************************************************************************
*       INCLUDE FILES
************************************************************************************************************************
*/

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <libserialport.h>

#include "utils.h"
#include "device.h"
#include "control_chain.h"

/*
************************************************************************************************************************
*       INTERNAL MACROS
************************************************************************************************************************
*/

#define CC_SERIAL_BUFFER_SIZE   2048
#define CC_SYNC_BYTE            0xA7
#define CC_HEADER_SIZE          6       // in bytes
#define CC_SYNC_TIMEOUT         500     // in ms
#define CC_HEADER_TIMEOUT       10      // in ms
#define CC_DATA_TIMEOUT         10      // in ms

#define CC_CHAIN_SYNC_INTERVAL  10000   // in us


/*
************************************************************************************************************************
*       INTERNAL CONSTANTS
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*       INTERNAL DATA TYPES
************************************************************************************************************************
*/

// receiver status
enum {WAITING_SYNCING, WAITING_HEADER, WAITING_DATA};

struct cc_handle_t {
    int state;
    struct sp_port *sp;
    uint8_t data_crc;
    void (*recv_callback)(void *arg);
    pthread_t receiver_thread, chain_sync_thread;
    pthread_mutex_t running, sending;
    cc_msg_t *msg;
};


/*
************************************************************************************************************************
*       INTERNAL GLOBAL VARIABLES
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*       INTERNAL FUNCTIONS
************************************************************************************************************************
*/

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

static void parser(cc_handle_t *handle)
{
    cc_msg_t *msg = handle->msg;
    int id;

    switch (msg->command)
    {
        case CC_CMD_HANDSHAKE:
            id = cc_device_handshake();
            if (id >= 0)
            {
                msg->dev_address = id;
                cc_send(handle, msg);
            }
            break;

        case CC_CMD_DEV_DESCRIPTOR:
            if (cc_device_add(msg) >= 0)
            {
                msg->data_size = 0;
                cc_send(handle, msg);
            }
            break;
    }
}

static void* receiver(void *arg)
{
    cc_handle_t *handle = (cc_handle_t *) arg;

    uint8_t buffer[CC_HEADER_SIZE];
    enum sp_return ret;

    // create a message object
    cc_msg_t msg;
    msg.data = (uint8_t *) malloc(CC_SERIAL_BUFFER_SIZE);
    if (msg.data == NULL)
        return NULL;

    handle->msg = &msg;

    while (running(handle))
    {
        // waiting sync byte
        if (handle->state == WAITING_SYNCING)
        {
            ret = sp_blocking_read(handle->sp, buffer, 1, CC_SYNC_TIMEOUT);
            if (ret > 0)
            {
                if (buffer[0] == CC_SYNC_BYTE)
                    handle->state = WAITING_HEADER;
            }

            // take the shortcut
            continue;
        }

        // waiting header
        else if(handle->state == WAITING_HEADER)
        {
            ret = sp_blocking_read(handle->sp, buffer, CC_HEADER_SIZE, CC_HEADER_TIMEOUT);
            if (ret == CC_HEADER_SIZE)
            {
                // verify header checksum
                uint8_t crc = buffer[CC_HEADER_SIZE-1];
                if (crc8(buffer, CC_HEADER_SIZE-1) == crc)
                {
                    handle->state = WAITING_DATA;
                    msg.dev_address = buffer[0];
                    msg.command = buffer[1];
                    msg.data_size = buffer[3];
                    msg.data_size <<= 8;
                    msg.data_size |= buffer[2];
                    handle->data_crc = buffer[4];

                    if (msg.data_size == 0)
                        handle->data_crc = 0;
                }
                else
                {
                    handle->state = WAITING_SYNCING;
                }
            }
            else
            {
                handle->state = WAITING_SYNCING;
            }
        }

        // waiting data
        else if (handle->state == WAITING_DATA)
        {
            ret = sp_blocking_read(handle->sp, msg.data, msg.data_size, CC_DATA_TIMEOUT);
            if (ret == msg.data_size)
            {
                // verify data checksum
                if (crc8(msg.data, msg.data_size) == handle->data_crc)
                {
                    parser(handle);
                }
            }

            // always go back to initial state regardless it got data or not
            handle->state = WAITING_SYNCING;
        }
    }

    free(msg.data);

    return NULL;
}

static void* chain_sync(void *arg)
{
    cc_handle_t *handle = (cc_handle_t *) arg;

    cc_msg_t chain_sync_msg;
    chain_sync_msg.dev_address = 0;
    chain_sync_msg.command = CC_CMD_CHAIN_SYNC;
    chain_sync_msg.data_size = 0;
    chain_sync_msg.data = 0;

    while (running(handle))
    {
        // take your time
        usleep(CC_CHAIN_SYNC_INTERVAL);

        // send chain_sync message
        cc_send(handle, &chain_sync_msg);
    }

    return NULL;
}


/*
************************************************************************************************************************
*       GLOBAL FUNCTIONS
************************************************************************************************************************
*/

cc_handle_t* cc_init(const char *port_name, int baudrate)
{
    cc_handle_t *handle = (cc_handle_t *) malloc(sizeof (cc_handle_t));

    if ( handle == NULL)
        return NULL;

    // init handle with null data
    memset(handle, 0, sizeof (cc_handle_t));

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

    // create mutexes
    pthread_mutex_init(&handle->sending, NULL);
    pthread_mutex_init(&handle->running, NULL);
    pthread_mutex_lock(&handle->running);

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

        free(handle);
    }
}

void cc_set_recv_callback(cc_handle_t *handle, void (*callback)(void *arg))
{
    if (handle)
    {
        handle->recv_callback = callback;
    }
}

void cc_send(cc_handle_t *handle, cc_msg_t *msg)
{
    const uint8_t sync_byte = CC_SYNC_BYTE;
    uint8_t buffer[CC_HEADER_SIZE];

    if (handle)
    {
        buffer[0] = msg->dev_address;
        buffer[1] = msg->command;
        buffer[2] = (msg->data_size >> 0) & 0xFF;
        buffer[3] = (msg->data_size >> 8) & 0xFF;
        buffer[4] = crc8(msg->data, msg->data_size);
        buffer[5] = crc8(buffer, CC_HEADER_SIZE-1);

        while (pthread_mutex_trylock(&handle->sending) == EBUSY);
        sp_nonblocking_write(handle->sp, &sync_byte, 1);
        sp_nonblocking_write(handle->sp, buffer, CC_HEADER_SIZE);
        sp_nonblocking_write(handle->sp, msg->data, msg->data_size);
        pthread_mutex_unlock(&handle->sending);
    }
}
