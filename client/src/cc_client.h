/*
 * This file is part of the control chain project
 *
 * Copyright (C) 2016 Ricardo Crudo <ricardo.crudo@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CC_CLIENT_H
#define CC_CLIENT_H


/*
****************************************************************************************************
*       INCLUDE FILES
****************************************************************************************************
*/

#include "control_chain.h"


/*
****************************************************************************************************
*       MACROS
****************************************************************************************************
*/


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
void cc_client_device_disable(cc_client_t *client, int device_id);

void cc_client_device_status_cb(cc_client_t *client, void (*callback)(void *arg));
void cc_client_data_update_cb(cc_client_t *client, void (*callback)(void *arg));


/*
****************************************************************************************************
*       CONFIGURATION ERRORS
****************************************************************************************************
*/


#endif
