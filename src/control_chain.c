
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
#include <semaphore.h>
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
#define CC_HEADER_SIZE          4       // in bytes
#define CC_SYNC_TIMEOUT         500     // in ms
#define CC_HEADER_TIMEOUT       10      // in ms
#define CC_DATA_TIMEOUT         10      // in ms

#define CC_CHAIN_SYNC_INTERVAL  10000   // in us
#define CC_RESPONSE_TIMEOUT     10      // in ms


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
enum {WAITING_SYNCING, WAITING_HEADER, WAITING_DATA, WAITING_CRC};

typedef struct cc_msg_t {
    uint8_t dev_address;
    uint8_t command;
    uint16_t data_size;
    uint8_t *header, *data;
} cc_msg_t;

struct cc_handle_t {
    int state;
    struct sp_port *sp;
    void (*data_update_cb)(void *arg);
    void (*dev_desc_cb)(void *arg);
    pthread_t receiver_thread, chain_sync_thread;
    pthread_mutex_t running, sending;
    sem_t waiting_response;
    cc_msg_t *msg;
    uint8_t *out_buffer;
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

static void send(cc_handle_t *handle, const cc_msg_t *msg)
{
    const uint8_t sync_byte = CC_SYNC_BYTE;

    if (handle && msg)
    {
        int i = 0;
        uint8_t buffer[CC_HEADER_SIZE];
        buffer[i++] = msg->dev_address;
        buffer[i++] = msg->command;
        buffer[i++] = (msg->data_size >> 0) & 0xFF;
        buffer[i++] = (msg->data_size >> 8) & 0xFF;

        // copy header to output buffer
        memcpy(handle->out_buffer, buffer, i);

        // copy data to output buffer
        if (msg->data_size > 0)
        {
            memcpy(&handle->out_buffer[i], msg->data, msg->data_size);
            i += msg->data_size;
        }

        // append crc
        handle->out_buffer[i] = crc8(handle->out_buffer, i);
        i++;

        pthread_mutex_lock(&handle->sending);
        sp_nonblocking_write(handle->sp, &sync_byte, 1);
        sp_nonblocking_write(handle->sp, handle->out_buffer, i);
        pthread_mutex_unlock(&handle->sending);
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
    cc_msg_t *msg = handle->msg;
    cc_data_t data[256];
    cc_data_update_t update;

    update.count = msg->data[0];
    update.updates_list = data;

    for (int i = 0, j = 1; i < update.count; i++)
    {
        data[i].assignment_id = msg->data[j++];
        memcpy(&data[i].value, &msg->data[j], sizeof (float));
        j += sizeof (float);
    }

    if (handle->data_update_cb)
        handle->data_update_cb(&update);
}

static void parser(cc_handle_t *handle)
{
    cc_msg_t *msg = handle->msg;

    if (msg->command == CC_CMD_HANDSHAKE)
    {
        int id = cc_device_handshake();
        if (id >= 0)
        {
            msg->dev_address = id;
            send(handle, msg);
        }
    }
    else if (msg->command == CC_CMD_DEV_DESCRIPTOR)
    {
        sem_post(&handle->waiting_response);
        cc_dev_descriptor_t *desc = cc_device_add(msg->dev_address, msg->data);
        if (desc && handle->dev_desc_cb)
            handle->dev_desc_cb(desc);
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
    cc_msg_t *msg = handle->msg;

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
            ret = sp_blocking_read(handle->sp, msg->header, CC_HEADER_SIZE, CC_HEADER_TIMEOUT);
            if (ret == CC_HEADER_SIZE)
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
                if (crc8(msg->header, CC_HEADER_SIZE + msg->data_size) == crc)
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

    const cc_msg_t chain_sync_msg = {
        .dev_address = 0,
        .command = CC_CMD_CHAIN_SYNC,
        .data_size = 0,
        .data = 0
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

        // request all missing device descriptors
        int *missing_desc = cc_device_missing_descriptors();
        while(*missing_desc)
        {
            int dev_id = *missing_desc++;
            dev_desc_msg.dev_address = dev_id;

            // request device descriptor
            if (send_and_wait(handle, &dev_desc_msg))
            {
                cc_device_remove(dev_id);
            }
        }

        // each control chain frame starts with a sync message
        // devices must only send 'data update' messages after receive a sync message
        send(handle, &chain_sync_msg);
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

    if (handle == NULL)
        return NULL;

    // init handle with null data
    memset(handle, 0, sizeof (cc_handle_t));

    // create a message object for receiver
    handle->msg = (cc_msg_t *) malloc(sizeof (cc_msg_t));
    handle->msg->header = (uint8_t *) malloc(CC_SERIAL_BUFFER_SIZE);
    handle->msg->data = &handle->msg->header[CC_HEADER_SIZE];

    // allocate memory to output buffer
    handle->out_buffer = (uint8_t *) malloc(CC_SERIAL_BUFFER_SIZE);

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
        cc_device_remove_all();

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

        free(handle->out_buffer);
        free(handle->msg->header);
        free(handle->msg);
        free(handle);
    }
}

int cc_assignment(cc_handle_t *handle, cc_assignment_t *assignment)
{
    uint8_t buffer[CC_SERIAL_BUFFER_SIZE];

    cc_msg_t msg;
    msg.dev_address = assignment->device_id;
    msg.command = CC_CMD_ASSIGNMENT;
    msg.data = buffer;

    int id = cc_assignment_add(assignment, msg.data, &msg.data_size);
    if (id >= 0)
    {
        if (send_and_wait(handle, &msg))
        {
            cc_assignment_remove(id, NULL, NULL);
            return -1;
        }

        return id;
    }

    return -1;
}

void cc_unassignment(cc_handle_t *handle, int assignment_id) // FIXME
{
    uint8_t buffer[CC_SERIAL_BUFFER_SIZE];

    cc_msg_t msg;
    msg.dev_address = assignment_id;
    msg.command = CC_CMD_UNASSIGNMENT;
    msg.data = buffer;

    cc_assignment_remove(assignment_id, msg.data, &msg.data_size);
    send_and_wait(handle, &msg);
}

void cc_data_update_cb(cc_handle_t *handle, void (*callback)(void *arg))
{
    handle->data_update_cb = callback;
}

void cc_dev_descriptor_cb(cc_handle_t *handle, void (*callback)(void *arg))
{
    handle->dev_desc_cb = callback;
}
