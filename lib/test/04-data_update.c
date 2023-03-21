#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "control_chain.h"
#include "assignment.h"

//Duo
// #define SERIAL_PORT            "/dev/ttyS3"
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
    while (no_device) sleep(1);

    int list_count = 3;
    cc_item_t items[] = {{"option 1", 1.0}, {"option 2", 2.0}, {"option 3", 3.0}};
    cc_item_t **list_items = malloc(sizeof(cc_item_t *) * list_count);

    for (int i = 0; i < list_count; ++i)
        list_items[i] = &items[i];

    // assignment id, device_id, actuator_id, label, value, min, max, def, mode, steps, unit, list_count, list_items
    // actuator_pair_id, assignment_pair_id
    // list_index, enumeration_frame_min, enumeration_frame_max, actuator_page_id
//     cc_assignment_t ass = {-1, dev_id, 0, "Tap", 50.0, 28.0, 280.0, 120.0, CC_MODE_TAP_TEMPO|CC_MODE_REAL, 0, "BPM",
//         0, NULL, -1, -1, 0, 0, 0, 0};
//     cc_assignment_t ass = {-1, dev_id, 0, "Opt", 0.0, 0.0, 2.0, 0.0, CC_MODE_INTEGER|CC_MODE_OPTIONS, 0, "",
//         list_count, list_items, -1, -1, 0, 0, 0, 0};
    cc_assignment_t ass = {-1, dev_id, 2, "Toggle", 0.0, 0.0, 1.0, 0.0, CC_MODE_INTEGER|CC_MODE_TOGGLE, 0, "",
        0, NULL, -1, -1, 0, 0, 0, 0};

    int id = cc_assignment(handle, &ass, true);
    if (id >= 0)
    {
        ass.id = id;
        sleep(60);
        printf("removing assignment %i\n", id);
        cc_assignment_key_t key = {id, dev_id, -1};
        cc_unassignment(handle, &key);
        sleep(1);
    }
    else
    {
        printf("assignment fail\n");
    }

    free(list_items);

    cc_finish(handle);

    return 0;
}
