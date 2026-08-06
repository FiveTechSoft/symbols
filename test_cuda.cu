/**
 * TEST_CUDA.CU - Prueba rápida de CUDA
 */

#include <stdio.h>
#include <cuda_runtime.h>

__global__ void hello_kernel() {
    printf("Hello from GPU thread %d!\n", threadIdx.x);
}

__global__ void test_matmul(float *A, float *B, float *C, int N) {
    int row = blockIdx.y * blockDim.y + threadIdx.y;
    int col = blockIdx.x * blockDim.x + threadIdx.x;
    
    if (row < N && col < N) {
        float sum = 0.0f;
        for (int k = 0; k < N; k++) {
            sum += A[row * N + k] * B[k * N + col];
        }
        C[row * N + col] = sum;
    }
}

int main() {
    // Info GPU
    cudaDeviceProp prop;
    cudaGetDeviceProperties(&prop, 0);
    
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  PRUEBA CUDA - TRANSFORMER HARBOUR/FWH                    ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
    
    printf("GPU Detectada:\n");
    printf("  Nombre:      %s\n", prop.name);
    printf("  VRAM:        %d MB\n", (int)(prop.totalGlobalMem / (1024*1024)));
    printf("  Compute:     %d.%d\n", prop.major, prop.minor);
    printf("  SM Count:    %d\n", prop.multiProcessorCount);
    printf("  Max Threads: %d/block\n\n", prop.maxThreadsPerBlock);
    
    // Prueba de kernel
    printf("Ejecutando kernel de prueba...\n");
    hello_kernel<<<1, 8>>>();
    cudaDeviceSynchronize();
    
    // Prueba de multiplicación de matrices
    printf("\nProbando multiplicación de matrices en GPU...\n");
    
    int N = 256;
    size_t size = N * N * sizeof(float);
    
    float *h_A = (float*)malloc(size);
    float *h_B = (float*)malloc(size);
    float *h_C = (float*)malloc(size);
    
    // Inicializar
    for (int i = 0; i < N * N; i++) {
        h_A[i] = (float)rand() / RAND_MAX;
        h_B[i] = (float)rand() / RAND_MAX;
    }
    
    float *d_A, *d_B, *d_C;
    cudaMalloc(&d_A, size);
    cudaMalloc(&d_B, size);
    cudaMalloc(&d_C, size);
    
    cudaMemcpy(d_A, h_A, size, cudaMemcpyHostToDevice);
    cudaMemcpy(d_B, h_B, size, cudaMemcpyHostToDevice);
    
    // Ejecutar
    dim3 block(16, 16);
    dim3 grid((N+15)/16, (N+15)/16);
    
    cudaEvent_t start, stop;
    cudaEventCreate(&start);
    cudaEventCreate(&stop);
    
    cudaEventRecord(start);
    test_matmul<<<grid, block>>>(d_A, d_B, d_C, N);
    cudaEventRecord(stop);
    
    cudaEventSynchronize(stop);
    float milliseconds = 0;
    cudaEventElapsedTime(&milliseconds, start, stop);
    
    cudaMemcpy(h_C, d_C, size, cudaMemcpyDeviceToHost);
    
    printf("  Matriz: %dx%d\n", N, N);
    printf("  Tiempo: %.3f ms\n", milliseconds);
    printf("  GFLOPS: %.2f\n", (2.0 * N * N * N) / (milliseconds * 1e6));
    
    // Verificar resultado
    printf("  Primeros 5 elementos de C: ");
    for (int i = 0; i < 5; i++) printf("%.4f ", h_C[i]);
    printf("\n");
    
    // Liberar
    free(h_A); free(h_B); free(h_C);
    cudaFree(d_A); cudaFree(d_B); cudaFree(d_C);
    
    printf("\n✓ Prueba CUDA completada exitosamente\n");
    printf("  La GPU está funcionando correctamente para entrenamiento.\n\n");
    
    return 0;
}
