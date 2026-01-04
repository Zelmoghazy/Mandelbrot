#define BUILD_DLL

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#include "app_api.h"

#define PROF_IMPLEMENTATION
#include "prof.h"

#include "simple_font.h"
#include "simple_font.c"

#include "gr.h"
#include "gr.c"

#include "util.h"
#include "util.c"

#ifdef USE_CUDA
#include "mandelbrot_gpu.h"
#endif

simple_font_t* simple_font = NULL;
color_t color_map[512];

#define SCREEN_TO_COMPLEX(screen, center, screen_size, scale) \
    ((center) + ((screen) - (screen_size) / 2.0) * (scale))

#define LOCAL_TO_COMPLEX(pixel, window, window_size, scale) \
    (((pixel) - (window) - (window_size) / 2.0) * (scale))

#define COMPLEX_TO_SCREEN(complex, center, screen_size, scale) \
    ((int)(((complex) - (center)) / (scale) + (screen_size) / 2.0))

#define REANCHOR_VIEW(fixed_world_coord, screen_coord, screen_size, new_scale)\
   ((fixed_world_coord) - ((screen_coord) - (screen_size)/2.0) * (new_scale))


typedef enum {
    COLOR_GRAYSCALE,
    COLOR_RAINBOW_1,
    COLOR_RAINBOW_2,
    COLOR_BLUE,
    COLOR_NEON,
    COLOR_ULTRA,
    COLOR_SPECTRAL
}color_palette_t;

color_t get_color(int iterations, int max_iterations, color_palette_t palette) 
{
    if (iterations == max_iterations) {
        return make_color(0, 0, 0);  
    }

    switch(palette)
    {
        case COLOR_GRAYSCALE:
        {
            uint8_t brightness = (iterations * 255) / max_iterations;
            return make_color(brightness, brightness, brightness);
        }
        case COLOR_RAINBOW_1:
        {
            int hue = (iterations * 16) % 256;
            if (hue < 64) {
                return make_color(255, hue * 4, 0);   
            } else if (hue < 128) {
                return make_color(255 - (hue - 64) * 4, 255, 0);  
            } else if (hue < 192) {
                return make_color(0, 255, (hue - 128) * 4);  
            } else {
                return make_color(0, 255 - (hue - 192) * 4, 255); 
            }
        }
        case COLOR_RAINBOW_2:
        {
            float hue = (float)iterations / (float)max_iterations * 360.0f;
            float h = hue / 60.0f;
            int i = (int)h;
            float f = h - i;
            float p = 0.0f;
            float q = 1.0f - f;
            float t = f;
            float r, g, b;
            switch (i % 6) {
                case 0: r = 1; g = t; b = p; break;
                case 1: r = q; g = 1; b = p; break;
                case 2: r = p; g = 1; b = t; break;
                case 3: r = p; g = q; b = 1; break;
                case 4: r = t; g = p; b = 1; break;
                case 5: r = 1; g = p; b = q; break;
            }
            return make_color((uint8_t)(r * 255), (uint8_t)(g * 255), (uint8_t)(b * 255));
        }
        case COLOR_BLUE:
        {
            // Continuous Dwell
            float smooth_iter = (float)iterations - log2f(log2f(sqrtf(iterations * iterations))) + 4.0f;
            float t = smooth_iter / (float)max_iterations;
            
            uint8_t r = (uint8_t)(9 * (1-t) * t * t * t * 255);
            uint8_t g = (uint8_t)(15 * (1-t) * (1-t) * t * t * 255);
            uint8_t b = (uint8_t)(8.5 * (1-t) * (1-t) * (1-t) * t * 255);
            
            return make_color(r, g, b);
        }
        case COLOR_NEON:
        {
            float t = (float)iterations / (float)max_iterations;
            
            uint8_t r = (uint8_t)(sinf(t * 6.28f + 0.0f) * 127 + 128);
            uint8_t g = (uint8_t)(sinf(t * 6.28f + 2.09f) * 127 + 128);
            uint8_t b = (uint8_t)(sinf(t * 6.28f + 4.18f) * 127 + 128);
            
            return make_color(r, g, b);
        }
        case COLOR_ULTRA:
        {
            float t = (float)iterations / (float)max_iterations;
            
            if (t < 0.16f) {
                uint8_t val = (uint8_t)(t * 255 * 6.25f);
                return make_color(0, val, val * 2);
            } else if (t < 0.33f) {
                uint8_t val = (uint8_t)((t - 0.16f) * 255 * 5.88f);
                return make_color(0, 255, 255 - val);
            } else if (t < 0.5f) {
                uint8_t val = (uint8_t)((t - 0.33f) * 255 * 5.88f);
                return make_color(val, 255, 0);
            } else if (t < 0.66f) {
                uint8_t val = (uint8_t)((t - 0.5f) * 255 * 6.25f);
                return make_color(255, 255 - val, 0);
            } else if (t < 0.83f) {
                uint8_t val = (uint8_t)((t - 0.66f) * 255 * 5.88f);
                return make_color(255, 0, val);
            } else {
                uint8_t val = (uint8_t)((t - 0.83f) * 255 * 5.88f);
                return make_color(255, val, 255);
            }
        }
        case COLOR_SPECTRAL:
        {
            return color_map[iterations % 512];
        }
    }
}

/*
    From wikipedia:
        "In mathematics, an iterated function is a function that is obtained by composing another function
         with itself two or several times. The process of repeatedly applying the same function is called iteration
         In this process, starting from some initial object, the result of applying a given function is fed again 
         into the function as input, and this process is repeated.."

    We will take the function :
        f_c(z) = {z}^2 + c 

    Given a certain complex number "c" we start iterating it repeatedly starting from 0 -> f(0),f(f(0)),...
​
    For example: 

    at c = 1 
    - f_1(0) = 0 + 1 = 1 (plug the output back)
    - f_1(1) = 1 + 1 = 2
    - f_1(2) = 4 + 1 = 5
    - f_1(5) = 25 + 1 = 26
    - and it increases to infinity (blows up)

    at c = -1
    - f_{-1}(0) = 0² + (-1) = -1
    - f_{-1}(-1) = (-1)² + (-1) = 1 - 1 = 0
    - f_{-1}(0) = 0² + (-1) = -1
    - f_{-1}(-1) = (-1)² + (-1) = 0
    - and continues to cycle -1 -> 0
    ---------------------------------------
    In conclusion the behaviour is either the values under iteration 
    1- blows up
    2- stay bounded -> the mandelbrot set

    Formal definition :
        A complex number is a member of the mandelbrot set if when starting
        with z=0 and applying the iteration repeatedly the absolute value of z
        stays bounded for all numbers
*/
void render_mandelbrot_simple(platform_api_t *platform, double center_x, double center_y, double scale, int max_iterations) 
{
    const double limit = 4.0;  // (we cannot get past the escape radius = 2.0 so no need to calculate further (squared so we dont deal with sqrt))
    
    int width = platform->screen_width;
    int height = platform->screen_height;
    
    for (int py = 0; py < height; py++) 
    {
        for (int px = 0; px < width; px++) 
        {
            
            double c_re = SCREEN_TO_COMPLEX(px,center_x,width,scale);
            double c_im = SCREEN_TO_COMPLEX(py,center_y,height,scale);
            
            double z_re = 0.0;
            double z_im = 0.0;

            int iteration = 0;

            /*
                Remember :
                - i^2 = -1

                - (z_re + z_img i)^2 = z_re^2 - z_im^2 + {(2 * z_re * z_img) i}

                - z^2 + c = (z_re^2 - z_im^2 + c_re) + (2 * z_re * z_img + c_im) i 
             */
            while (iteration < max_iterations) 
            {
                double re_tmp = z_re*z_re - z_im*z_im + c_re;
                z_im = 2 * z_re * z_im + c_im;
                z_re = re_tmp;
                iteration++;

                // distance larger than escape radius abort
                if (z_re*z_re + z_im*z_im > limit) 
                    break;
                
                // unrolled will it do anything ? please auto vectorize
                re_tmp = z_re*z_re - z_im*z_im + c_re;
                z_im = 2*z_re*z_im + c_im;
                z_re = re_tmp;
                iteration++;

                if (z_re*z_re + z_im*z_im > limit) 
                    break;
            }
            
            color_t color = get_color(iteration, max_iterations, COLOR_BLUE);
            set_pixel(platform, px, py, color);
        }
    }
}

/*
    In the julia set the parameter "c" is fixed, and the initial object varies across the complex plane
 */

void render_julia_set(platform_api_t *platform, double cx, double cy, int x, int y, int width, int height, int max_iterations) 
{
    const double limit = 4.0;
    
    double c_re = cx;
    double c_im = cy;
    
    double scale = 4.0 / width;
    
    for (int py = y; py < y + height; py++) 
    {
        for (int px = x; px < x + width; px++) 
        {
            /* 
                initial object varies and c is const
             */
            double z_re = LOCAL_TO_COMPLEX(px, x, width, scale);
            double z_im = LOCAL_TO_COMPLEX(py, y, height, scale);
            
            int iteration = 0;
            
            while (iteration < max_iterations) 
            {
                double z_re_temp = z_re*z_re - z_im*z_im + c_re;
                z_im = 2*z_re*z_im + c_im;
                z_re = z_re_temp;
                iteration++;
                if (z_re*z_re + z_im*z_im > limit) 
                    break;
                
                z_re_temp = z_re*z_re - z_im*z_im + c_re;
                z_im = 2*z_re*z_im + c_im;
                z_re = z_re_temp;
                iteration++;
                if (z_re*z_re + z_im*z_im > limit) 
                    break;
            }
            
            color_t color = get_color(iteration, max_iterations,COLOR_BLUE);
            set_pixel(platform, px, py, color);
        }
    }
    
    color_t border_color = {27, 27, 28, 255};
    int l_width = 10;
    draw_thick_line(platform, x-l_width/2, y, x+width+l_width/2, y,l_width,border_color) ;
    draw_thick_line(platform, x-l_width/2, y+height, x+width+l_width/2, y+height,l_width,border_color) ;
    draw_thick_line(platform, x, y, x, y+height,l_width,border_color);
    draw_thick_line(platform, x+width+l_width/4, y,x+width+l_width/4, y+height,l_width,border_color) ;
}

void visualize_mandelbrot_iterations(platform_api_t *platform, double cx, double cy,  double m_cx, double m_cy, double m_s) 
{
    const int max_iterations = 10;
    
    int c_x = COMPLEX_TO_SCREEN(cx, m_cx, platform->screen_width, m_s);
    int c_y = COMPLEX_TO_SCREEN(cy, m_cy, platform->screen_height, m_s);
    
    color_t c_color = {255, 255, 0, 255};
    draw_circle_filled_aa(platform, c_x, c_y, 6, c_color);
    
    draw_line(platform, c_x - 8, c_y, c_x + 8, c_y, c_color);
    draw_line(platform, c_x, c_y - 8, c_x, c_y + 8, c_color);
    
    double z_re = 0.0;
    double z_im = 0.0;
    
    color_t iteration_colors[] = {
        {255, 0, 0, 255},     
        {255, 128, 0, 255},   
        {255, 255, 0, 255},   
        {128, 255, 0, 255},   
        {0, 255, 0, 255},     
        {0, 255, 128, 255},   
        {0, 255, 255, 255},   
        {0, 128, 255, 255},   
        {0, 0, 255, 255},     
        {128, 0, 255, 255}    
    };
    
    int z0_x = COMPLEX_TO_SCREEN(0.0, m_cx, platform->screen_width, m_s);
    int z0_y = COMPLEX_TO_SCREEN(0.0, m_cy, platform->screen_height, m_s);
    
    color_t z0_color = {255, 255, 255, 255};
    draw_circle_filled_aa(platform, z0_x, z0_y, 4, z0_color);
    
    for (int i = 0; i < max_iterations; i++) 
    {
        double z_re_sq = z_re * z_re;
        double z_im_sq = z_im * z_im;
        double new_z_re = z_re_sq - z_im_sq + cx;
        double new_z_im = 2 * z_re * z_im + cy;
        
        int start_x = COMPLEX_TO_SCREEN(z_re, m_cx, platform->screen_width, m_s);
        int start_y = COMPLEX_TO_SCREEN(z_im, m_cy, platform->screen_height, m_s);
        int end_x = COMPLEX_TO_SCREEN(new_z_re, m_cx, platform->screen_width, m_s);
        int end_y = COMPLEX_TO_SCREEN(new_z_im, m_cy, platform->screen_height, m_s);
        
        color_t line_color = iteration_colors[i % 10];
        
        if (abs(start_x - end_x) < platform->screen_width * 2 && 
            abs(start_y - end_y) < platform->screen_height * 2) 
        {
            draw_thick_aaline(platform, start_x, start_y, end_x, end_y, 2, line_color);
        }
        
        if (start_x >= 0 && start_x < (int)platform->screen_width &&
            start_y >= 0 && start_y < (int)platform->screen_height) 
        {
            draw_circle_filled_aa(platform, start_x, start_y, 3, line_color);
        }
        
        z_re = new_z_re;
        z_im = new_z_im;
        
        if (z_re * z_re + z_im * z_im > 4.0) 
        {
            if (end_x >= 0 && end_x < (int)platform->screen_width &&
                end_y >= 0 && end_y < (int)platform->screen_height) 
            {
                color_t escape_color = {255, 0, 0, 255};
                draw_circle_filled_aa(platform, end_x, end_y, 8, escape_color);
                draw_line(platform, end_x - 5, end_y - 5, end_x + 5, end_y + 5, escape_color);
                draw_line(platform, end_x - 5, end_y + 5, end_x + 5, end_y - 5, escape_color);
            }
            break;
        }
    }
    
    int final_x = COMPLEX_TO_SCREEN(z_re, m_cx, platform->screen_width, m_s);
    int final_y = COMPLEX_TO_SCREEN(z_im, m_cy, platform->screen_height, m_s);
    
    if (final_x >= 0 && final_x < (int)platform->screen_width &&
        final_y >= 0 && final_y < (int)platform->screen_height) 
    {
        color_t final_color = {255, 255, 255, 255};
        draw_circle_filled_aa(platform, final_x, final_y, 5, final_color);
    }
}

void render_complex_plane(platform_api_t *platform, double m_cx, double m_cy, double m_s) 
{
    color_t axis_color = {255, 255, 255, 255};
    draw_line(platform, 0, COMPLEX_TO_SCREEN(0.0, m_cy, platform->screen_height, m_s), platform->screen_width, COMPLEX_TO_SCREEN(0.0, m_cy, platform->screen_height, m_s), axis_color);  
    draw_line(platform, COMPLEX_TO_SCREEN(0.0, m_cx, platform->screen_width, m_s), 0, COMPLEX_TO_SCREEN(0.0, m_cx, platform->screen_width, m_s), platform->screen_height, axis_color); 

    color_t grid_color = {255, 255, 255, 64};
    double grid_spacing = 0.1; 
    
    if (m_s < 0.001) {
        grid_spacing = 0.01;
        grid_color.a = 32;
    }
    else if (m_s < 0.01) 
        grid_spacing = 0.05; 
    else if (m_s < 0.1) 
        grid_spacing = 0.1;
    else 
        grid_spacing = 0.2;
    
    for (double val = -2.0; val <= 2.0; val += grid_spacing) 
    {
        if (fabs(val) < 0.0001) continue; 
        
        int x = COMPLEX_TO_SCREEN(val, m_cx, platform->screen_width, m_s);
        int y = COMPLEX_TO_SCREEN(val, m_cy, platform->screen_height, m_s);
        
        if (x >= -100 && x <= platform->screen_width + 100) {
            draw_line(platform, x, 0, x, platform->screen_height, grid_color);
        }
        if (y >= -100 && y <= platform->screen_height + 100) {
            draw_line(platform, 0, y, platform->screen_width, y, grid_color);
        }
    }
    
    double escape_radius = 2.0;
    int circle_radius = (int)(escape_radius / m_s);
    if (circle_radius < platform->screen_width * 2) 
    {
        color_t circle_color = {255, 255, 255, 255};
        int circle_center_x = COMPLEX_TO_SCREEN(0.0, m_cx, platform->screen_width, m_s);
        int circle_center_y = COMPLEX_TO_SCREEN(0.0, m_cy, platform->screen_height, m_s);
        draw_circle_aa(platform, circle_center_x, circle_center_y, circle_radius, circle_color);
    }
}

typedef struct 
{
    u32 start_x, end_x;
    u32 start_y, end_y;
    u32 width, height;
    platform_api_t *platform;
    int max_iterations;
    double center_x;
    double center_y;
    double scale;
} tile_data_t;

thread_func_ret_t render_tile(thread_func_param_t data) 
{
    tile_data_t *tile = (tile_data_t *)data;

    const double limit = 4.0;      // (we cannot get past the escape radius so no need to calculate further (distance sqrt no need))
    
    for (u32 y = tile->start_y; y < tile->end_y; ++y) 
    {
        for (u32 x = tile->start_x; x < tile->end_x; ++x) 
        {
            double c_re = SCREEN_TO_COMPLEX(x, tile->center_x, tile->platform->screen_width, tile->scale);
            double c_im = SCREEN_TO_COMPLEX(y, tile->center_y, tile->platform->screen_height, tile->scale);
            
            double z_re = 0.0;
            double z_im = 0.0;

            int iteration = 0;

            while (iteration < tile->max_iterations) 
            {
                double re_tmp = z_re*z_re - z_im*z_im + c_re;
                z_im = 2 * z_re * z_im + c_im;
                z_re = re_tmp;
                iteration++;

                if (z_re*z_re + z_im*z_im > limit) 
                    break;
                
                re_tmp = z_re*z_re - z_im*z_im + c_re;
                z_im = 2*z_re*z_im + c_im;
                z_re = re_tmp;
                iteration++;

                if (z_re*z_re + z_im*z_im > limit) 
                    break;
            }
            
            color_t color = get_color(iteration, tile->max_iterations, COLOR_BLUE);
            set_pixel(tile->platform, x, y, color);

/*             if(y == tile->start_y ||  y == tile->end_y-1)
                set_pixel_blend(tile->platform, x, y, (color_t){255,255,255,64});

            if(x == tile->start_x ||  x == tile->end_x-1)
                set_pixel_blend(tile->platform, x, y, (color_t){255,255,255,64}); */

        }
    }

    #ifdef _WIN32
        return 0;
    #else
        return NULL;
    #endif
}

void render_mandelbrot_parallel(platform_api_t *platform, double center_x, double center_y, double scale, int max_iterations)
{
    u32 height  = platform->screen_height;
    u32 width   = platform->screen_width;

    i32 num_threads = get_core_count();

    // slice the screen up to 64x64 px tiles
    const u32 tile_size = 64;

    u32 tiles_x = CEIL_DIV(width, tile_size);
    u32 tiles_y = CEIL_DIV(height, tile_size);
    u32 total_tiles = tiles_x * tiles_y;

    tile_data_t* tiles = malloc(total_tiles * sizeof(tile_data_t));
    thread_handle_t* threads = malloc(num_threads * sizeof(thread_handle_t));

    i32 tile_idx = 0;
    
    for (u32 ty = 0; ty < tiles_y; ty++) 
    {
        u32 start_y = ty * tile_size;
        u32 end_y = MIN(height, start_y + tile_size);
        
        for (u32 tx = 0; tx < tiles_x; tx++) 
        {
            u32 start_x = tx * tile_size;
            u32 end_x = MIN(width, (start_x + tile_size)); 
            
            tiles[tile_idx] = (tile_data_t){
                .start_x = start_x, .end_x = end_x,
                .start_y = start_y, .end_y = end_y,
                .width = width, .height = height,
                .platform = platform,
                .max_iterations = max_iterations,
                .center_x = center_x,
                .center_y = center_y,
                .scale  = scale
            };
            
            int thread_slot = tile_idx % num_threads;
            
            if (tile_idx >= num_threads) {
                PROFILE("Waiting for a slot"){
                    join_thread(threads[thread_slot]);
                }
            }
            
            PROFILE("Actually creating a thread, does it matter ?")
            {
                threads[thread_slot] = create_thread(render_tile, &tiles[tile_idx]);
            }
            
            tile_idx++;
        }
    }

    // total num of tiles maybe less than number of threads created 
    int remaining_threads = MIN(total_tiles, num_threads);

    PROFILE("Waiting to join")
    {
        for (int i = 0; i < remaining_threads; i++) {
            join_thread(threads[i]);
        }
    }

    free(tiles);
    free(threads);
}

void render_mandelbrot_gpu(platform_api_t *platform, double center_x, double center_y, 
                           double scale, int max_iterations) 
{
    #ifdef USE_CUDA
        cuda_render_mandelbrot(
            platform->pixels,
            platform->screen_width,
            platform->screen_height,
            center_x,
            center_y,
            scale,
            max_iterations
        );
    #else
        render_mandelbrot_parallel(platform, center_x, center_y, scale, max_iterations);
    #endif
}

EXPORT void app_init(platform_api_t *platform, app_state_t *state) 
{
    (void) platform;

    state->initialized = true;
    state->animation_time = 0.0f;
    state->frame_count = 0;

    state->view_center_x = -0.637011;
    state->view_center_y = -0.0395159;
    state->view_scale = 0.002;
    state->target_scale = 0.002;

    state->is_panning = false;
    state->pan_start_x = 0;
    state->pan_start_y = 0;
    state->pan_start_center_x = 0.0;
    state->pan_start_center_y = 0.0;

    state->last_mouse_x = 0;
    state->last_mouse_y = 0;

    prof_init();

    simple_font = init_simple_font((u32*)font_pixels);

    for (int i = 0; i < 512; ++i){
        color_map[i] = rgb_from_wavelength(380.0 + (i * 400.0 / 512));
    }

    #ifdef USE_CUDA
        if (cuda_is_available()) 
        {
            printf("[GPU] CUDA is available!\n");
            cuda_print_info();
            cuda_init(1920,1080);
            state->use_gpu = true;
        } 
        else 
        {
            printf("[GPU] CUDA not available, using CPU fallback\n");
            state->use_gpu = false;
        }
    #else
        state->use_gpu = false;
        printf("[GPU] Not compiled with CUDA support\n");
    #endif


    printf("[APP] Initialized!\n");
}

EXPORT void app_update(platform_api_t *platform, app_state_t *state) 
{
    state->animation_time += (float) platform->dt;
    state->frame_count++;
    
    if (platform->keys_pressed[256]) { 
        platform->should_quit = true;
    }
    
    if (platform->keys_pressed[301]) { 
        platform->capture_frame = true;
    }

    if (platform->scroll_y != 0) 
    {
        double zoom_factor = 1.0 + (platform->scroll_y * 0.1);

        double mouse_cx = SCREEN_TO_COMPLEX(platform->mouse_x, state->view_center_x, platform->screen_width, state->view_scale);
        double mouse_cy = SCREEN_TO_COMPLEX(platform->mouse_y, state->view_center_y, platform->screen_height, state->view_scale);
        
        state->view_scale *= zoom_factor;

        state->view_center_x = REANCHOR_VIEW(mouse_cx, platform->mouse_x, platform->screen_width, state->view_scale);
        state->view_center_y = REANCHOR_VIEW(mouse_cy, platform->mouse_y, platform->screen_height, state->view_scale);
    }

    bool pan_button = platform->right_button_down;

    if (pan_button && !state->is_panning) 
    {
        state->is_panning = true;
        state->pan_start_x = platform->mouse_x;
        state->pan_start_y = platform->mouse_y;
        state->pan_start_center_x = state->view_center_x;
        state->pan_start_center_y = state->view_center_y;
    } 
    else if (!pan_button && state->is_panning) 
    {
        state->is_panning = false;
    }

    if (state->is_panning) 
    {
        int dx = platform->mouse_x - state->pan_start_x;
        int dy = platform->mouse_y - state->pan_start_y;
        
        state->view_center_x = state->pan_start_center_x - dx * state->view_scale;
        state->view_center_y = state->pan_start_center_y - dy * state->view_scale;
    }

    if (platform->keys_pressed['R']) {
        state->view_center_x = -0.637011;
        state->view_center_y = -0.0395159;
        state->view_scale = 0.002;
        state->target_scale = 0.002;
    }

    if (platform->keys_pressed['G']) 
    {
        #ifdef USE_CUDA
            state->use_gpu = !state->use_gpu;
            printf("Switched to %s rendering\n", state->use_gpu ? "GPU" : "CPU");
        #else
            printf("Not compiled with CUDA support\n");
        #endif
    }

    state->last_mouse_x = platform->mouse_x;
    state->last_mouse_y = platform->mouse_y;
}

EXPORT void app_render(platform_api_t *platform, app_state_t *state) 
{
    float t = state->animation_time;

    double current_scale = state->view_scale;           
    double current_center_x = state->view_center_x;
    double current_center_y = state->view_center_y;
    int max_iterations = 1024;
    
    clear_screen(platform);

    #ifdef USE_CUDA
        if (state->use_gpu) 
        {
            PROFILE("mandelbrot_gpu") 
            {
                render_mandelbrot_gpu(platform, current_center_x, current_center_y, 
                                    current_scale, max_iterations);
            }
        } 
        else
    #endif
        {
            PROFILE("mandelbrot_cpu") 
            {
                render_mandelbrot_parallel(platform, current_center_x, current_center_y, 
                                        current_scale, max_iterations);
            }
        }

    double mouse_cx = SCREEN_TO_COMPLEX(platform->mouse_x, current_center_x, platform->screen_width, current_scale);
    double mouse_cy = SCREEN_TO_COMPLEX(platform->mouse_y, current_center_y, platform->screen_height, current_scale);

    if(current_scale > 0.0001 && current_scale < 0.003)
    {
        // render_complex_plane(platform, current_center_x, current_center_y, current_scale);
        // visualize_mandelbrot_iterations(platform, mouse_cx, mouse_cy, current_center_x, current_center_y, current_scale);

        // static char coord_text[64];
        // snprintf(coord_text, sizeof(coord_text), "(%.2f %+.2fi)", mouse_cx, mouse_cy);
        // rendered_text_t text = {
        //     .font = simple_font,  
        //     .string = coord_text,
        //     .size = strlen(coord_text),
        //     .pos = { platform->mouse_x + 15, platform->mouse_y - 15 }, 
        //     .color = { 255, 255, 255, 255 }, 
        //     .scale = 2
        // };
        // render_text(platform, &text);
    }
    
    int julia_width = platform->screen_width / 4;
    int julia_height = platform->screen_height / 4;
    int julia_x = 10;  
    int julia_y = 10;  
    
    PROFILE("julia_set") {
        // render_julia_set(platform, mouse_cx, mouse_cy, julia_x, julia_y, julia_width, julia_height, 64);
    }

    static char time_text[64];
    snprintf(time_text, sizeof(time_text), "%.2f ms", platform->dt*1000);
    rendered_text_t delta_time_text = {
        .font = simple_font,  
        .string = time_text,
        .size = strlen(time_text),
        .pos = { platform->screen_width - 180,15 },
        .color = { 255, 255, 255, 255 },
        .scale = 2
    };
    
    render_text(platform, &delta_time_text);
    
    // static char info[128];
    // snprintf(info, sizeof(info), "c_x: %.2f\nc_y: %.2f\nc_s: %.4f\nc_iter: %d\n", current_center_x, current_center_y,current_scale, max_iterations);
    // rendered_text_t info_text = {
    //     .font = simple_font,  
    //     .string = info,
    //     .size = strlen(info),
    //     .pos = {15 , platform->screen_height*0.85}, 
    //     .color = { 255, 255, 255, 255 }, 
    //     .scale = 1
    // };    
    // render_text(platform, &info_text);

    static char backend_text[32];
    #ifdef USE_CUDA
        snprintf(backend_text, sizeof(backend_text), "%s", state->use_gpu ? "GPU" : "CPU");
    #else
        snprintf(backend_text, sizeof(backend_text), "CPU");
    #endif
    rendered_text_t backend = {
        .font = simple_font,  
        .string = backend_text,
        .size = strlen(backend_text),
        .pos = { platform->screen_width - 180, 45 },
        .color = { 255, 255, 0, 255 },
        .scale = 2
    };
    render_text(platform, &backend);

    prof_sort_results();
    // prof_print_results();
    prof_reset();
}

EXPORT void app_cleanup(platform_api_t *platform, app_state_t *state) 
{
    (void) platform;
    (void) state;

    #ifdef USE_CUDA
        cuda_cleanup(); 
    #endif

    printf("Cleanup called (before reload/exit)\n");
}

EXPORT void app_on_reload(platform_api_t *platform, app_state_t *state) 
{
    (void) platform;
    
    if(simple_font){
        free(simple_font);
    }
    simple_font = init_simple_font((u32*)font_pixels);
    for (int i = 0; i < 512; ++i){
        color_map[i] = rgb_from_wavelength(380.0 + (i * 400.0 / 512));
    }
    #ifdef USE_CUDA
        cuda_init(1920,1080); 
    #endif

    printf("Reloaded! State preserved: frame=%d, time=%.2f\n", state->frame_count, state->animation_time);
}