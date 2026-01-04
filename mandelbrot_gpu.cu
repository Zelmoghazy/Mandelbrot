#include <cuda_runtime.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

__global__ void mandelbrot_kernel(
    uint32_t* output,
    uint32_t width,
    uint32_t height,
    double center_x,
    double center_y,
    double scale,
    int max_iterations)
{
    uint32_t x = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t y = blockIdx.y * blockDim.y + threadIdx.y;
    
    if (x >= width || y >= height) return;
    
    double c_re = center_x + (x - width / 2.0) * scale;
    double c_im = center_y + (y - height / 2.0) * scale;
    
    double z_re = 0.0;
    double z_im = 0.0;

    int iteration = 0;

    const double limit = 4.0;
    
    while (iteration < max_iterations) 
    {
        double re_tmp = z_re*z_re - z_im*z_im + c_re;
        z_im = 2.0 * z_re * z_im + c_im;
        z_re = re_tmp;
        iteration++;
        
        if (z_re*z_re + z_im*z_im > limit) break;
        
        re_tmp = z_re*z_re - z_im*z_im + c_re;
        z_im = 2.0 * z_re * z_im + c_im;
        z_re = re_tmp;
        iteration++;
        
        if (z_re*z_re + z_im*z_im > limit) break;
    }
    
    uint32_t color;
    if (iteration == max_iterations) {
        color = 0xFF000000; // ARGB
    } else {
        float smooth_iter = (float)iteration - log2f(log2f(sqrtf((float)(z_re*z_re + z_im*z_im)))) + 4.0f;
        float t = smooth_iter / (float)max_iterations;
        
        uint8_t r = (uint8_t)(9.0f * (1.0f-t) * t * t * t * 255.0f);
        uint8_t g = (uint8_t)(15.0f * (1.0f-t) * (1.0f-t) * t * t * 255.0f);
        uint8_t b = (uint8_t)(8.5f * (1.0f-t) * (1.0f-t) * (1.0f-t) * t * 255.0f);
        
        color = 0xFF000000 | (r << 16) | (g << 8) | b;
    }
    
    output[y * width + x] = color;
}

extern "C" {

uint32_t* d_output;
static size_t g_allocated_size = 0;

void cuda_init(uint32_t max_width, uint32_t max_height)
{
    size_t max_size = max_width * max_height * sizeof(uint32_t);
    cudaMalloc(&d_output, max_size);
    g_allocated_size = max_size;
}

void cuda_cleanup(void) 
{
    if (d_output) {
        cudaFree(d_output);
        d_output = NULL;
        g_allocated_size = 0;
    }
}

void cuda_render_mandelbrot(
    uint32_t* output,
    uint32_t width,
    uint32_t height,
    double center_x,
    double center_y,
    double scale,
    int max_iterations)
{
    size_t pixel_count = width * height;
    size_t buffer_size = pixel_count * sizeof(uint32_t);

    if (buffer_size > g_allocated_size) {
        if (d_output) cudaFree(d_output);
        cudaMalloc(&d_output, buffer_size);
        g_allocated_size = buffer_size;
    }
    
    // GTX 1060 -> max 1024 threads per block, warp size = 32 threads
    dim3 block_size(16,16);  // 256 threads per block
    dim3 grid_size(
        (width + block_size.x - 1) / block_size.x,
        (height + block_size.y - 1) / block_size.y
    );
    
    mandelbrot_kernel<<<grid_size, block_size>>>(
        d_output, width, height,
        center_x, center_y, scale,
        max_iterations
    );
    
    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        printf("CUDA kernel error: %s\n", cudaGetErrorString(err));
        cudaFree(d_output);
        return;
    }
    
    cudaDeviceSynchronize();
    
    cudaMemcpy(output, d_output, buffer_size, cudaMemcpyDeviceToHost);
}

int cuda_is_available()
{
    int device_count = 0;
    cudaError_t err = cudaGetDeviceCount(&device_count);
    return (err == cudaSuccess && device_count > 0);
}

void cuda_print_info()
{
    int device_count = 0;
    cudaGetDeviceCount(&device_count);
    
    if (device_count == 0) {
        printf("No CUDA devices found\n");
        return;
    }
    
    printf("Found %d CUDA device(s)\n", device_count);
    
    for (int i = 0; i < device_count; i++) {
        cudaDeviceProp prop;
        cudaGetDeviceProperties(&prop, i);
        
        printf("Device %d: %s\n", i, prop.name);
        printf("  Compute Capability: %d.%d\n", prop.major, prop.minor);
        printf("  Total Memory: %.2f GB\n", prop.totalGlobalMem / (1024.0*1024.0*1024.0));
        printf("  Multiprocessors: %d\n", prop.multiProcessorCount);
        printf("  Max Threads per Block: %d\n", prop.maxThreadsPerBlock);
    }
}

} // extern "C"