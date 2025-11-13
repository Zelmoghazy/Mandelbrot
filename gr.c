#include "gr.h"

float d_sqrt(float number) 
{
    int i;
    float x, y;
    x = number * 0.5;
    y = number;
    i = *(int *) &y;
    i = 0x5f3759df - (i >> 1);
    y = *(float *) &i;
    y = y * (1.5 - (x * y * y));
    y = y * (1.5 - (x * y * y));
    return number * y;
}

float gradient_noise(float x, float y) 
{
    const float magic_x = 0.06711056f;
    const float magic_y = 0.00583715f;
    const float magic_z = 52.9829189f;

    float dot_product = x * magic_x + y * magic_y;
    float fract_dot = dot_product - floorf(dot_product);
    float result = magic_z * fract_dot;
    return result - floorf(result);
}

color_t make_color(uint8_t r, uint8_t g, uint8_t b) 
{
    color_t color = {r, g, b, 255};  
    return color;
}

color_t rgb_from_wavelength(double wave) 
{
    double r = 0;
    double g = 0;
    double b = 0;

    if (wave >= 380.0 && wave <= 440.0) {
        r = -1.0 * (wave - 440.0) / (440.0 - 380.0);
        b = 1.0;
    } else if (wave >= 440.0 && wave <= 490.0) {
        g = (wave - 440.0) / (490.0 - 440.0);
        b = 1.0;
    } else if (wave >= 490.0 && wave <= 510.0) {
        g = 1.0;
        b = -1.0 * (wave - 510.0) / (510.0 - 490.0);
    } else if (wave >= 510.0 && wave <= 580.0) {
        r = (wave - 510.0) / (580.0 - 510.0);
        g = 1.0;
    } else if (wave >= 580.0 && wave <= 645.0) {
        r = 1.0;
        g = -1.0 * (wave - 645.0) / (645.0 - 580.0);
    } else if (wave >= 645.0 && wave <= 780.0) {
        r = 1.0;
    }

    double s = 1.0;
    if (wave > 700.0)
        s = 0.3 + 0.7 * (780.0 - wave) / (780.0 - 700.0);
    else if (wave < 420.0)
        s = 0.3 + 0.7 * (wave - 380.0) / (420.0 - 380.0);

    r = pow(r * s, 0.8);
    g = pow(g * s, 0.8);
    b = pow(b * s, 0.8);

    color_t color;
    color.r = (uint8_t) (r * 255);
    color.g = (uint8_t) (g * 255);
    color.b = (uint8_t) (b * 255);
    color.a = 255;   

    return color;
}


void set_pixel(platform_api_t *platform, int x, int y, color_t color) 
{
    if (x >= 0 && x < (int) platform->screen_width && y >= 0 &&
        y < (int) platform->screen_height) {
        platform->pixels[y * platform->screen_width + x] = color;
    }
}

inline color_t blend_pixel(color_t dst, color_t src) 
{
    if (IS_OPAQUE(src)) {
        return src;
    }

    if (IS_INVIS(src)) {
        return dst;
    }

    unsigned char inv_alpha = 255 - src.a;

    color_t result = {
        .r = (src.r * src.a + dst.r * inv_alpha) * (1.0f / 255.0f),
        .g = (src.g * src.a + dst.g * inv_alpha) * (1.0f / 255.0f),
        .b = (src.b * src.a + dst.b * inv_alpha) * (1.0f / 255.0f),
        .a = src.a + (dst.a * inv_alpha) * (1.0f / 255.0f)};

    return result;
}

/* Get existing color in the frame buffer and blend it witht the new color */
inline void set_pixel_blend(platform_api_t *platform, int x, int y,color_t color) 
{
    if (IS_OPAQUE(color)) {
        set_pixel(platform, x, y, color);
    } else {
        if (x >= 0 && x < (int) platform->screen_width && y >= 0 &&
            y < (int) platform->screen_height) {
            color_t dst = platform->pixels[y * platform->screen_width + x];
            set_pixel(platform, x, y, blend_pixel(dst, color));
        }
    }
}

void set_pixel_weighted(platform_api_t *platform, int x, int y, color_t color, int weight) 
{
    color_t weighted = color;
    weighted.a = ClampTop(255, (color.a * weight) * 1.0f / 255.0f);
    set_pixel_blend(platform, x, y, weighted);
}

void draw_line(platform_api_t *platform, int x0, int y0, int x1, int y1, color_t color) 
{
    bool steep = false;

    if (abs(x0 - x1) < abs(y0 - y1)) {
        SWAP(x0, y0, int);
        SWAP(x1, y1, int);
        steep = true;
    }

    if (x0 > x1) {
        SWAP(x0, x1, int);
        SWAP(y0, y1, int);
    }

    int dx = x1 - x0;
    int dy = y1 - y0;
    int derror2 = abs(dy) * 2;
    int error2 = 0;
    int y = y0;

    for (int x = x0; x <= x1; x++) {
        if (steep) {
            set_pixel_blend(platform, y, x, color);
        } else {
            set_pixel_blend(platform, x, y, color);
        }

        error2 += derror2;
        if (error2 > dx) {
            y += (y1 > y0 ? 1 : -1);
            error2 -= dx * 2;
        }
    }
}

void draw_hline(platform_api_t *platform, int y, int x0, int x1, color_t const color) 
{
    if (y < 0 || y >= (int) platform->screen_height) {
        return;
    }

    if (x0 > x1) {
        SWAP(x0, x1, int);
    }

    x0 = MAX(0, x0);
    x1 = MIN((int) platform->screen_width - 1, x1);

    for (int x = x0; x <= x1; x++) {
        set_pixel_blend(platform, x, y, color);
    }
}

void draw_vline(platform_api_t *platform, int x, int y0, int y1, color_t const color) 
{
    if (x < 0 || x >= (int) platform->screen_width) {
        return;
    }

    if (y0 > y1) {
        SWAP(y0, y1, int);
    }

    y0 = MAX(0, y0);
    y1 = MIN((int) platform->screen_height - 1, y1);

    for (int y = y0; y <= y1; y++) {
        set_pixel_blend(platform, x, y, color);
    }
}

void draw_aaline(platform_api_t *platform, int x0, int y0, int x1, int y1, color_t color, bool draw_endpoint) 
{
    if (y0 > y1) {
        SWAP(x0, x1, int);
        SWAP(y0, y1, int);
    }

    set_pixel_blend(platform, x0, y0, color);

    int xdir, dx = x1 - x0;
    if (dx >= 0) {
        xdir = 1;
    } else {
        xdir = -1;
        dx = -dx;  
    }

    int dy = y1 - y0;

    if (dy == 0) {
        for (int i = 0; i < dx; i++) {
            x0 += xdir;
            set_pixel_blend(platform, x0, y0, color);
        }
        return;
    }

    if (dx == 0) {
        for (int i = 0; i < dy; i++) {
            y0++;
            set_pixel_blend(platform, x0, y0, color);
        }
        return;
    }

    if (dx == dy) {
        for (int i = 0; i < dy; i++) {
            x0 += xdir;
            y0++;
            set_pixel_blend(platform, x0, y0, color);
        }
        return;
    }

    unsigned short error_adj;
    unsigned short error_acc = 0;

    if (dy > dx) {
        error_adj = ((unsigned long) dx << 16) / (unsigned long) dy;

        for (int i = 0; i < dy - 1; i++) {
            unsigned short error_acc_temp = error_acc;
            error_acc += error_adj;
            if (error_acc <= error_acc_temp) {
                x0 += xdir;
            }
            y0++;

            unsigned short weighting = error_acc >> 8;
            set_pixel_weighted(platform, x0, y0, color, 255 - weighting);
            set_pixel_weighted(platform, x0 + xdir, y0, color, weighting);
        }
    } else {
        error_adj = ((unsigned long) dy << 16) / (unsigned long) dx;

        for (int i = 0; i < dx - 1; i++) {
            unsigned short error_acc_temp = error_acc;
            error_acc += error_adj;
            if (error_acc <= error_acc_temp) {
                y0++;
            }
            x0 += xdir;

            unsigned short weighting = error_acc >> 8;
            set_pixel_weighted(platform, x0, y0, color, 255 - weighting);
            set_pixel_weighted(platform, x0, y0 + 1, color, weighting);
        }
    }

    if (draw_endpoint) {
        set_pixel_blend(platform, x1, y1, color);
    }
}

void draw_thick_line(platform_api_t *platform, int x1, int y1, int x2, int y2, int width, color_t color) 
{
    if (width < 1) {
        return;
    }

    if ((x1 == x2) && (y1 == y2)) {
        int radius = width / 2;
        draw_filled_polygon(
            platform,
            (int[]) {x1 - radius, x1 + radius, x1 + radius, x1 - radius},
            (int[]) {y1 - radius, y1 - radius, y1 + radius, y1 + radius}, 4,
            color);
        return;
    }

    if (width == 1) {
        draw_line(platform, x1, y1, x2, y2, color);
        return;
    }

    double dx = (double) (x2 - x1);
    double dy = (double) (y2 - y1);
    double l = sqrt(dx * dx + dy * dy);
    double ang = atan2(dx, dy);
    double adj = 0.1 + 0.9 * fabs(cos(2.0 * ang));
    double wl2 = ((double) width - adj) / (2.0 * l);
    double nx = dx * wl2;
    double ny = dy * wl2;

    int px[4], py[4];
    px[0] = (int) (x1 + ny);
    px[1] = (int) (x1 - ny);
    px[2] = (int) (x2 - ny);
    px[3] = (int) (x2 + ny);
    py[0] = (int) (y1 - nx);
    py[1] = (int) (y1 + nx);
    py[2] = (int) (y2 + nx);
    py[3] = (int) (y2 - nx);

    draw_filled_polygon(platform, px, py, 4, color);
}

void draw_thick_aaline(platform_api_t *platform, int x1, int y1, int x2, int y2, int width, color_t color) 
{
    if (width < 1)
        return;

    if (width == 1) {
        draw_aaline(platform, x1, y1, x2, y2, color, true);
        return;
    }

    if (x1 == x2 && y1 == y2) {
        int radius = width / 2;
        draw_filled_polygon(
            platform,
            (int[]) {x1 - radius, x1 + radius, x1 + radius, x1 - radius},
            (int[]) {y1 - radius, y1 - radius, y1 + radius, y1 + radius}, 4,
            color);
        return;
    }

    double dx = x2 - x1;
    double dy = y2 - y1;
    double length = sqrt(dx * dx + dy * dy);
    if (length == 0)
        return;

    double nx = -dy / length;
    double ny = dx / length;
    double hw = width / 2.0;
    double offset_x = nx * hw;
    double offset_y = ny * hw;

    int px[4], py[4];
    px[0] = (int) (x1 + offset_x);
    py[0] = (int) (y1 + offset_y);   // top start
    px[1] = (int) (x1 - offset_x);
    py[1] = (int) (y1 - offset_y);   // bottom start
    px[2] = (int) (x2 - offset_x);
    py[2] = (int) (y2 - offset_y);   // bottom end
    px[3] = (int) (x2 + offset_x);
    py[3] = (int) (y2 + offset_y);   // top end

    draw_filled_polygon(platform, px, py, 4, color);

    draw_aaline(platform, px[0], py[0], px[3], py[3], color,
                true);   // top edge
    draw_aaline(platform, px[1], py[1], px[2], py[2], color,
                true);   // bottom edge
}

void draw_rect(platform_api_t *platform, int x, int y, int w, int h, color_t color) 
{
    for (int py = y; py < y + h; py++) {
        for (int px = x; px < x + w; px++) {
            set_pixel(platform, px, py, color);
        }
    }
}

void draw_circle_outline(platform_api_t *platform, int cx, int cy, int radius, color_t color) 
{
    int x = 0;
    int y = radius;          // start at the bottom of the circle

    // decision parameter = ((x+1)^2 + (y-0.5)^2 - r^2) checks whether next step is inside or outside the circle
    // First iteration: x=0, y = radius -> d = 1 + (r-0.5)^2 - r^2 = 1 + r^2 - r + 0.25 - r^2 = (1.25 - r)
    // double and round  = 2.5 - 2r = 3 - 2r
    int d = 3 - 2 * radius;
    
    while (y >= x) 
    {
        set_pixel_blend(platform, cx + x, cy + y, color);   // bottom right arc
        set_pixel_blend(platform, cx - x, cy + y, color);   // bottom left arc
        set_pixel_blend(platform, cx + x, cy - y, color);   // top right arc
        set_pixel_blend(platform, cx - x, cy - y, color);   // top left arc
        set_pixel_blend(platform, cx + y, cy + x, color);   // low mid right arc
        set_pixel_blend(platform, cx - y, cy + x, color);   // low mid left arc
        set_pixel_blend(platform, cx + y, cy - x, color);   // high mid right arc
        set_pixel_blend(platform, cx - y, cy - x, color);   // high mid left arc
        
        x++;

        // decide whether we should decrement y or not (go up)
        // depending on whether the midpoint is inside the circle
        if (d > 0) {
            y--;
            d = d + 4 * (x - y) + 10;
        } else {
            d = d + 4 * x + 6;
        }
    }
}

void draw_circle_filled(platform_api_t *platform, int cx, int cy, int radius, color_t color) 
{
    int x = 0;
    int y = radius;   // start at the bottom of the circle

    // decision parameter = ((x+1)^2 + (y-0.5)^2 - r^2) checks whether we are
    // inside or outside the circle first iteration : x=0, y = radius -> d = 1 +
    // (r-0.5)^2 - r^2 = 1 + r^2 - r + 0.25 - r^2 = 1.25 - r double and round
    // = 2.5 - 2r = 3 - 2r
    int d = 3 - 2 * radius;

    // iterate through an octant of the circle
    while (y >= x) {
        set_pixel(platform, cx + x, cy + y, color);   // bottom right arc
        set_pixel(platform, cx - x, cy + y, color);   // bottom left arc
        set_pixel(platform, cx + x, cy - y, color);   // top right arc
        set_pixel(platform, cx - x, cy - y, color);   // top left arc
        set_pixel(platform, cx + y, cy + x, color);   // low mid right arc
        set_pixel(platform, cx - y, cy + x, color);   // low mid left arc
        set_pixel(platform, cx + y, cy - x, color);   // high mid right arc
        set_pixel(platform, cx - y, cy - x, color);   // high mid left arc

        draw_hline(platform, cy + y, cx - x, cx + x, color);
        draw_hline(platform, cy - y, cx - x, cx + x, color);
        draw_hline(platform, cy + x, cx - y, cx + y, color);
        draw_hline(platform, cy - x, cx - y, cx + y, color);

        x++;

        // decide whether we should decrement y or not (go up)
        // depending on whether the midpoint is inside the circle
        if (d > 0) {
            y--;
            d = d + 4 * (x - y) + 10;
        } else {
            d = d + 4 * x + 6;
        }
    }
}

void plot_4_points(platform_api_t *platform, int cx, int cy, int x, int y, float opacity, color_t color) 
{
    if (opacity <= 0.0f)
        return;

    unsigned char weight = (opacity * 255.0f);

    if (x > 0 && y > 0) {
        set_pixel_weighted(platform, cx + x, cy + y, color, weight);
        set_pixel_weighted(platform, cx + x, cy - y, color, weight);
        set_pixel_weighted(platform, cx - x, cy + y, color, weight);
        set_pixel_weighted(platform, cx - x, cy - y, color, weight);
    } else if (x == 0) {
        set_pixel_weighted(platform, cx + x, cy + y, color, weight);
        set_pixel_weighted(platform, cx + x, cy - y, color, weight);
    } else if (y == 0) {
        set_pixel_weighted(platform, cx + x, cy + y, color, weight);
        set_pixel_weighted(platform, cx - x, cy + y, color, weight);
    }
}

void draw_circle_aa(platform_api_t *platform, int cx, int cy, float radius, color_t color) 
{
    if (radius <= 0)
        return;

    float rsq = radius * radius;
    int ffd = (int) roundf(radius / sqrtf(2.0f));

    for (int xi = 0; xi <= ffd; xi++) {
        float yj = sqrtf(rsq - xi * xi);
        float frc = yj - floorf(yj);
        int flr = (int) floorf(yj);

        plot_4_points(platform, cx, cy, xi, flr, 1.0f - frc, color);
        plot_4_points(platform, cx, cy, xi, flr + 1, frc, color);
    }

    for (int yi = 0; yi <= ffd; yi++) {
        float xj = sqrtf(rsq - yi * yi);
        float frc = xj - floorf(xj);
        int flr = (int) floorf(xj);

        plot_4_points(platform, cx, cy, flr, yi, 1.0f - frc, color);
        plot_4_points(platform, cx, cy, flr + 1, yi, frc, color);
    }
}

void draw_circle_filled_aa(platform_api_t *platform, int cx, int cy, float radius, color_t color) 
{
    if (radius <= 0)
        return;

    float radiusX = radius;
    float radiusY = radius;
    float radiusX2 = radiusX * radiusX;
    float radiusY2 = radiusY * radiusY;

    int quarter = (int) roundf(radiusX2 / d_sqrt(radiusX2 + radiusY2));
    for (int x = 0; x <= quarter; x++) {
        float y = radiusY * d_sqrt(1.0f - (float) (x * x) / radiusX2);
        float error = y - floorf(y);

        unsigned char transparency = (unsigned char) (error * 255.0f);
        unsigned char transparency2 = (unsigned char) ((1.0f - error) * 255.0f);

        int y_floor = (int) floorf(y);

        for (int fill_x = -x; fill_x <= x; fill_x++) {
            set_pixel_blend(platform, cx + fill_x, cy + y_floor, color);
            set_pixel_blend(platform, cx + fill_x, cy - y_floor, color);
        }

        set_pixel_weighted(platform, cx + x, cy + y_floor + 1, color,
                           transparency);
        set_pixel_weighted(platform, cx - x, cy + y_floor + 1, color,
                           transparency);
        set_pixel_weighted(platform, cx + x, cy - y_floor - 1, color,
                           transparency);
        set_pixel_weighted(platform, cx - x, cy - y_floor - 1, color,
                           transparency);
    }

    quarter = (int) roundf(radiusY2 / d_sqrt(radiusX2 + radiusY2));
    for (int y = 0; y <= quarter; y++) {
        float x = radiusX * d_sqrt(1.0f - (float) (y * y) / radiusY2);
        float error = x - floorf(x);

        unsigned char transparency = (unsigned char) (error * 255.0f);
        unsigned char transparency2 = (unsigned char) ((1.0f - error) * 255.0f);

        int x_floor = (int) floorf(x);

        for (int fill_x = -x_floor; fill_x <= x_floor; fill_x++) {
            set_pixel_blend(platform, cx + fill_x, cy + y, color);
            set_pixel_blend(platform, cx + fill_x, cy - y, color);
        }

        set_pixel_weighted(platform, cx + x_floor + 1, cy + y, color,
                           transparency);
        set_pixel_weighted(platform, cx - x_floor - 1, cy + y, color,
                           transparency);
        set_pixel_weighted(platform, cx + x_floor + 1, cy - y, color,
                           transparency);
        set_pixel_weighted(platform, cx - x_floor - 1, cy - y, color,
                           transparency);
    }
}

void draw_ellipse_filled(platform_api_t *platform, int cx, int cy, int rx, int ry, color_t color) 
{
    if (rx <= 0 || ry <= 0)
        return;

    int rx2 = rx * rx;
    int ry2 = ry * ry;
    int tworx2 = 2 * rx2;
    int twory2 = 2 * ry2;
    int x = 0;
    int y = ry;
    int px = 0;
    int py = tworx2 * y;

    int p = ry2 - (rx2 * ry) + (rx2 / 4);
    while (px < py) {
        draw_hline(platform, cy + y, cx - x, cx + x, color);
        draw_hline(platform, cy - y, cx - x, cx + x, color);

        x++;
        px += twory2;
        if (p < 0) {
            p += ry2 + px;
        } else {
            y--;
            py -= tworx2;
            p += ry2 + px - py;
        }
    }

    p = (int) (ry2 * (x + 0.5) * (x + 0.5) + rx2 * (y - 1) * (y - 1) -
               rx2 * ry2);
    while (y >= 0) {
        draw_hline(platform, cy + y, cx - x, cx + x, color);
        draw_hline(platform, cy - y, cx - x, cx + x, color);

        y--;
        py -= tworx2;
        if (p > 0) {
            p += rx2 - py;
        } else {
            x++;
            px += twory2;
            p += rx2 - py + px;
        }
    }
}

void draw_filled_polygon(platform_api_t *platform, const int *vx, const int *vy, int n, color_t color) 
{
    if (vx == NULL || vy == NULL || n < 3) {
        return;
    }

    int miny = vy[0];
    int maxy = vy[0];
    for (int i = 1; i < n; i++) {
        if (vy[i] < miny)
            miny = vy[i];
        if (vy[i] > maxy)
            maxy = vy[i];
    }

    int *intersections = (int *) malloc(sizeof(int) * n);
    if (intersections == NULL) {
        return;
    }

    for (int y = miny; y <= maxy; y++) {
        int num_intersections = 0;

        for (int i = 0; i < n; i++) {
            int i1 = i;
            int i2 = (i + 1) % n;

            int y1 = vy[i1];
            int y2 = vy[i2];

            if (y1 == y2) {
                continue;
            }

            int x1, x2;
            if (y1 < y2) {
                x1 = vx[i1];
                x2 = vx[i2];
            } else {
                int temp_y = y1;
                y1 = y2;
                y2 = temp_y;
                x1 = vx[i2];
                x2 = vx[i1];
            }

            if (y >= y1 && y < y2) {
                int intersection =
                    ((y - y1) * (x2 - x1) * 256) / (y2 - y1) + x1 * 256;
                intersections[num_intersections++] = intersection;
            }
        }

        for (int i = 0; i < num_intersections - 1; i++) {
            for (int j = i + 1; j < num_intersections; j++) {
                if (intersections[i] > intersections[j]) {
                    int temp = intersections[i];
                    intersections[i] = intersections[j];
                    intersections[j] = temp;
                }
            }
        }

        for (int i = 0; i < num_intersections; i += 2) {
            if (i + 1 < num_intersections) {
                int x_start = (intersections[i] + 255) >> 8;   
                int x_end = (intersections[i + 1] - 1) >> 8;   

                if (x_start <= x_end) {
                    draw_hline(platform, y, x_start, x_end, color);
                }
            }
        }
    }
    free(intersections);
}

void clear_screen(platform_api_t *platform)
{
    color_t bg = {98, 114, 164, 255};
    color_t bg2 = {40, 42, 54, 255};

    float center_x = platform->screen_width * 0.5f;
    float center_y = platform->screen_height * 0.5f;
    float max_dist_sq = center_x * center_x + center_y * center_y;

    for (uint32_t y = 0; y < platform->screen_height; ++y) {
        for (uint32_t x = 0; x < platform->screen_width; ++x) {
            float dx = (float) x - center_x;
            float dy = (float) y - center_y;
            float dist_sq = dx * dx + dy * dy;

            float t = Clamp(0.0f, dist_sq / max_dist_sq, 1.0f);

            float noise = gradient_noise((float) x, (float) y) * 10.0f;

            float r = LERP_F32(bg.r, bg2.r, t) + noise;
            float g = LERP_F32(bg.g, bg2.g, t) + noise;
            float b = LERP_F32(bg.b, bg2.b, t) + noise;

            r = Clamp(0.0f, r, 255.0f);
            g = Clamp(0.0f, g, 255.0f);
            b = Clamp(0.0f, b, 255.0f);

            color_t pixel = {(uint8_t) r, (uint8_t) g, (uint8_t) b, 255};
            platform->pixels[x + y * platform->screen_width] = pixel;
        }
    }
}
