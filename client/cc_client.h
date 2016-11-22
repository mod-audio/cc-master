#ifndef CC_CLIENT_H
#define CC_CLIENT_H


/*
****************************************************************************************************
*       INCLUDE FILES
****************************************************************************************************
*/

#include <cc/control_chain.h>


/*
****************************************************************************************************
*       MACROS
****************************************************************************************************
*/

enum cc_request_t {CC_DEVICE_LIST, CC_DEVICE_DESCRIPTOR, CC_DEVICE_STATUS, CC_ASSIGNMENT,
                   CC_UNASSIGNMENT, CC_DATA_UPDATE};


/*
****************************************************************************************************
*       CONFIGURATION
****************************************************************************************************
*/


/*
****************************************************************************************************
*       DATA TYPES
****************************************************************************************************
*/

typedef struct cc_client_t cc_client_t;


/*
****************************************************************************************************
*       FUNCTION PROTOTYPES
****************************************************************************************************
*/

cc_client_t *cc_client_new(const char *path);
void cc_client_delete(cc_client_t *client);

int cc_client_assignment(cc_client_t *client, cc_assignment_t *assignment);
void cc_client_unassignment(cc_client_t *client, cc_unassignment_t *unassignment);

int* cc_client_device_list(cc_client_t *client);
char *cc_client_device_descriptor(cc_client_t *client, int device_id);

void cc_client_device_status_cb(cc_client_t *client, void (*callback)(void *arg));
void cc_client_data_update_cb(cc_client_t *client, void (*callback)(void *arg));


/*
****************************************************************************************************
*       CONFIGURATION ERRORS
****************************************************************************************************
*/


#endif
