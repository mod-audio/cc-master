#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "control_chain.h"

//Duo
#define SERIAL_PORT            "/dev/ttyS3"
//DuoX
//#define SERIAL_PORT         "/dev/ttymxc0"
#define SERIAL_BAUDRATE     115200


int main(void)
{
    cc_handle_t *handle = cc_init(SERIAL_PORT, SERIAL_BAUDRATE);
    if (!handle)
    {
        printf("can't initiate control chain using %s\n", SERIAL_PORT);
        exit(1);
    }

    cc_finish(handle);

    return 0;
}
