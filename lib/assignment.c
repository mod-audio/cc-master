/*
****************************************************************************************************
*       INCLUDE FILES
****************************************************************************************************
*/

#include "assignment.h"
#include "device.h"
#include "utils.h"

#include <stdlib.h>
#include <string.h>


/*
****************************************************************************************************
*       INTERNAL MACROS
****************************************************************************************************
*/

#define CC_MAX_ASSIGNMENTS  256


/*
****************************************************************************************************
*       INTERNAL CONSTANTS
****************************************************************************************************
*/


/*
****************************************************************************************************
*       INTERNAL DATA TYPES
****************************************************************************************************
*/


/*
****************************************************************************************************
*       INTERNAL GLOBAL VARIABLES
****************************************************************************************************
*/

static int g_assignments_ids[CC_MAX_DEVICES][CC_MAX_ASSIGNMENTS];


/*
****************************************************************************************************
*       INTERNAL FUNCTIONS
****************************************************************************************************
*/

static int new_id(int device_id)
{
    for (int i = 0; i < CC_MAX_ASSIGNMENTS; i++)
    {
        if (g_assignments_ids[device_id][i] == 0)
        {
            g_assignments_ids[device_id][i] = 1;
            return i;
        }
    }

    return -1;
}

static void delete_id(int device_id, int assignment_id)
{
    g_assignments_ids[device_id][assignment_id] = 0;
}


/*
****************************************************************************************************
*       GLOBAL FUNCTIONS
****************************************************************************************************
*/

int cc_assignment_add(cc_assignment_t *assignment)
{
    cc_device_t *device = cc_device_get(assignment->device_id);

    if (!device->assignments)
        device->assignments = lili_create();

    // duplicate assignment
    cc_assignment_t *copy = malloc(sizeof(cc_assignment_t));
    memcpy(copy, assignment, sizeof(cc_assignment_t));

    // get new assignment id
    int assignment_id = new_id(device->id);
    if (assignment_id >= 0)
    {
        // store id and push to assigments list
        assignment->id = assignment_id;
        copy->id = assignment_id;
        lili_push(device->assignments, copy);
    }

    return assignment_id;
}

void cc_assignment_remove(cc_unassignment_t *unassignment)
{
    cc_device_t *device = cc_device_get(unassignment->device_id);

    int index = 0;
    LILI_FOREACH(device->assignments, node)
    {
        cc_assignment_t *assignment = node->data;
        if (unassignment->assignment_id == assignment->id)
        {
            delete_id(device->id, unassignment->assignment_id);
            lili_pop_from(device->assignments, index);
            free(assignment);
            return;
        }

        index++;
    }
}
