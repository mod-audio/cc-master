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

#ifndef CC_HANDSHAKE_H
#define CC_HANDSHAKE_H


/*
****************************************************************************************************
*       INCLUDE FILES
****************************************************************************************************
*/

#include <stdint.h>
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


/*
****************************************************************************************************
*       DATA TYPES
****************************************************************************************************
*/

enum {CC_HANDSHAKE_OK, CC_UPDATE_AVAILABLE, CC_UPDATE_REQUIRED};

// handshake structure received from device
typedef struct cc_handshake_dev_t {
    string_t *uri;
    uint16_t random_id;
    version_t protocol, firmware;
} cc_handshake_dev_t;

// handshake structure sent to device
typedef struct cc_handshake_mod_t {
    uint16_t random_id;
    int status, device_id, channel;
} cc_handshake_mod_t;


/*
****************************************************************************************************
*       FUNCTION PROTOTYPES
****************************************************************************************************
*/

int cc_handshake_check(cc_handshake_dev_t *received, cc_handshake_mod_t *response);


/*
****************************************************************************************************
*       CONFIGURATION ERRORS
****************************************************************************************************
*/


#endif
