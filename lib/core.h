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

#ifndef CC_CORE_H
#define CC_CORE_H

/*
****************************************************************************************************
*       INCLUDE FILES
****************************************************************************************************
*/


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

typedef struct cc_handle_t cc_handle_t;


/*
****************************************************************************************************
*       FUNCTION PROTOTYPES
****************************************************************************************************
*/

cc_handle_t* cc_init(const char *port_name, int baudrate);
void cc_finish(cc_handle_t *handle);


/*
****************************************************************************************************
*       CONFIGURATION ERRORS
****************************************************************************************************
*/


#endif
