#include "rdma_base.h"
#include "hip_kernels.hip"

#include <stdio.h>
#include <hip/hip_runtime.h>

#define BUF_SIZE 1024
#define SERVER_IP "10.224.0.181"

int main()
{
    struct rdma_conn c = {};

    /* --- RDMA setup --- */
    rdma_server_listen(&c, SERVER_IP, 20079);
    printf("HIP RDMA Server listening...\n");

    rdma_server_accept(&c);
    rdma_setup_qp(&c, 16, 16);

    /* --- Allocate and register GPU memory --- */
    if (rdma_register_gpu_memory(&c, BUF_SIZE) != 0) {
        fprintf(stderr, "GPU memory registration failed\n");
        return 1;
    }

    /* --- Post receive --- */
    rdma_post_recv(&c);

    struct ibv_wc wc;
    rdma_poll_cq(&c, &wc);

    char* buffer = (char*)malloc(BUF_SIZE * sizeof(char));
    hipMemcpy(buffer, c.buf, BUF_SIZE*sizeof(char), hipMemcpyDeviceToHost);
    printf("Server received data into GPU memory %c\n", buffer[0]);

    /* --- Process data on GPU --- */
    dim3 blk(256);
    dim3 grd((BUF_SIZE + blk.x - 1) / blk.x);
    hipLaunchKernelGGL(fill_buffer,
                       grd, blk, 0, 0,
                       (char *)c.buf, 'P', BUF_SIZE);
    hipDeviceSynchronize();

    /* --- Send pong --- */
    rdma_post_send(&c, BUF_SIZE, 1);
    rdma_poll_cq(&c, &wc);

    printf("Server sent pong\n");

    /* --- Cleanup --- */
    rdma_gpu_cleanup(&c);
    free(buffer);

    return 0;
}
