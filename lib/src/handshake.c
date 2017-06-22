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

/*
****************************************************************************************************
*       INCLUDE FILES
****************************************************************************************************
*/

#include <stdlib.h>
#include "control_chain.h"
#include "handshake.h"


/*
****************************************************************************************************
*       INTERNAL MACROS
****************************************************************************************************
*/


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

int cc_handshake_check(cc_handshake_dev_t *received, cc_handshake_mod_t *response)
{
    // TODO: check device firmware version

    int status = CC_HANDSHAKE_OK;

    if (received->protocol.major < CC_PROTOCOL_MAJOR)
        status = CC_UPDATE_REQUIRED;
    else if (received->protocol.minor < CC_PROTOCOL_MINOR)
        status = CC_UPDATE_AVAILABLE;

    response->random_id = received->random_id;
    response->status = status;
    response->device_id = 0;
    response->channel = cc_device_count(received->uri->text);

    return status;
}
