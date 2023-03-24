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

    // assignment id, device_id, actuator_id, label, value, min, max, def, mode, steps, unit, list_count, list_items
    // actuator_pair_id, assignment_pair_id
    // list_index, enumeration_frame_min, enumeration_frame_max, actuator_page_id
    cc_assignment_t ass_1 = {-1, dev_id, 1, "Mom On", 0, 0, 1, 0, CC_MODE_MOMENTARY, 0, "-", 0, NULL, -1, -1, 0, 0, 0, 0};
    cc_assignment_t ass_2 = {-1, dev_id, 5, "Mom Off", 1, 0, 1, 1, CC_MODE_MOMENTARY|CC_MODE_REVERSE, 0, "-", 0, NULL, -1, -1, 0, 0, 0, 0};

    ass_1.id = cc_assignment(handle, &ass_1, true);
    ass_2.id = cc_assignment(handle, &ass_2, true);

    if (ass_1.id < 0)
    {
        printf("error in assignment %i\n", ass_1.id);
    }
    if (ass_2.id < 0)
    {
        printf("error in assignment %i\n", ass_2.id);
    }

    printf("Test pages with momentary\n");

    //give some time to test the actuatots
    sleep(30);

    //assign actuator 1 with 0
    if (ass_1.id >= 0)
    {
        sleep(1);
        float update_value = 0.0f;
        cc_set_value_t update_data = {dev_id, ass_1.id, 0, update_value};
        const int idv = cc_value_set(handle, &update_data);
        printf("Value set: assignment: %i, value: %i\n", idv, (int)update_value);
        sleep(1);
     }

    //assign actuator 1 with 0
    if (ass_2.id >= 0)
    {
        sleep(1);
        float update_value = 0.0f;
        cc_set_value_t update_data = {dev_id, ass_2.id, 1, update_value};
        const int idv = cc_value_set(handle, &update_data);
        printf("Value set: assignment: %i, value: %i\n", idv, (int)update_value);
        sleep(1);
     }

    //unassign
    printf("removing assignments %i %i\n", ass_1.id, ass_2.id);
    cc_assignment_key_t key_1 = {ass_1.id, dev_id, -1};
    cc_unassignment(handle, &key_1);
    cc_assignment_key_t key_2 = {ass_2.id, dev_id, -1};
    cc_unassignment(handle, &key_2);
    sleep(1);


    cc_finish(handle);

    return 0;
}

