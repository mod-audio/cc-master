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
        printf("***   id = %i, value = %f\n", data->assignment_id, data->value);
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
    while (no_device)
    {
    	sleep(1);
    }

    cc_item_t items[] = {
        {"option 1", 0.f},
        {"option 2", 1.f},
        {"option 3", 2.f},
        {"option 4", 3.f},
        {"option 5", 4.f},
        {"option 6", 5.f},
        {"option 7", 6.f},
        {"option 8", 7.f},
        {"option 9", 8.f},
        {"option 10", 9.f},
        {"option 11", 10.f},
        {"option 12", 11.f}
    };
    int list_count = sizeof(items)/sizeof(items[0]);

    cc_item_t **list_items = malloc(sizeof(cc_item_t *) * list_count);

    for (int i = 0; i < list_count; ++i)
    {
        list_items[i] = &items[i];
    }

    // assignment of LED cycling

    // assignment id, device_id, actuator_id, label, value, min, max, def, mode, steps, unit, list_count, list_items
    // actuator_pair_id, assignment_pair_id
    // list_index, enumeration_frame_min, enumeration_frame_max, actuator_page_id
    cc_assignment_t ass_1 = {-1, dev_id, 1, "List1", 0.f, 0.f, list_count-1, 0.f, CC_MODE_INTEGER|CC_MODE_OPTIONS|CC_MODE_COLOURED, list_count, "-",
        list_count, list_items, -1, -1, 0, 0, 0, 0};

    ass_1.id = cc_assignment(handle, &ass_1, true);

    if (ass_1.id < 0)
    {
        printf("error in assignment %i\n", ass_1.id);
    }

    // assignment of Static LED list

    // assignment id, device_id, actuator_id, label, value, min, max, def, mode, steps, unit, list_count, list_items
    // actuator_pair_id, assignment_pair_id
    // list_index, enumeration_frame_min, enumeration_frame_max, actuator_page_id
    cc_assignment_t ass_2 = {-1, dev_id, 2, "List2", 0.f, 0.f, list_count-1, 0.0, list_count, CC_MODE_INTEGER|CC_MODE_OPTIONS, "-",
        list_count, list_items, -1, -1, 0, 0, 0, 0};

    ass_2.id = cc_assignment(handle, &ass_2, true);

    if (ass_2.id < 0)
    {
        printf("error in assignment %i\n", ass_2.id);
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

    cc_finish(handle);

    free(list_items);

    return 0;
}

