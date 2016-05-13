
#ifndef CONTROL_CHAIN_H
#define CONTROL_CHAIN_H

#include <stdint.h>

typedef struct cc_handle_t cc_handle_t;

typedef struct cc_msg_t
{
    uint8_t dev_address;
    uint8_t command;
    uint16_t data_size;
    uint8_t *data;
} cc_msg_t;

enum cc_cmd_t {CC_CMD_CHAIN_SYNC};

cc_handle_t* cc_init(const char *port_name, int baudrate);
void cc_finish(cc_handle_t *handle);

void cc_set_recv_callback(cc_handle_t *handle, void (*callback)(void *arg));
void cc_send(cc_handle_t *handle, cc_msg_t *msg);

#endif
