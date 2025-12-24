#include "rdma_base.h"
#include <stdio.h>
#include <cstring>

int main()
{
    struct rdma_conn c;

    rdma_client_connect(&c, "10.224.0.181", 20079);
    rdma_setup_qp(&c, 16, 16);
    rdma_register_memory(&c, 1024);

    rdma_post_recv(&c);

    std::strcpy((char*)c.buf, "ping");
    rdma_post_send(&c, 4, 1);

    struct ibv_wc wc;
    rdma_poll_cq(&c, &wc);
    rdma_poll_cq(&c, &wc);

    printf("Received reply: %s\n", (char *)c.buf);

    rdma_cleanup(&c);
    return 0;
}
