#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "cc_client.h"

static void data_update_cb(void *arg)
{
    cc_update_list_t *updates = arg;

    printf("device id: %i\n", updates->device_id);
    for (int i = 0; i < updates->count; i++)
    {
        cc_update_data_t *data = &updates->list[i];
        printf("id: %i, value: %f\n", data->assignment_id, data->value);
    }
}

int main(void)
{
    cc_client_t *client = cc_client_new("/tmp/control-chain.sock");

    // set data update callback
    cc_client_data_update_cb(client, data_update_cb);

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

    // options
    int list_count = 3;
    cc_item_t items[] = {{"option 1", 1.0}, {"option 2", 2.0}, {"option 3", 3.0}};
    cc_item_t **list_items = malloc(list_count * sizeof(cc_item_t *));
    for (int i = 0; i < list_count; ++i)
        list_items[i] = &items[i];

    int device_id = 1;

    // assignment id, device_id, actuator_id, label, value, min, max, def, mode, steps, unit, list_count, list_items
    // actuator_pair_id, assignment_pair_id
    // list_index, enumeration_frame_min, enumeration_frame_max, actuator_page_id
    cc_assignment_t assignment = {
        -1, device_id, 0, "gain", 1.0, 0.0, 2.0, 0.5, 1, 32, "dB", list_count, list_items,
        -1, -1,
        0, 0, 0, 0
    };

    // assignment
    printf("creating assignment\n");
    assignment.id = cc_client_assignment(client, &assignment);
    printf("assignment id: %i\n", assignment.id);
    free(list_items);

    sleep(1);

    if (assignment.id >= 0)
    {
        // unassignment
        cc_assignment_key_t key = {assignment.id, device_id, -1};
        printf("removing assignment: %i\n", assignment.id);
        cc_client_unassignment(client, &key);
    }
    else
    {
        printf("assignment fail\n");
    }

    cc_client_delete(client);
    printf("done\n");

    return 0;
}
