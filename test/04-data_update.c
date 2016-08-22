#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "control_chain.h"
#include "assignment.h"

#define SERIAL_PORT         "/dev/ttyACM0"
#define SERIAL_BAUDRATE     115200

int no_device = 1;
int dev_id;

void dev_desc(void *arg)
{
    cc_dev_descriptor_t *desc = arg;
    printf("device id = %i, name = %s plugged in\n", desc->id, desc->label->text);
    dev_id = desc->id;
    no_device = 0;
}

void data_update(void *arg)
{
    cc_data_update_t *updates = arg;

    for (int i = 0; i < updates->count; ++i)
    {
        cc_data_t *data = &updates->updates_list[i];
        printf("id = %i, value = %f\n", data->assignment_id, data->value);
    }
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
    cc_data_update_cb(handle, data_update);

    printf("waiting device descriptor\n");
    while (no_device) sleep(1);

/*
typedef struct cc_assignment_t {
    int device_id;
    int actuator_id;
    float value, min, max, def;
    uint32_t mode;
} cc_assignment_t;
*/
    cc_assignment_t ass = {1, 0, 0.0, 0.0, 1.0, 0.0, 1};
    int ass_id = cc_assignment(handle, &ass);
    sleep(5);
    cc_unassignment(handle, ass_id);

    cc_finish(handle);

    return 0;
}
