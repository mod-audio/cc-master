#include "cc_client.h"

int main(void)
{
    cc_client_t *client = cc_client_new("/tmp/control-chain.sock");
    cc_client_delete(client);
    return 0;
}