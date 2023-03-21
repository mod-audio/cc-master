#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "control_chain.h"
#include "assignment.h"

//Duo
// #define SERIAL_PORT            "/dev/ttyS1"
//DuoX
#define SERIAL_PORT         "/dev/ttymxc0"
#define SERIAL_BAUDRATE     115200

int no_device = 1;
int dev_id = -1;

void dev_desc(void *arg)
{
    cc_device_t *device = arg;
    dev_id = device->id;
    if (device->label)
        printf("*** device %s connected\n", device->label->text);
    else
        printf("*** device connected\n");
    no_device = 0;
}

void data_update(void *arg)
{
    cc_update_list_t *updates = arg;
    printf("*** received %i updates\n", updates->count);

    for (int i = 0; i < updates->count; ++i)
    {
        cc_update_data_t *data = &updates->list[i];
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

    cc_device_status_cb(handle, dev_desc);
    cc_data_update_cb(handle, data_update);

    printf("waiting device descriptor\n");
    while (no_device) sleep(1);

    //assigning 4 different tap types

    //tap 1, bpm
    // assignment id, device_id, actuator_id, label, value, min, max, def, mode, steps, unit, list_count, list_items
    // actuator_pair_id, assignment_pair_id
    // list_index, enumeration_frame_min, enumeration_frame_max, actuator_page_id
    cc_assignment_t ass_1 = {-1, dev_id, 0, "Tap", 120.0, 20.0, 280.0, 10.0, 8, 0, "bpm",
        0, NULL, -1, -1, 0, 0, 0, 0};

    ass_1.id = cc_assignment(handle, &ass_1, true);

    if (ass_1.id < 0)
    {
        printf("error in assignment %i\n", ass_1.id);
    }

    //tap 2 s
    cc_assignment_t ass_2 = {-1, dev_id, 1, "Tap", 0.5, 0.1, 5.0, 10.0, 8, 0, "s",
        0, NULL, -1, -1, 0, 0, 0, 0};

    ass_2.id = cc_assignment(handle, &ass_2, true);

    if (ass_2.id < 0)
    {
        printf("error in assignment %i\n", ass_2.id);
    }

    //tap 3 hz
    cc_assignment_t ass_3 = {-1, dev_id, 2, "Tap", 2, 0.1, 50.0, 10.0, 8, 0, "hz",
        0, NULL, -1, -1, 0, 0, 0, 0};

    ass_3.id = cc_assignment(handle, &ass_3, true);

    if (ass_3.id < 0)
    {
        printf("error in assignment %i\n", ass_3.id);
    }

    //give some time to test the actuatots
    sleep(60);

    //unassign
    printf("removing assignment %i\n", ass_1.id);
    cc_assignment_key_t key_1 = {ass_1.id, dev_id, -1};
    cc_unassignment(handle, &key_1);
    sleep(1);

    printf("removing assignment %i\n", ass_2.id);
    cc_assignment_key_t key_2 = {ass_2.id, dev_id, -1};
    cc_unassignment(handle, &key_2);
    sleep(1);

    printf("removing assignment %i\n", ass_3.id);
    cc_assignment_key_t key_3 = {ass_3.id, dev_id, -1};
    cc_unassignment(handle, &key_3);
    sleep(1);

    cc_finish(handle);

    return 0;
}
