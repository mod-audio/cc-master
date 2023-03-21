#include <stdio.h>
#include <stdlib.h>
#include "cc_client.h"

int main(void)
{
    cc_client_t *client = cc_client_new("/tmp/control-chain.sock");

    int *device_list = cc_client_device_list(client);

    // list devices
    int i = 0;
    printf("device list:\n");
    while (device_list && device_list[i])
    {
        char *descriptor = cc_client_device_descriptor(client, device_list[i]);
        printf("  dev %i: %s\n", device_list[i], descriptor);
        free(descriptor);
        i++;
    }
    free(device_list);

    cc_client_delete(client);

    return 0;
}
