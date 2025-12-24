// hip_kernels.h
#pragma once
#include <hip/hip_runtime.h>

__global__ void fill_buffer(char *buf, char val, size_t n)
{
    size_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n)
        buf[i] = val;

    if (i==0)
        printf("Sent %c \n", val);
    
}