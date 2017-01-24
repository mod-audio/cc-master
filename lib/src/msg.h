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

#ifndef CC_MSG_H
#define CC_MSG_H


/*
****************************************************************************************************
*       INCLUDE FILES
****************************************************************************************************
*/

#include <stdint.h>


/*
****************************************************************************************************
*       MACROS
****************************************************************************************************
*/

#define CC_MSG_HEADER_SIZE  4


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

// commands definition
enum cc_cmd_t {CC_CMD_CHAIN_SYNC, CC_CMD_HANDSHAKE, CC_CMD_DEV_DESCRIPTOR, CC_CMD_ASSIGNMENT,
               CC_CMD_DATA_UPDATE, CC_CMD_UNASSIGNMENT, CC_NUM_COMMANDS};

// fields names and sizes in bytes
// DEV_ADDRESS (1), COMMAND (1), DATA_SIZE (2), DATA (N), CHECKSUM (1)

typedef struct cc_msg_t {
    uint8_t device_id;
    uint8_t command;
    uint16_t data_size;
    uint8_t *header, *data;
} cc_msg_t;


/*
****************************************************************************************************
*       FUNCTION PROTOTYPES
****************************************************************************************************
*/

cc_msg_t* cc_msg_new(void);
void cc_msg_delete(cc_msg_t *msg);
void cc_msg_parser(const cc_msg_t *msg, void *data_struct);
void cc_msg_builder(int command, const void *data_struct, cc_msg_t *msg);


/*
****************************************************************************************************
*       CONFIGURATION ERRORS
****************************************************************************************************
*/


#endif
