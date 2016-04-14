#include <stdio.h>
#include <stdlib.h>
#include "control_chain.h"

#define SERIAL_PORT         "/dev/ttyACM0"
#define SERIAL_BAUDRATE     115200

void cc_callback(void *arg)
{
    cc_t *cc = (cc_t *) arg;
    printf("device address: %i, command: %i\n", cc->dev_address, cc->command);
    for (int i = 0; i < cc->data_size; i++)
    {
        printf("%02X ", cc->data[i]);
    }
    if (cc->data_size) printf("\n");

    cc_send(cc);
}

int main(void)
{
    cc_t *cc_master;

    cc_master = cc_init(SERIAL_PORT, SERIAL_BAUDRATE);
    if (!cc_master)
    {
        printf("can't initiate control chain using %s\n", SERIAL_PORT);
        exit(1);
    }

    cc_set_recv_callback(cc_master, cc_callback);
    printf("we are good\n");

    while (1);

    // no data
    // s.write(b'\xA7\x01\x01\x00\x00\x00\x80')

    // 4 bytes data
    // s.write(b'\xA7\x01\x01\x04\x00\x48\x4F\xAA\xBB\xCC\xDD')

    return 0;
}
