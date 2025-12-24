#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>      // inet_pton
#include <netinet/in.h>     // sockaddr_in

#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>

#define SERVER_IP "10.224.1.61"
#define PORT 20079
#define MSG_SIZE 1024

int main(){
    struct rdma_cm_id *id;
    struct rdma_event_channel *ec;
    struct rdma_cm_event *event;

    struct ibv_pd *pd;
    struct ibv_comp_channel* comp_chan;
    struct ibv_cq *cq;
    struct ibv_qp_init_attr qp_attr;
    struct ibv_mr *mr;
    char *buffer;


    //Create event channel id
    ec = rdma_create_event_channel();
    rdma_create_id(ec, &id, NULL, RDMA_PS_TCP); 
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &addr.sin_addr);

    rdma_resolve_addr(id, NULL, (struct sockaddr *)&addr, 2000);
    rdma_get_cm_event(ec, &event);
    rdma_ack_cm_event(event);

    rdma_resolve_route(id, 2000);
    rdma_get_cm_event(ec, &event);
    rdma_ack_cm_event(event);

    //Allcoate PD and CQ
    pd = ibv_alloc_pd(id->verbs);
    comp_chan = ibv_create_comp_channel(id->verbs);
    cq = ibv_create_cq(id->verbs, 10, NULL, comp_chan, 0);
    ibv_req_notify_cq(cq, 0);

    //Create QP
    memset(&qp_attr, 0, sizeof(qp_attr));
    qp_attr.send_cq = cq;
    qp_attr.recv_cq = cq;
    qp_attr.qp_type = IBV_QPT_RC;
    qp_attr.cap.max_send_wr = 10;
    qp_attr.cap.max_recv_wr = 10;
    qp_attr.cap.max_send_sge = 1;
    qp_attr.cap.max_recv_sge = 1;
    rdma_create_qp(id, pd, &qp_attr);

    //Register Memory
    buffer = (char*)malloc(MSG_SIZE);
    strcpy(buffer, "Hello RDMA!");
    mr = ibv_reg_mr(pd, buffer, MSG_SIZE, IBV_ACCESS_LOCAL_WRITE);

    //Connect
    struct rdma_conn_param conn_param = {0};
    rdma_connect(id, &conn_param);
    rdma_get_cm_event(ec, &event);
    rdma_ack_cm_event(event);

    //Post a send
    struct ibv_sge sge = {.addr = (uintptr_t)buffer, .length = MSG_SIZE, .lkey = mr->lkey};
    struct ibv_send_wr wr = {.wr_id = 1, .next = NULL, .sg_list = &sge, .num_sge = 1, .opcode = IBV_WR_SEND, .send_flags = IBV_SEND_SIGNALED};
    struct ibv_send_wr *bad_wr;
    ibv_post_send(id->qp, &wr, &bad_wr);


    //Poll completion
    struct ibv_wc wc;
    while (ibv_poll_cq(cq, 1, &wc) == 0);
    printf("Client sent: %s\n", buffer);

    //Cleanup
    rdma_disconnect(id);
    rdma_destroy_qp(id);
    ibv_dereg_mr(mr);
    free(buffer);
    rdma_destroy_id(id);
    rdma_destroy_event_channel(ec);

    return 0;

}