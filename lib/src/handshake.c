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

static int g_frames[CC_MAX_FRAMES];


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

cc_handshake_mod_t* cc_handshake_check(cc_handshake_dev_t *received)
{
    // TODO: check protocol version
    // TODO: check device firmware version
    // TODO: calculate channel (based on URI)
    // TODO: check if device has this handshake already

    static cc_handshake_mod_t to_send;
    to_send.random_id = received->random_id;
    to_send.status = 0;
    to_send.channel = 0;

    for (int i = 0; i < CC_MAX_FRAMES; i++)
    {
        if (g_frames[i] == 0)
        {
            g_frames[i] = 1;
            to_send.device_id = i + 1;
            return &to_send;
        }
    }

    return 0;
}

void cc_handshake_destroy(cc_handshake_dev_t *handshake)
{
    string_destroy(handshake->uri);
    free(handshake);
}
