#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "control_chain.h"

#define SERIAL_PORT         "/dev/ttyACM0"
#define SERIAL_BAUDRATE     115200

volatile int msg_ok;

void cc_callback(void *arg)
{
    cc_msg_t *msg = arg;

    printf("device address: %i, command: %i, data size: %i\n", msg->dev_address, msg->command, msg->data_size);
    for (int i = 0; i < msg->data_size; i++)
    {
        printf("%02X ", msg->data[i]);
    }
    if (msg->data_size) printf("\n");
    msg_ok = 1;
}

int main(void)
{
    cc_handle_t *handle = cc_init(SERIAL_PORT, SERIAL_BAUDRATE);
    if (!handle)
    {
        printf("can't initiate control chain using %s\n", SERIAL_PORT);
        exit(1);
    }

    cc_set_recv_callback(handle, cc_callback);
    printf("thread running\n");

    sleep(5);
    //while (!msg_ok);

    cc_finish(handle);

    // no data
    // s.write(b'\xA7\x01\x01\x00\x00\x00\x80')

    // 4 bytes data
    // s.write(b'\xA7\x01\x01\x04\x00\x48\x4F\xAA\xBB\xCC\xDD')

    // valgrind --leak-check=full --show-leak-kinds=all ./control-chain

    return 0;
}
