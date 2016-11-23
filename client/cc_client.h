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

#define CC_DEV_LIST_REQ_FORMAT          "n"
#define CC_DEV_LIST_REPLY_FORMAT        ""

#define CC_DEV_DESCRIPTOR_REQ_FORMAT    "{si}"
#define CC_DEV_DESCRIPTOR_REPLY_FORMAT  ""

#define CC_DEV_STATUS_REQ_FORMAT        "{si}"
#define CC_DEV_STATUS_REPLY_FORMAT      "n"
#define CC_DEV_STATUS_EVENT_FORMAT      "{si,si}"

#define CC_ASSIGNMENT_REQ_FORMAT        "{si,si,sf,sf,sf,sf,si}"
#define CC_ASSIGNMENT_REPLY_FORMAT      "{si}"

#define CC_UNASSIGNMENT_REQ_FORMAT      "{si,si}"
#define CC_UNASSIGNMENT_REPLY_FORMAT    "n"

#define CC_DATA_UPDATE_REQ_FORMAT       "{si}"
#define CC_DATA_UPDATE_REPLY_FORMAT     "n"
#define CC_DATA_UPDATE_EVENT_FORMAT     "{si,ss}"


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
