#pragma once

#include <rdma/rdma_cma.h>
#include <infiniband/verbs.h>
#include <stddef.h>

struct rdma_conn {
    /* CM */
    struct rdma_event_channel *ec;
    struct rdma_cm_id *id;

    /* Verbs */
    struct ibv_pd *pd;
    struct ibv_cq *cq;
    struct ibv_comp_channel *comp_ch;
    struct ibv_qp *qp;

    /* Memory */
    void *buf;
    size_t buf_size;
    struct ibv_mr *mr;

    int is_server;
};

/* Server */
int rdma_server_listen(struct rdma_conn *c,
                       const char *ip,
                       int port);

int rdma_server_accept(struct rdma_conn *c);

/* Client */
int rdma_client_connect(struct rdma_conn *c,
                         const char *ip,
                         int port);

/* Shared setup */
int rdma_setup_qp(struct rdma_conn *c,
                  int send_wr,
                  int recv_wr);

int rdma_register_memory(struct rdma_conn *c,
                          size_t size);

/* Data path */
int rdma_post_recv(struct rdma_conn *c);
int rdma_post_send(struct rdma_conn *c,
                   size_t len,
                   int signaled);

int rdma_poll_cq(struct rdma_conn *c,
                 struct ibv_wc *wc);

/* SEND with immediate */
int rdma_post_send_imm(struct rdma_conn *c,
                       size_t len,
                       uint32_t imm,
                       int signaled);

/* Poll CQ and extract imm if present */
int rdma_poll_cq_imm(struct rdma_conn *c,
                     struct ibv_wc *wc,
                     uint32_t *imm);

/* Cleanup */
void rdma_cleanup(struct rdma_conn *c);

#include <hip/hip_runtime.h>
int rdma_register_gpu_memory(struct rdma_conn *c, size_t size);
void rdma_gpu_cleanup(struct rdma_conn *c);