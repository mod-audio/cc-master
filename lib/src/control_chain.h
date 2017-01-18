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

#ifndef CONTROL_CHAIN_H
#define CONTROL_CHAIN_H

/*
****************************************************************************************************
*       INCLUDE FILES
****************************************************************************************************
*/

#include "core.h"
#include "utils.h"
#include "msg.h"
#include "handshake.h"
#include "device.h"
#include "assignment.h"
#include "update.h"


/*
****************************************************************************************************
*       MACROS
****************************************************************************************************
*/

#define CC_PROTOCOL_MAJOR       0
#define CC_PROTOCOL_MINOR       1
#define CC_PROTOCOL_VERSION     STR(CC_PROTOCOL_MAJOR) "." STR(CC_PROTOCOL_MINOR)


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


/*
****************************************************************************************************
*       FUNCTION PROTOTYPES
****************************************************************************************************
*/

int cc_assignment(cc_handle_t *handle, cc_assignment_t *assignment);
void cc_unassignment(cc_handle_t *handle, cc_unassignment_t *unassignment);
void cc_data_update_cb(cc_handle_t *handle, void (*callback)(void *arg));
void cc_device_status_cb(cc_handle_t *handle, void (*callback)(void *arg));


/*
****************************************************************************************************
*       CONFIGURATION ERRORS
****************************************************************************************************
*/


#endif
