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

#ifndef CC_DEVICE_H
#define CC_DEVICE_H


/*
****************************************************************************************************
*       INCLUDE FILES
****************************************************************************************************
*/

#include "handshake.h"
#include "assignment.h"
#include "utils.h"


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

#define CC_MAX_DEVICES  8


/*
****************************************************************************************************
*       DATA TYPES
****************************************************************************************************
*/

// enumeration to filter list of devices
// REGISTERED means that the device already received the device descriptor
// UNREGISTERED means that the device still waiting for the device descriptor
enum {CC_DEVICE_LIST_ALL, CC_DEVICE_LIST_REGISTERED, CC_DEVICE_LIST_UNREGISTERED};

// device status
enum {CC_DEVICE_DISCONNECTED, CC_DEVICE_CONNECTED};

// device control
enum {CC_DEVICE_DISABLE, CC_DEVICE_ENABLE};

typedef struct cc_actuator_t {
    int id;
} cc_actuator_t;

typedef struct cc_device_t {
    int id, status;
    string_t *label, *uri;
    cc_actuator_t **actuators;
    int actuators_count;
    cc_assignment_t **assignments;
    unsigned int timeout;
    version_t firmware;
} cc_device_t;


/*
****************************************************************************************************
*       FUNCTION PROTOTYPES
****************************************************************************************************
*/

// create and return a device, or NULL if fail
cc_device_t* cc_device_create(cc_handshake_dev_t *handshake);

// destroy the device
void cc_device_destroy(int device_id);

// return the device descriptor in json format
char* cc_device_descriptor(int device_id);

// return a NULL terminated list containing the filtered devices id
int* cc_device_list(int filter);

// return the device pointer or NULL if id is invalid
cc_device_t* cc_device_get(int device_id);


/*
****************************************************************************************************
*       CONFIGURATION ERRORS
****************************************************************************************************
*/


#endif
