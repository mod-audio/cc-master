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
    int list_count = 3;
    cc_item_t items[] = {{"option 1", 0.f}, {"option 2", 1.f}, {"option 3", 2.f}};
    cc_item_t **list_items = malloc(sizeof(cc_item_t *) * list_count);

    for (int i = 0; i < list_count; ++i)
        list_items[i] = &items[i];

    // assignment id, device_id, actuator_id, label, value, min, max, def, mode, steps, unit, list_count, list_items
    // actuator_pair_id, assignment_pair_id
    // list_index, enumeration_frame_min, enumeration_frame_max, actuator_page_id
    cc_assignment_t ass = {0, dev_id, act_id, "list", 1.f, 0.f, 2.f, 1.f, CC_MODE_OPTIONS|CC_MODE_INTEGER, 3, "-",
        list_count, list_items, -1, -1, 0, 0, 0, 0};

    int id = cc_assignment(handle, &ass, true);
    printf("assignment id: %i\n", id);

    if (id >= 0)
    {
        ass.id = id;
        //wait 1 sec
        sleep(1);
        // assign value 3
        float listvalue_update = 2.0;
        cc_set_value_t update_data = {dev_id, id, act_id, listvalue_update};
        id = cc_value_set(handle, &update_data);
        printf("Value set: assignment: %i, value: %i\n", id, (int)listvalue_update);
        sleep(2);

        if (id >= 0)
        {
            printf("removing assignment %i\n", id);
            cc_assignment_key_t unass = {id, dev_id, -1};
            cc_unassignment(handle, &unass);
        }
        else 
        {
            printf("control set fail\n");
        }
    }
    else
    {
        printf("assignment fail\n");
    }

    free(list_items);

    cc_finish(handle);

    return 0;
}
