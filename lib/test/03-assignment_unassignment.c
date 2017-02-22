#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "control_chain.h"
#include "assignment.h"

#define SERIAL_PORT         "/dev/ttyACM0"
#define SERIAL_BAUDRATE     115200

int no_device = 1;
int dev_id = -1;

void dev_desc(void *arg)
{
    cc_device_t *device = arg;

    dev_id = device->id;
    printf("device %s connected\n", device->label->text);
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

    int act_id = 0;
    printf("creating assignment: dev %i, act: %i\n", dev_id, act_id);

    // assignment id, device_id, actuator_id, label, value, min, max, def, mode, steps, unit
    cc_assignment_t ass = {-1, dev_id, act_id, "gain", 1.0, 0.0, 1.0, 0.0, 1, 32, "dB"};

    int id = cc_assignment(handle, &ass);
    printf("assignment id: %i\n", id);

    if (id >= 0)
    {
        printf("removing assignment\n");
        cc_assignment_key_t unass = {dev_id, id};
        cc_unassignment(handle, &unass);
    }
    else
    {
        printf("assignment fail\n");
    }

    cc_finish(handle);

    return 0;
}
