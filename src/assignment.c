
/*
************************************************************************************************************************
*       INCLUDE FILES
************************************************************************************************************************
*/

#include "assignment.h"


/*
************************************************************************************************************************
*       INTERNAL MACROS
************************************************************************************************************************
*/

#define CC_MAX_ASSIGNMENTS  1000


/*
************************************************************************************************************************
*       INTERNAL CONSTANTS
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*       INTERNAL DATA TYPES
************************************************************************************************************************
*/

struct assignment_key_t {
    int device_id, actuator_id;
};


/*
************************************************************************************************************************
*       INTERNAL GLOBAL VARIABLES
************************************************************************************************************************
*/

static struct assignment_key_t g_assignments[CC_MAX_ASSIGNMENTS];


/*
************************************************************************************************************************
*       INTERNAL FUNCTIONS
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*       GLOBAL FUNCTIONS
************************************************************************************************************************
*/

int cc_assignment_add(cc_assignment_t *assignment, uint8_t *buffer, uint16_t *written)
{
    *written = 0;

    // get next free assignment
    int assignment_id = -1;
    for (int i = 0; i < CC_MAX_ASSIGNMENTS; i++)
    {
        if (g_assignments[i].device_id == 0)
        {
            g_assignments[i].device_id = assignment->device_id;
            g_assignments[i].actuator_id = assignment->actuator_id;
            assignment_id = i;
            break;
        }
    }

    // no free assignment available
    if (assignment_id < 0)
        return -1;

    int i = 0;
    buffer[i++] = assignment_id;
    buffer[i++] = assignment->actuator_id;

    *written = i;

    return assignment_id;
}

int cc_assignment_remove(int assignment_id, uint8_t *buffer, uint16_t *written)
{
    if (assignment_id < 0 || assignment_id > CC_MAX_ASSIGNMENTS)
        return -1;

    struct assignment_key_t *assignment = &g_assignments[assignment_id];

    if (buffer)
    {
        buffer[0] = assignment->actuator_id;
        *written = 1;
    }

    // make this assignment id available again
    int device_id = assignment->device_id;
    assignment->device_id = 0;

    return device_id;
}
