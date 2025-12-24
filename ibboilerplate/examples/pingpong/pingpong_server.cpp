#include "rdma_base.h"
#include <cstring>

int main()
{
    struct rdma_conn c;
    char ip[] = "10.224.0.181";

    rdma_server_listen(&c, ip, 20079);
    printf("Server listening...\n");

    rdma_server_accept(&c);
    rdma_setup_qp(&c, 16, 16);
    rdma_register_memory(&c, 1024);

    rdma_post_recv(&c);

    struct ibv_wc wc;
    rdma_poll_cq(&c, &wc);

    printf("Received: %s\n", (char *)c.buf);

    std::strcpy((char*)c.buf, "pong");
    rdma_post_send(&c, 4, 1);
    rdma_poll_cq(&c, &wc);

    rdma_cleanup(&c);
    return 0;
}
