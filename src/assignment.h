
#ifndef ASSIGNMENT_H
#define ASSIGNMENT_H


/*
************************************************************************************************************************
*       INCLUDE FILES
************************************************************************************************************************
*/

#include <stdint.h>


/*
************************************************************************************************************************
*       MACROS
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*       CONFIGURATION
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*       DATA TYPES
************************************************************************************************************************
*/

typedef struct cc_assignment_t {
    int device_id;
    int actuator_id;
} cc_assignment_t;


/*
************************************************************************************************************************
*       FUNCTION PROTOTYPES
************************************************************************************************************************
*/

int cc_assignment_add(cc_assignment_t *assignment, uint8_t *buffer, uint16_t *written);
void cc_assignment_remove(int assignment_id, uint8_t *buffer, uint16_t *written);


/*
************************************************************************************************************************
*   CONFIGURATION ERRORS
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*   END HEADER
************************************************************************************************************************
*/

#endif