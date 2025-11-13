#ifndef GR_H_
#define GR_H_

#include "app_api.h" 

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#define IS_OPAQUE(c) (c.a == 255)
#define IS_INVIS(c)  (c.a == 0)

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define ClampTop(A, X) MIN(A, X)
#define Clamp(A, X, B)          (((X) < (A)) ? (A) : ((X) > (B)) ? (B) : (X))
#define CEIL_DIV(x, y)                  (((x) + (y) - 1) / (y))

#define LERP_F32(start, end, t) ((float) ((start) + ((end) - (start)) * (t)))

#define SWAP(x, y, T) \
    do {              \
        T tmp = (x);  \
        (x) = (y);    \
        (y) = tmp;    \
    } while (0)

float d_sqrt(float number); 
float gradient_noise(float x, float y);
color_t make_color(uint8_t r, uint8_t g, uint8_t b); 
color_t rgb_from_wavelength(double wave); 
void set_pixel(platform_api_t *platform, int x, int y, color_t color);
color_t blend_pixel(color_t dst, color_t src);
void set_pixel_blend(platform_api_t *platform, int x, int y,color_t color);
void set_pixel_weighted(platform_api_t *platform, int x, int y, color_t color, int weight);
void draw_line(platform_api_t *platform, int x0, int y0, int x1, int y1, color_t color);
void draw_hline(platform_api_t *platform, int y, int x0, int x1, color_t const color); 
void draw_vline(platform_api_t *platform, int x, int y0, int y1, color_t const color); 
void draw_aaline(platform_api_t *platform, int x0, int y0, int x1, int y1, color_t color, bool draw_endpoint);
void draw_thick_line(platform_api_t *platform, int x1, int y1, int x2, int y2, int width, color_t color);
void draw_thick_aaline(platform_api_t *platform, int x1, int y1, int x2, int y2, int width, color_t color);
void draw_rect(platform_api_t *platform, int x, int y, int w, int h, color_t color);
void draw_circle_outline(platform_api_t *platform, int cx, int cy, int radius, color_t color); 
void draw_circle_filled(platform_api_t *platform, int cx, int cy, int radius, color_t color);
void plot_4_points(platform_api_t *platform, int cx, int cy, int x, int y, float opacity, color_t color); 
void draw_circle_aa(platform_api_t *platform, int cx, int cy, float radius, color_t color); 
void draw_circle_filled_aa(platform_api_t *platform, int cx, int cy, float radius, color_t color); 
void draw_ellipse_filled(platform_api_t *platform, int cx, int cy, int rx, int ry, color_t color); 
void draw_filled_polygon(platform_api_t *platform, const int *vx, const int *vy, int n, color_t color);
void clear_screen(platform_api_t *platform);

#endif