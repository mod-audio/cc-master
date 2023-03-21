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

void assign_tasks(cc_handle_t *handle, int id)
{
    if (id >= 0)
    {
        sleep(1);
        float update_value = 0.0f;
        cc_set_value_t update_data = {dev_id, id, id, update_value};
        id = cc_value_set(handle, &update_data);
        printf("*** Value set: assignment: %i, value: %i\n", id, (int)update_value);
        sleep(1);
        update_value = 1.0f;
        update_data.value = update_value;
        id = cc_value_set(handle, &update_data);
        printf("*** Value set: assignment: %i, value: %i\n", id, (int)update_value);
        sleep(1);

        if (id >= 0)
        {
            printf("*** removing assignment %i\n", id);
            cc_assignment_key_t unass = {id, dev_id, -1};
            cc_unassignment(handle, &unass);
        }
        else
        {
            printf("*** control set fail\n");
        }
    }
    else
    {
        printf("*** assignment fail\n");
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

    printf("*** waiting device descriptor\n");
    while (no_device) sleep(1);

    int list_count = 0;
    cc_item_t items[] = {{"option 1", 1.0}, {"option 2", 2.0}, {"option 3", 3.0}};
    cc_item_t **list_items = malloc(sizeof(cc_item_t *) * list_count);

    for (int i = 0; i < list_count; ++i)
        list_items[i] = &items[i];

    // assignment id, device_id, actuator_id, label, value, min, max, def, mode, steps, unit, list_count, list_items
    // actuator_pair_id, assignment_pair_id
    // list_index, enumeration_frame_min, enumeration_frame_max, actuator_page_id
    cc_assignment_t ass1 = {-1, dev_id, 0, "gain", 1.0, 0.0, 1.0, 0.0, 1, 32, "dB",
        list_count, list_items, -1, -1, 0, 0, 0, 0};
    cc_assignment_t ass2 = {-1, dev_id, 1, "gain", 1.0, 0.0, 1.0, 0.0, 1, 32, "dB",
        list_count, list_items, -1, -1, 0, 0, 0, 0};
    cc_assignment_t ass3 = {-1, dev_id, 2, "gain", 1.0, 0.0, 1.0, 0.0, 1, 32, "dB",
        list_count, list_items, -1, -1, 0, 0, 0, 0};

    ass1.id = cc_assignment(handle, &ass1, true);
    printf("*** assignment id: %i\n", ass1.id);

    ass2.id = cc_assignment(handle, &ass2, true);
    printf("*** assignment id: %i\n", ass2.id);

    ass3.id = cc_assignment(handle, &ass3, true);
    printf("*** assignment id: %i\n", ass3.id);

    assign_tasks(handle, ass1.id);
    assign_tasks(handle, ass2.id);
    assign_tasks(handle, ass3.id);

    free(list_items);

    cc_finish(handle);

    return 0;
}
