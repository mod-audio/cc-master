
#define SYNC_BYTE           0xA7
#define THIS_DEV_ADDRESS    0x01
#define BROADCAST_ADDRESS   0x00

int led = 0;

int assignment_done, need_handshake = 1;

enum cc_cmd_t {CC_CMD_CHAIN_SYNC, CC_CMD_HANDSHAKE, CC_CMD_DEV_DESCRIPTOR, CC_CMD_ASSIGNMENT, CC_CMD_DATA_UPDATE,
               CC_CMD_UNASSIGNMENT};

static uint8_t crc8_table[] = {
    0x00, 0x3e, 0x7c, 0x42, 0xf8, 0xc6, 0x84, 0xba, 0x95, 0xab, 0xe9, 0xd7,
    0x6d, 0x53, 0x11, 0x2f, 0x4f, 0x71, 0x33, 0x0d, 0xb7, 0x89, 0xcb, 0xf5,
    0xda, 0xe4, 0xa6, 0x98, 0x22, 0x1c, 0x5e, 0x60, 0x9e, 0xa0, 0xe2, 0xdc,
    0x66, 0x58, 0x1a, 0x24, 0x0b, 0x35, 0x77, 0x49, 0xf3, 0xcd, 0x8f, 0xb1,
    0xd1, 0xef, 0xad, 0x93, 0x29, 0x17, 0x55, 0x6b, 0x44, 0x7a, 0x38, 0x06,
    0xbc, 0x82, 0xc0, 0xfe, 0x59, 0x67, 0x25, 0x1b, 0xa1, 0x9f, 0xdd, 0xe3,
    0xcc, 0xf2, 0xb0, 0x8e, 0x34, 0x0a, 0x48, 0x76, 0x16, 0x28, 0x6a, 0x54,
    0xee, 0xd0, 0x92, 0xac, 0x83, 0xbd, 0xff, 0xc1, 0x7b, 0x45, 0x07, 0x39,
    0xc7, 0xf9, 0xbb, 0x85, 0x3f, 0x01, 0x43, 0x7d, 0x52, 0x6c, 0x2e, 0x10,
    0xaa, 0x94, 0xd6, 0xe8, 0x88, 0xb6, 0xf4, 0xca, 0x70, 0x4e, 0x0c, 0x32,
    0x1d, 0x23, 0x61, 0x5f, 0xe5, 0xdb, 0x99, 0xa7, 0xb2, 0x8c, 0xce, 0xf0,
    0x4a, 0x74, 0x36, 0x08, 0x27, 0x19, 0x5b, 0x65, 0xdf, 0xe1, 0xa3, 0x9d,
    0xfd, 0xc3, 0x81, 0xbf, 0x05, 0x3b, 0x79, 0x47, 0x68, 0x56, 0x14, 0x2a,
    0x90, 0xae, 0xec, 0xd2, 0x2c, 0x12, 0x50, 0x6e, 0xd4, 0xea, 0xa8, 0x96,
    0xb9, 0x87, 0xc5, 0xfb, 0x41, 0x7f, 0x3d, 0x03, 0x63, 0x5d, 0x1f, 0x21,
    0x9b, 0xa5, 0xe7, 0xd9, 0xf6, 0xc8, 0x8a, 0xb4, 0x0e, 0x30, 0x72, 0x4c,
    0xeb, 0xd5, 0x97, 0xa9, 0x13, 0x2d, 0x6f, 0x51, 0x7e, 0x40, 0x02, 0x3c,
    0x86, 0xb8, 0xfa, 0xc4, 0xa4, 0x9a, 0xd8, 0xe6, 0x5c, 0x62, 0x20, 0x1e,
    0x31, 0x0f, 0x4d, 0x73, 0xc9, 0xf7, 0xb5, 0x8b, 0x75, 0x4b, 0x09, 0x37,
    0x8d, 0xb3, 0xf1, 0xcf, 0xe0, 0xde, 0x9c, 0xa2, 0x18, 0x26, 0x64, 0x5a,
    0x3a, 0x04, 0x46, 0x78, 0xc2, 0xfc, 0xbe, 0x80, 0xaf, 0x91, 0xd3, 0xed,
    0x57, 0x69, 0x2b, 0x15
};

uint8_t crc8(const uint8_t *data, uint32_t len)
{
    const uint8_t *end;
    uint8_t crc = 0x00;

    if (len == 0)
        return crc;

    crc ^= 0xff;
    end = data + len;

    do {
        crc = crc8_table[crc ^ *data++];
    } while (data < end);

    return crc ^ 0xff;
}


void setup()
{
    // user led
    pinMode(13, OUTPUT);
    digitalWrite(13, LOW);

    // serial write enable
    pinMode(2, OUTPUT);
    digitalWrite(2, LOW);

    Serial.begin(115200);
}

void loop()
{
}

void send_msg(uint8_t command, uint8_t *data, uint16_t data_size)
{
    uint8_t i = 0, buffer[32];

    // dev address
    buffer[i++] = THIS_DEV_ADDRESS;

    // command
    buffer[i++] = command;

    // data size
    buffer[i++] = (data_size >> 0) & 0xFF;
    buffer[i++] = (data_size >> 8) & 0xFF;

    if (data_size > 0)
    {
        uint8_t j = 0;
        for (j = 0; j < data_size; j++)
        {
            buffer[i++] = data[j];
        }
    }

    buffer[i] = crc8(buffer, i);
    i++;

    uint8_t sync = SYNC_BYTE;
    Serial.write(&sync, 1);
    Serial.write(buffer, i);
}

void send_handshake()
{
    uint8_t handshake[] = {0x07, 'a', 'r', 'd', 'u', 'i', 'n', 'o', 0xB0, 0xCA, 1, 0, 2, 3, 4};
    send_msg(CC_CMD_HANDSHAKE, handshake, sizeof (handshake));
}

void send_dev_desc()
{
    uint8_t data[] = {0x04, 'T', 'E', 'S', 'T', 2, 1, 2};
    send_msg(CC_CMD_DEV_DESCRIPTOR, data, sizeof(data));
}

void confirm_assignment()
{
    assignment_done = 1;
    send_msg(CC_CMD_ASSIGNMENT, NULL, 0);
    digitalWrite(13, HIGH);
}

void confirm_unassignment()
{
    assignment_done = 0;
    send_msg(CC_CMD_UNASSIGNMENT, NULL, 0);
}

void send_data_update()
{
    uint8_t *pvalue, data[6] = {0x01, 0, 0, 0, 0, 0};
    static float value;
    pvalue = (uint8_t *) &value;
    data[2] = *pvalue++;
    data[3] = *pvalue++;
    data[4] = *pvalue++;
    data[5] = *pvalue++;
    send_msg(CC_CMD_DATA_UPDATE, data, sizeof(data));
    value += 1.0;
}

void parser(int command, uint8_t *data)
{
    if (command == CC_CMD_CHAIN_SYNC)
    {
        // handshake cycle is when data[0] == 1
        if (need_handshake && data[0] == 1)
            send_handshake();
        else if (assignment_done)
            send_data_update();
    }
    else if (command == CC_CMD_HANDSHAKE)
    {
//        digitalWrite(13, HIGH);
        need_handshake = 0;
    }
    else if (command == CC_CMD_DEV_DESCRIPTOR)
    {
        send_dev_desc();
    }
    else if (command == CC_CMD_ASSIGNMENT)
    {
        confirm_assignment();
    }
    else if (command == CC_CMD_UNASSIGNMENT)
    {
        confirm_unassignment();
    }
}

/*
  SerialEvent occurs whenever a new data comes in the
 hardware serial RX.  This routine is run between each
 time loop() runs, so using delay inside loop can delay
 response.  Multiple bytes of data may be available.
 */
void serialEvent()
{
    static uint8_t data[32];
    static uint8_t state, command, aux;
    static uint16_t data_size, recv_data;

    while (Serial.available() > 0)
    {
        uint8_t byte;
        Serial.readBytes(&byte, 1);

        data[state + recv_data] = byte;

        switch (state)
        {
            // sync
            case 0:
                if (byte == SYNC_BYTE)
                {
                    state++;
                }
                break;

            // address
            case 1:
                if (byte == THIS_DEV_ADDRESS ||
                    byte == BROADCAST_ADDRESS)
                {
                    state++;
                }
                break;

            // command
            case 2:
                command = byte;
                data_size = 0;
                state++;
                break;

            // data size LSB
            case 3:
                aux = byte;
                state++;
                break;

            // data size MSB
            case 4:
                data_size = byte;
                data_size <<= 8;
                data_size |= aux;
                state++;
                break;

            // data
            case 5:
                if (data_size == 0)
                {
                    if (crc8(&data[1], 4) == byte)
                    {
                        parser(command, &data[5]);
                    }

                    state = 0;
                    recv_data = 0;
                }
                else
                {
                    recv_data++;
                    if (recv_data == data_size)
                        state++;
                }
                break;

            // crc
            case 6:
                if (crc8(&data[1], data_size + 4) == byte)
                {
                    parser(command, &data[5]);
                }

                state = 0;
                recv_data = 0;
                break;
        }
    }
}
