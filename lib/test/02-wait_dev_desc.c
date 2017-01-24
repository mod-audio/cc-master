#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "control_chain.h"

#define SERIAL_PORT         "/dev/ttyACM0"
#define SERIAL_BAUDRATE     115200

int no_device = 1;

void dev_desc(void *arg)
{
    cc_device_t *device = arg;
    char *descriptor = cc_device_descriptor(device->id);

    printf("received via callback: %s\n", descriptor);
    free(descriptor);

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

    cc_device_status_cb(handle, dev_desc);

    printf("waiting device descriptor\n");
    while (no_device) sleep(1);

    printf("list of devices\n");
    int *dev_list = cc_device_list(CC_DEVICE_LIST_ALL);

    int i = 0;
    while (dev_list[i])
    {
        printf("  dev id: %i\n", dev_list[i]);
        i++;
    }
    free(dev_list);

    cc_finish(handle);

    return 0;
}
