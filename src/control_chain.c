
#include <stdlib.h>
#include <string.h>
#include "control_chain.h"

#define CC_SERIAL_BUFFER_SIZE   2048
#define CC_SYNC_BYTE            0xA7
#define CC_HEADER_SIZE          6
#define CC_DATA_TIMEOUT         10

// fields names and sizes in bytes
// DEV_ADDRESS (1), COMMAND (1), DATA_SIZE (2), DATA_CHECKSUM (1), HEADER_CHECKSUM (1), DATA (N)

enum {WAITING_SYNCING, WAITING_HEADER, WAITING_DATA};

/*  http://stackoverflow.com/a/15171925/1283578
    8-bit CRC with polynomial x^8+x^6+x^3+x^2+1, 0x14D.
    Chosen based on Koopman, et al. (0xA6 in his notation = 0x14D >> 1):
    http://www.ece.cmu.edu/~koopman/roses/dsn04/koopman04_crc_poly_embedded.pdf
 */
static uint8_t crc8_table[] = {
    0x00, 0x3e, 0x7c, 0x42, 0xf8, 0xc6, 0x84, 0xba, 0x95, 0xab, 0xe9, 0xd7,
    0x6d, 0x53, 0x11, 0x2f, 0x4f, 0x71, 0x33, 0x0d, 0xb7, 0x89, 0xcb, 0xf5,
    0xda, 0xe4, 0xa6, 0x98, 0x22, 0x1c, 0x5e, 0x60, 0x9e, 0xa0, 0xe2, 0xdc,
    0x66, 0x58, 0x1a, 0x24, 0x0b, 0x35, 0x77, 0x49, 0xf3, 0xcd, 0x8f, 0xb1,
    0xd1, 0xef, 0xad, 0x93, 0x29, 0x17, 0x55, 0x6b, 0x44, 0x7a, 0x38, 0x06,
    0xbc, 0x82, 0xc0, 0xfe, 0x59, 0x67, 0x25, 0x1b, 0xa1, 0x9f, 0xdd, 0xe3,
    0xcc, 0xf2, 0xb0, 0x8e, 0x34, 0x0a, 0x48, 0x76, 0x16, 0x28, 0x6a, 0x54,
    0xee, 0xd0, 0x92, 0xac, 0x83, 0xbd, 0xff, 0xc1, 0x7b, 0x45, 0x07, 0x39,
    0xc7, 0xf9, 0xbb, 0x85, 0x3f, 0x01, 0x43, 0x7d, 0x52, 0x6c, 0x2e, 0x10,
    0xaa, 0x94, 0xd6, 0xe8, 0x88, 0xb6, 0xf4, 0xca, 0x70, 0x4e, 0x0c, 0x32,
    0x1d, 0x23, 0x61, 0x5f, 0xe5, 0xdb, 0x99, 0xa7, 0xb2, 0x8c, 0xce, 0xf0,
    0x4a, 0x74, 0x36, 0x08, 0x27, 0x19, 0x5b, 0x65, 0xdf, 0xe1, 0xa3, 0x9d,
    0xfd, 0xc3, 0x81, 0xbf, 0x05, 0x3b, 0x79, 0x47, 0x68, 0x56, 0x14, 0x2a,
    0x90, 0xae, 0xec, 0xd2, 0x2c, 0x12, 0x50, 0x6e, 0xd4, 0xea, 0xa8, 0x96,
    0xb9, 0x87, 0xc5, 0xfb, 0x41, 0x7f, 0x3d, 0x03, 0x63, 0x5d, 0x1f, 0x21,
    0x9b, 0xa5, 0xe7, 0xd9, 0xf6, 0xc8, 0x8a, 0xb4, 0x0e, 0x30, 0x72, 0x4c,
    0xeb, 0xd5, 0x97, 0xa9, 0x13, 0x2d, 0x6f, 0x51, 0x7e, 0x40, 0x02, 0x3c,
    0x86, 0xb8, 0xfa, 0xc4, 0xa4, 0x9a, 0xd8, 0xe6, 0x5c, 0x62, 0x20, 0x1e,
    0x31, 0x0f, 0x4d, 0x73, 0xc9, 0xf7, 0xb5, 0x8b, 0x75, 0x4b, 0x09, 0x37,
    0x8d, 0xb3, 0xf1, 0xcf, 0xe0, 0xde, 0x9c, 0xa2, 0x18, 0x26, 0x64, 0x5a,
    0x3a, 0x04, 0x46, 0x78, 0xc2, 0xfc, 0xbe, 0x80, 0xaf, 0x91, 0xd3, 0xed,
    0x57, 0x69, 0x2b, 0x15
};

uint8_t crc8(uint8_t *data, size_t len)
{
    uint8_t *end, crc = 0x00;

    if (len == 0)
        return crc;

    crc ^= 0xff;
    end = data + len;

    do
    {
        crc = crc8_table[crc ^ *data++];
    } while (data < end);

    return crc ^ 0xff;
}

static void* cc_parser(void *arg)
{
    cc_t *cc = (cc_t *) arg;

    uint8_t buffer[CC_HEADER_SIZE];
    enum sp_return ret;

    while (1)
    {
        // waiting sync byte
        if (cc->state == WAITING_SYNCING)
        {
            ret = sp_blocking_read(cc->sp, buffer, 1, 0);
            if (ret > 0)
            {
                if (buffer[0] == CC_SYNC_BYTE)
                    cc->state = WAITING_HEADER;
            }

            // take the shortcut
            continue;
        }

        // waiting header
        else if(cc->state == WAITING_HEADER)
        {
            ret = sp_blocking_read(cc->sp, buffer, CC_HEADER_SIZE, 0);
            if (ret == CC_HEADER_SIZE)
            {
                // verify header checksum
                uint8_t crc = buffer[CC_HEADER_SIZE-1];
                if (crc8(buffer, CC_HEADER_SIZE-1) == crc)
                {
                    cc->state = WAITING_DATA;
                    cc->dev_address = buffer[0];
                    cc->command = buffer[1];
                    cc->data_size = buffer[3];
                    cc->data_size <<= 8;
                    cc->data_size |= buffer[2];
                    cc->data_crc = buffer[4];

                    if (cc->data_size == 0)
                        cc->data_crc = 0;
                }
                else
                {
                    cc->state = WAITING_SYNCING;
                }
            }
            else
            {
                cc->state = WAITING_SYNCING;
            }
        }

        // waiting data
        else if (cc->state == WAITING_DATA)
        {
            ret = sp_blocking_read(cc->sp, cc->data, cc->data_size, CC_DATA_TIMEOUT);
            if (ret == cc->data_size)
            {
                // verify data checksum
                if (crc8(cc->data, cc->data_size) == cc->data_crc)
                {
                    if (cc->recv_callback)
                        cc->recv_callback(cc);
                }
            }

            // always go back to initial state regardless it got data or not
            cc->state = WAITING_SYNCING;
        }
    }

    return NULL;
}

cc_t* cc_init(const char *port_name, int baudrate)
{
    cc_t *cc = (cc_t *) malloc(sizeof (cc_t));
    if (cc == NULL)
        return NULL;

    // init object with null data
    memset(cc, 0, sizeof (cc_t));

    // get serial port
    enum sp_return ret;
    ret = sp_get_port_by_name(port_name, &(cc->sp));
    if (ret != SP_OK)
    {
        cc_finish(cc);
        return NULL;
    }

    // open serial port
    ret = sp_open(cc->sp, SP_MODE_READ_WRITE);
    if (ret != SP_OK)
    {
        cc_finish(cc);
        return NULL;
    }

    // configure serial port
    sp_set_baudrate(cc->sp, baudrate);

    // create data buffer
    cc->data = (uint8_t *) malloc(CC_SERIAL_BUFFER_SIZE);
    if (cc->data == NULL)
    {
        cc_finish(cc);
        return NULL;
    }

    // set thread attributes
    pthread_attr_t attributes;
    pthread_attr_init(&attributes);
    pthread_attr_setinheritsched(&attributes, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setscope(&attributes, PTHREAD_SCOPE_PROCESS);
    //pthread_attr_setschedpolicy(&attributes, SCHED_FIFO);

    // create thread
    int ret_val = pthread_create(&cc->recv_thread, &attributes, cc_parser, (void*) cc);
    pthread_attr_destroy(&attributes);

    if (ret_val == 0)
        return cc;

    cc_finish(cc);

    return NULL;
}

void cc_finish(cc_t *cc)
{
    if (cc)
    {
        if (cc->sp)
        {
            sp_close(cc->sp);
            sp_free_port(cc->sp);
        }

        if (cc->data)
            free(cc->data);

        pthread_join(cc->recv_thread, NULL);

        free(cc);
    }
}

void cc_set_recv_callback(cc_t *cc, void (*callback)(void *arg))
{
    if (cc)
    {
        cc->recv_callback = callback;
    }
}

void cc_send(cc_t *cc)
{
    const uint8_t sync_byte = CC_SYNC_BYTE;
    uint8_t buffer[CC_HEADER_SIZE];

    if (cc)
    {
        buffer[0] = cc->dev_address;
        buffer[1] = cc->command;
        buffer[2] = (cc->data_size >> 0) & 0xFF;
        buffer[3] = (cc->data_size >> 8) & 0xFF;
        buffer[4] = crc8(cc->data, cc->data_size);
        buffer[5] = crc8(buffer, CC_HEADER_SIZE-1);
        sp_nonblocking_write(cc->sp, &sync_byte, 1);
        sp_nonblocking_write(cc->sp, buffer, CC_HEADER_SIZE);
        sp_nonblocking_write(cc->sp, cc->data, cc->data_size);
    }
}
