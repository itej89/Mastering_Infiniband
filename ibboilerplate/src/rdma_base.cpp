#include "rdma_base.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#define CHECK(x) do { if ((x)) { perror(#x); return -1; } } while (0)

static int wait_for_event(struct rdma_event_channel *ec,
                          enum rdma_cm_event_type expect,
                          struct rdma_cm_event **out)
{
    CHECK(rdma_get_cm_event(ec, out));
    if ((*out)->event != expect) {
        fprintf(stderr, "Unexpected CM event %d (expected %d)\n",
                (*out)->event, expect);
        rdma_ack_cm_event(*out);
        return -1;
    }
    return 0;
}

/* ---------------- SERVER ---------------- */

int rdma_server_listen(struct rdma_conn *c,
                       const char *ip,
                       int port)
{
    memset(c, 0, sizeof(*c));
    c->is_server = 1;

    c->ec = rdma_create_event_channel();
    CHECK(!c->ec);

    CHECK(rdma_create_id(c->ec, &c->id, NULL, RDMA_PS_TCP));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (ip)
        inet_pton(AF_INET, ip, &addr.sin_addr);
    else
        addr.sin_addr.s_addr = INADDR_ANY;

    CHECK(rdma_bind_addr(c->id, (struct sockaddr *)&addr));
    CHECK(rdma_listen(c->id, 1));

    return 0;
}

int rdma_server_accept(struct rdma_conn *c)
{
    struct rdma_cm_event *event;

    CHECK(wait_for_event(c->ec,
                         RDMA_CM_EVENT_CONNECT_REQUEST,
                         &event));

    c->id = event->id;
    rdma_ack_cm_event(event);

    return 0;
}

/* ---------------- CLIENT ---------------- */

int rdma_client_connect(struct rdma_conn *c,
                         const char *ip,
                         int port)
{
    memset(c, 0, sizeof(*c));
    c->is_server = 0;

    c->ec = rdma_create_event_channel();
    CHECK(!c->ec);

    CHECK(rdma_create_id(c->ec, &c->id, NULL, RDMA_PS_TCP));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    CHECK(rdma_resolve_addr(c->id, NULL,
                            (struct sockaddr *)&addr, 2000));

    struct rdma_cm_event *event;
    CHECK(wait_for_event(c->ec,
                         RDMA_CM_EVENT_ADDR_RESOLVED,
                         &event));
    rdma_ack_cm_event(event);

    CHECK(rdma_resolve_route(c->id, 2000));
    CHECK(wait_for_event(c->ec,
                         RDMA_CM_EVENT_ROUTE_RESOLVED,
                         &event));
    rdma_ack_cm_event(event);

    return 0;
}

/* ---------------- SHARED SETUP ---------------- */

int rdma_setup_qp(struct rdma_conn *c,
                  int send_wr,
                  int recv_wr)
{
    c->pd = ibv_alloc_pd(c->id->verbs);
    CHECK(!c->pd);

    c->comp_ch = ibv_create_comp_channel(c->id->verbs);
    CHECK(!c->comp_ch);

    c->cq = ibv_create_cq(c->id->verbs,
                          send_wr + recv_wr,
                          NULL,
                          c->comp_ch,
                          0);
    CHECK(!c->cq);

    struct ibv_qp_init_attr qp_attr = {0};
    qp_attr.send_cq = c->cq;
    qp_attr.recv_cq = c->cq;
    qp_attr.qp_type = IBV_QPT_RC;
    qp_attr.cap.max_send_wr = send_wr;
    qp_attr.cap.max_recv_wr = recv_wr;
    qp_attr.cap.max_send_sge = 1;
    qp_attr.cap.max_recv_sge = 1;

    CHECK(rdma_create_qp(c->id, c->pd, &qp_attr));
    c->qp = c->id->qp;

    struct rdma_conn_param param = {0};
    if (c->is_server)
        CHECK(rdma_accept(c->id, &param));
    else {
        CHECK(rdma_connect(c->id, &param));
        struct rdma_cm_event *event;
        CHECK(wait_for_event(c->ec,
                             RDMA_CM_EVENT_ESTABLISHED,
                             &event));
        rdma_ack_cm_event(event);
    }

    return 0;
}

int rdma_register_memory(struct rdma_conn *c,
                          size_t size)
{
    c->buf_size = size;
    c->buf = malloc(size);
    CHECK(!c->buf);

    c->mr = ibv_reg_mr(c->pd,
                       c->buf,
                       size,
                       IBV_ACCESS_LOCAL_WRITE);
    CHECK(!c->mr);

    return 0;
}

/* ---------------- DATA PATH ---------------- */

int rdma_post_recv(struct rdma_conn *c)
{
    struct ibv_sge sge = {
        .addr = (uintptr_t)c->buf,
        .length = static_cast<uint32_t>(c->buf_size),
        .lkey = c->mr->lkey
    };

    struct ibv_recv_wr wr = {
        .wr_id = 1,
        .sg_list = &sge,
        .num_sge = 1
    };

    struct ibv_recv_wr *bad;
    CHECK(ibv_post_recv(c->qp, &wr, &bad));

    return 0;
}

int rdma_post_send(struct rdma_conn *c,
                   size_t len,
                   int signaled)
{
    struct ibv_sge sge = {
        .addr = (uintptr_t)c->buf,
        .length = static_cast<uint32_t>(len),
        .lkey = c->mr->lkey
    };

    struct ibv_send_wr wr = {
        .wr_id = 2,
        .opcode = IBV_WR_SEND,
        .sg_list = &sge,
        .num_sge = 1,
        .send_flags = signaled ? static_cast<unsigned int>(IBV_SEND_SIGNALED) : 0
    };

    struct ibv_send_wr *bad;
    CHECK(ibv_post_send(c->qp, &wr, &bad));

    return 0;
}

int rdma_poll_cq(struct rdma_conn *c,
                 struct ibv_wc *wc)
{
    int n;
    do {
        n = ibv_poll_cq(c->cq, 1, wc);
    } while (n == 0);

    if (wc->status != IBV_WC_SUCCESS) {
        fprintf(stderr, "CQ error %d\n", wc->status);
        return -1;
    }
    return 0;
}

int rdma_post_send_imm(struct rdma_conn *c,
                       size_t len,
                       uint32_t imm,
                       int signaled)
{
    struct ibv_sge sge = {
        .addr   = (uintptr_t)c->buf,
        .length = static_cast<unsigned int>(len),
        .lkey   = c->mr->lkey
    };

    struct ibv_send_wr wr = {
        .wr_id      = 3,  // distinguish from normal SEND
        .opcode     = IBV_WR_SEND_WITH_IMM,
        .imm_data   = htonl(imm),
        .sg_list    = &sge,
        .num_sge    = 1,
        .send_flags = signaled ? (unsigned)IBV_SEND_SIGNALED : 0
    };

    struct ibv_send_wr *bad;
    if (ibv_post_send(c->qp, &wr, &bad))
        return -1;

    return 0;
}

int rdma_poll_cq_imm(struct rdma_conn *c,
                     struct ibv_wc *wc,
                     uint32_t *imm)
{
    int n;
    do {
        n = ibv_poll_cq(c->cq, 1, wc);
    } while (n == 0);

    if (wc->status != IBV_WC_SUCCESS)
        return -1;

    if ((wc->opcode == IBV_WC_RECV) &&
        (wc->wc_flags & IBV_WC_WITH_IMM)) {
        if (imm)
            *imm = ntohl(wc->imm_data);
    } else {
        if (imm)
            *imm = 0;
    }

    return 0;
}


/* ---------------- CLEANUP ---------------- */

void rdma_cleanup(struct rdma_conn *c)
{
    if (c->mr) ibv_dereg_mr(c->mr);
    if (c->buf) free(c->buf);
    if (c->qp) rdma_destroy_qp(c->id);
    if (c->cq) ibv_destroy_cq(c->cq);
    if (c->comp_ch) ibv_destroy_comp_channel(c->comp_ch);
    if (c->pd) ibv_dealloc_pd(c->pd);
    if (c->id) rdma_destroy_id(c->id);
    if (c->ec) rdma_destroy_event_channel(c->ec);
}

void rdma_gpu_cleanup(struct rdma_conn *c)
{
    if (c->mr) ibv_dereg_mr(c->mr);
    if (c->buf) hipFree(c->buf);
    if (c->qp) rdma_destroy_qp(c->id);
    if (c->cq) ibv_destroy_cq(c->cq);
    if (c->comp_ch) ibv_destroy_comp_channel(c->comp_ch);
    if (c->pd) ibv_dealloc_pd(c->pd);
    if (c->id) rdma_destroy_id(c->id);
    if (c->ec) rdma_destroy_event_channel(c->ec);
}

int rdma_register_gpu_memory(struct rdma_conn *c, size_t size)
{
    hipError_t err;
    err = hipMalloc(&c->buf, size);
    if (err != hipSuccess) return -1;

    c->mr = ibv_reg_mr(c->pd,
                       c->buf,
                       size,
                       IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
    if (!c->mr) return -1;

    c->buf_size = size;
    return 0;
}