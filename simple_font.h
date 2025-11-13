#ifndef BASE_S_FONT
#define BASE_S_FONT



#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "app_api.h"

typedef uint8_t     u8;
typedef int8_t      i8;

typedef uint16_t    u16;
typedef int16_t     i16;

typedef uint32_t    u32;
typedef int32_t     i32;

typedef uint64_t    u64;
typedef int64_t     i64;

typedef float       f32;
typedef double      f64;

typedef struct {
    i32 x, y, w, h;
}rect_t;


typedef struct vec2_t{
    i32 x,y;
}vec2_t;

#define A(c)           ((u8) ((c) >> 24))
#define R(c)           ((u8) ((c) >> 16))
#define G(c)           ((u8) ((c) >>  8))
#define B(c)           ((u8)  (c)       )

#define HEX_TO_RGBA(s,hex)  \
    s.r = (R(hex) & 0xFF);  \
    s.g = (G(hex) & 0xFF);  \
    s.b = (B(hex) & 0xFF);  \
    s.a = 255     

/*
    Quick way to get debug ascii text on screen without having 
    to deal with any external library or font files
 */

#define FONT_ROWS           6   
#define FONT_COLS           18

#define ASCII_LOW           32
#define ASCII_HIGH          126

extern const uint8_t font_pixels[];

typedef struct
{
    u32 const       *font_pixels;
    u32              font_width;
    u32              font_height;
    u32              font_char_width;
    u32              font_char_height;
    rect_t           glyph_table[ASCII_HIGH - ASCII_LOW + 1];
}simple_font_t;

typedef struct
{
    simple_font_t    *font;
    char             *string;
    u32              size;
    vec2_t           pos;       
    color_t         color;     
    u32              scale;     
}rendered_text_t;

void render_glyph_to_buffer(rendered_text_t *text, u32 glyph_idx,
                               platform_api_t const *color_buf,
                               u32 dst_x, u32 dst_y);
void render_text(platform_api_t const *color_buf, rendered_text_t *text);
simple_font_t* init_simple_font(u32 *font_pixels);

#endif