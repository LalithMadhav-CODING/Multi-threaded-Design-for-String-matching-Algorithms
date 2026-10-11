#include "cuda_search.h"
#include <cuda_runtime.h>
#include <iostream>

// CUDA Kernel for string searching
__global__ void searchKernel(const char* text, size_t text_len, const char* pattern, int pattern_len, int* count) {
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t stride = blockDim.x * gridDim.x;
    
    for (size_t i = idx; i <= text_len - pattern_len; i += stride) {
        bool match = true;
        for (int j = 0; j < pattern_len; ++j) {
            if (text[i + j] != pattern[j]) {
                match = false;
                break;
            }
        }
        if (match) {
            atomicAdd(count, 1);
        }
    }
}

namespace cuda_search {

double runCUDA(const std::string& text, const std::string& pattern, size_t& out_count) {
    char* d_text = nullptr;
    char* d_pattern = nullptr;
    int* d_count = nullptr;
    
    size_t text_len = text.length();
    int pattern_len = pattern.length();
    
    if (text_len == 0 || pattern_len == 0) return 0.0;

    cudaEvent_t start, stop;
    cudaEventCreate(&start);
    cudaEventCreate(&stop);

    cudaEventRecord(start);
    
    // Allocate device memory
    cudaError_t err1 = cudaMalloc(&d_text, text_len);
    cudaError_t err2 = cudaMalloc(&d_pattern, pattern_len);
    cudaError_t err3 = cudaMalloc(&d_count, sizeof(int));
    
    if (err1 != cudaSuccess || err2 != cudaSuccess || err3 != cudaSuccess) {
        std::cerr << "CUDA Memory Allocation Failed" << std::endl;
        return 0.0;
    }

    cudaMemcpy(d_text, text.c_str(), text_len, cudaMemcpyHostToDevice);
    cudaMemcpy(d_pattern, pattern.c_str(), pattern_len, cudaMemcpyHostToDevice);
    
    int initial_count = 0;
    cudaMemcpy(d_count, &initial_count, sizeof(int), cudaMemcpyHostToDevice);
    
    int blockSize = 1024;
    // We launch a fixed number of blocks to avoid exceeding limits
    // and let the loop in the kernel handle the full workload.
    int numBlocks = 65535; 
    
    searchKernel<<<numBlocks, blockSize>>>(d_text, text_len, d_pattern, pattern_len, d_count);
    
    cudaDeviceSynchronize();
    
    int final_count = 0;
    cudaMemcpy(&final_count, d_count, sizeof(int), cudaMemcpyDeviceToHost);
    out_count = final_count;
    
    cudaFree(d_text);
    cudaFree(d_pattern);
    cudaFree(d_count);
    
    cudaEventRecord(stop);
    cudaEventSynchronize(stop);
    
    float milliseconds = 0;
    cudaEventElapsedTime(&milliseconds, start, stop);
    
    return milliseconds / 1000.0;
}

} // namespace cuda_search
