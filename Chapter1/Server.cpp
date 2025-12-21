#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>      // inet_pton
#include <netinet/in.h>     // sockaddr_in

#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>

#define PORT 20079
#define MSG_SIZE 1024


int main(){
    struct rdma_cm_id *listen_id, *client_id;
    struct rdma_event_channel *ec;
    struct rdma_cm_event *event;
    
    struct ibv_pd *pd;
    struct ibv_comp_channel *comp_chan;
    struct ibv_cq *cq;
    struct ibv_qp_init_attr qp_attr;
    struct ibv_mr *mr;

    char *buffer;

    //Create event channel
    ec = rdma_create_event_channel();
    rdma_create_id(ec, &listen_id, NULL, RDMA_PS_TCP);
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port  = htons(PORT);
    // addr.sin_addr.s_addr = INADDR_ANY;
    inet_pton(AF_INET, "10.224.1.61", &addr.sin_addr);
    rdma_bind_addr(listen_id, (struct sockaddr *)&addr);
    rdma_listen(listen_id, 1);
    printf("Server listening on port %d...\n", PORT);

    //Accept Connection
    rdma_get_cm_event(ec, &event);
    client_id = event->id;
    rdma_ack_cm_event(event);

    //Allocate pd
    pd = ibv_alloc_pd(client_id->verbs);

    //Allocate cq
    comp_chan = ibv_create_comp_channel(client_id->verbs);
    cq = ibv_create_cq(client_id->verbs, 10, NULL, comp_chan, 0);
    ibv_req_notify_cq(cq, 0);

    //Create qp
    memset(&qp_attr, 0 , sizeof(qp_attr));
    qp_attr.send_cq = cq;
    qp_attr.recv_cq = cq;
    qp_attr.qp_type = IBV_QPT_RC;
    qp_attr.cap.max_send_wr = 10;
    qp_attr.cap.max_recv_wr = 10;
    qp_attr.cap.max_send_sge = 1;
    qp_attr.cap.max_recv_sge = 1;
    rdma_create_qp(client_id, pd, &qp_attr);

    //Register Memory
    buffer = (char*)malloc(MSG_SIZE);
    mr = ibv_reg_mr(pd, buffer, MSG_SIZE, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);

    //Post a receive
    struct ibv_sge sge = {.addr=(uintptr_t)buffer, .length=MSG_SIZE, .lkey=mr->lkey};
    struct ibv_recv_wr wr  = {.wr_id=1, .next=NULL, .sg_list=&sge, .num_sge=1};
    struct ibv_recv_wr *bad_wr;
    ibv_post_recv(client_id->qp, &wr, &bad_wr);

    // 8. Accept connection
    struct rdma_conn_param conn_param = {0};
    rdma_accept(client_id, &conn_param);

    //Poll completion
    struct ibv_wc wc;
    while(ibv_poll_cq(cq, 1, &wc) == 0);
    printf("Server received : %s \n", buffer); 

    //Cleanup
    rdma_disconnect(client_id);
    rdma_destroy_qp(client_id);
    ibv_dereg_mr(mr);
    free(buffer);
    rdma_destroy_id(client_id);
    rdma_destroy_id(listen_id);
    rdma_destroy_event_channel(ec);

    return 0;
}
