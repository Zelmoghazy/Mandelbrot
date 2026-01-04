#ifndef APP_API_H
#define APP_API_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t r, g, b, a;
} color_t;

typedef struct 
{
    uint32_t screen_width;
    uint32_t screen_height;
    color_t *pixels;
    
    int32_t mouse_x;
    int32_t mouse_y;
    int32_t mouse_delta_x;
    int32_t mouse_delta_y;

    bool left_button_down;
    bool right_button_down;
    bool left_button_pressed;   
    bool right_button_pressed;  
    bool left_button_released;  
    bool right_button_released; 

    float scroll_x;
    float scroll_y;
    
    bool keys[512];              
    bool keys_pressed[512];      
    bool keys_released[512];     
    
    char input_chars[32];        
    int32_t input_char_count;
    
    double time;        
    double dt;          

    bool should_quit;
    bool capture_frame;
} platform_api_t;

/*
    Stuff we want to keep after reload
 */
typedef struct 
{
    bool initialized;
    
    float animation_time;
    int frame_count;

    double view_center_x;
    double view_center_y;
    double view_scale;
    double target_scale;

    bool is_panning;
    int pan_start_x;
    int pan_start_y;
    double pan_start_center_x;
    double pan_start_center_y;

    int last_mouse_x;
    int last_mouse_y;

    bool use_gpu;
        
    void *user_data;
} app_state_t;

#ifdef BUILD_DLL
    #ifdef _WIN32
        #define EXPORT __declspec(dllexport)
    #else
        #define EXPORT __attribute__((visibility("default")))
    #endif
#else
    #define EXPORT
#endif

typedef void (*app_init_func)(platform_api_t *platform, app_state_t *state);
typedef void (*app_update_func)(platform_api_t *platform, app_state_t *state);
typedef void (*app_render_func)(platform_api_t *platform, app_state_t *state);
typedef void (*app_cleanup_func)(platform_api_t *platform, app_state_t *state);
typedef void (*app_on_reload_func)(platform_api_t *platform, app_state_t *state);

#endif // APP_API_H