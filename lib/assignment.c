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


/*
****************************************************************************************************
*       INTERNAL FUNCTIONS
****************************************************************************************************
*/


/*
****************************************************************************************************
*       GLOBAL FUNCTIONS
****************************************************************************************************
*/

int cc_assignment_add(cc_assignment_t *assignment)
{
    cc_device_t *device = cc_device_get(assignment->device_id);

    if (!device->assignments)
        device->assignments = calloc(CC_MAX_ASSIGNMENTS, sizeof(cc_assignment_t *));

    // store assignment
    for (int i = 0; i < CC_MAX_ASSIGNMENTS; i++)
    {
        if (!device->assignments[i])
        {
            // duplicate assignment
            cc_assignment_t *copy = malloc(sizeof(cc_assignment_t));
            device->assignments[i] = copy;
            memcpy(copy, assignment, sizeof(cc_assignment_t));

            return i;
        }
    }

    return -1;
}

void cc_assignment_remove(cc_unassignment_t *unassignment)
{
    cc_device_t *device = cc_device_get(unassignment->device_id);
    int id = unassignment->assignment_id;
    free(device->assignments[id]);
    device->assignments[id] = 0;
}
