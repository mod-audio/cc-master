#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "control_chain.h"

#define SERIAL_PORT         "/dev/ttyACM0"
#define SERIAL_BAUDRATE     115200

int no_device = 1;

void dev_desc(void *arg)
{
    cc_dev_descriptor_t *desc = arg;
    printf("name = %s, n actuators = %i\n", desc->label->text, desc->actuators_count);
    for (int i = 0; i < desc->actuators_count; i++)
    {
        printf("   actuator id: %i\n", desc->actuators[i]->id);
    }

    no_device = 0;
}

int main(void)
{
    cc_handle_t *handle = cc_init(SERIAL_PORT, SERIAL_BAUDRATE);
    if (!handle)
    {
        printf("can't initiate control chain using %s\n", SERIAL_PORT);
        exit(1);
    }

    cc_dev_descriptor_cb(handle, dev_desc);

    printf("waiting device descriptor\n");
    while (no_device) sleep(1);

    cc_finish(handle);

    return 0;
}
