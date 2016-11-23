/*
****************************************************************************************************
*       INCLUDE FILES
****************************************************************************************************
*/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "sockcli.h"


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

typedef struct sockcli_t {
    int sock_fd;
} sockcli_t;


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

sockcli_t* sockcli_init(const char *path)
{
    sockcli_t *client = calloc(1, sizeof(sockcli_t));

    // create socket endpoint
    client->sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (client->sock_fd < 0)
    {
        perror("ERROR opening socket");
        exit(1);
    }

    // set server address
    struct sockaddr_un remote;
    remote.sun_family = AF_UNIX;
    strcpy(remote.sun_path, path);
    int len = strlen(remote.sun_path) + sizeof(remote.sun_family);

    // connect to the server
    if (connect(client->sock_fd, (struct sockaddr*)&remote, len) < 0)
    {
        perror("ERROR connecting");
        exit(1);
    }

    return client;
}

void sockcli_finish(sockcli_t *client)
{
    close(client->sock_fd);
    free(client);
}

int sockcli_read(sockcli_t *client, void *buffer, size_t size)
{
    bzero(buffer, size);
    return recv(client->sock_fd, buffer, size, 0);
}

int sockcli_write(sockcli_t *client, const void *buffer, size_t size)
{
    return send(client->sock_fd, buffer, size, 0);
}
