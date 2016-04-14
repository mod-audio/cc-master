
#ifndef CONTROL_CHAIN_H
#define CONTROL_CHAIN_H

#include <stdint.h>
#include <pthread.h>
#include <libserialport.h>

typedef struct control_chain_t
{
    struct sp_port *sp;
    int state;
    void (*recv_callback)(void *arg);
    pthread_t recv_thread;
    volatile int running;

    uint8_t dev_address;
    uint8_t command;
    uint16_t data_size;
    uint8_t data_crc;
    uint8_t *data;
} cc_t;

cc_t* cc_init(const char *port_name, int baudrate);
void cc_finish(cc_t *cc);

void cc_set_recv_callback(cc_t *cc, void (*callback)(void *arg));
void cc_send(cc_t *cc);

#endif
