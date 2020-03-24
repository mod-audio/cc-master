#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "control_chain.h"
#include "assignment.h"

//Duo
#define SERIAL_PORT            "/dev/ttyS3"
//DuoX
//#define SERIAL_PORT         "/dev/ttymxc0"
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

    int list_count = 3;
    cc_item_t items[] = {{"option 1", 1.0}, {"option 2", 2.0}, {"option 3", 3.0}};
    cc_item_t **list_items = malloc(sizeof(cc_item_t *) * list_count);

    for (int i = 0; i < list_count; ++i)
        list_items[i] = &items[i];

    // assignment id, device_id, actuator_id, label, value, min, max, def, mode, steps, unit,
    // list_count, list_items
    int assign_id = 1;
    cc_assignment_t ass = {assign_id, dev_id, 0, "Tap", 50.0, 10.0, 5000.0, 10.0, 80, 0, "ms",
        0, NULL};

    printf("assigning %i\n", assign_id);

    int id = cc_assignment(handle, &ass);
    if (id >= 0)
    {
        sleep(60);
        printf("removing assignment %i\n", id);
        cc_assignment_key_t key = {id, dev_id};
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
