#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "control_chain.h"
#include "assignment.h"

#define SERIAL_PORT         "/dev/ttyACM0"
#define SERIAL_BAUDRATE     115200

int no_device = 1;

void dev_desc(void *arg)
{
    cc_dev_descriptor_t *desc = arg;
    printf("name = %s, n actuators = %i\n", desc->label->text, desc->actuators_count);
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

    int dev_id = 1, act_id = 0;
    printf("creating assignment: dev %i, act: %i\n", dev_id, act_id);
    cc_assignment_t ass = {-1, dev_id, act_id, 1.0, 0.0, 1.0, 0.0, 1};
    int id = cc_assignment(handle, &ass);
    printf("assignment id: %i\n", id);

    printf("removing assignment\n");
    cc_unassignment_t unass = {1, id};
    cc_unassignment(handle, &unass);

    cc_finish(handle);

    return 0;
}
