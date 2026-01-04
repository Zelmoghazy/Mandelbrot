#ifndef MANDELBROT_GPU_H
#define MANDELBROT_GPU_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void cuda_render_mandelbrot(
    uint32_t* output,
    uint32_t width,
    uint32_t height,
    double center_x,
    double center_y,
    double scale,
    int max_iterations);

void cuda_init(uint32_t max_width, uint32_t max_height);
void cuda_cleanup(void);
int cuda_is_available(void);
void cuda_print_info(void);

#ifdef __cplusplus
}
#endif

#endif // MANDELBROT_GPU_H