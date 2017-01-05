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

#ifndef CC_REQUEST_H
#define CC_REQUEST_H


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

#define CC_DEV_LIST_REQ_FORMAT          "n"
#define CC_DEV_LIST_REPLY_FORMAT        ""

#define CC_DEV_DESCRIPTOR_REQ_FORMAT    "{si}"
#define CC_DEV_DESCRIPTOR_REPLY_FORMAT  ""

#define CC_DEV_STATUS_REQ_FORMAT        "{si}"
#define CC_DEV_STATUS_REPLY_FORMAT      "n"
#define CC_DEV_STATUS_EVENT_FORMAT      "{si,si}"

#define CC_ASSIGNMENT_REQ_FORMAT        "{si,si,ss,sf,sf,sf,sf,si}"
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


/*
****************************************************************************************************
*       FUNCTION PROTOTYPES
****************************************************************************************************
*/


/*
****************************************************************************************************
*       CONFIGURATION ERRORS
****************************************************************************************************
*/


#endif
