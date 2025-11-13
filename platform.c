#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <windows.h>
    #include <dwmapi.h>
    #pragma comment(lib, "dwmapi.lib")
#else
    #include <dlfcn.h>
    #include <sys/stat.h>
    #include <unistd.h>
#endif

#include "app_api.h"

typedef struct 
{
    GLFWwindow *window;
    GLuint texture;
    GLuint read_fbo;
    
    uint32_t screen_width;
    uint32_t screen_height;
    color_t *pixels;
    
    int32_t mouse_x, mouse_y;
    int32_t mouse_last_x, mouse_last_y;
    bool first_mouse;
    bool left_button_down;
    bool right_button_down;
    bool left_button_pressed;
    bool right_button_pressed;
    bool left_button_released;
    bool right_button_released;

    float scroll_x, scroll_y;
    
    bool keys[512];
    bool keys_pressed[512];
    bool keys_released[512];
    
    char input_chars[32];
    int32_t input_char_count;
    
    double time;
    double dt;
    double last_time;
    
    #ifdef _WIN32
        HMODULE app_dll;
        HANDLE compile_process;
    #else
        void *app_dll;
        pid_t compile_pid; 
    #endif
    
    app_init_func app_init;
    app_update_func app_update;
    app_render_func app_render;
    app_cleanup_func app_cleanup;
    app_on_reload_func app_on_reload;
    
    uint64_t last_dll_write_time;
    bool should_reload;
    bool is_compiling;           
    
    app_state_t app_state;
    
    bool running;
} platform_state_t;

platform_state_t plat = {0};


/*
    To automatically check whether a new version of the dll is produced
    by comparing it periodically with the current write time
 */
uint64_t get_file_write_time(const char *path) 
{
    #ifdef _WIN32
        WIN32_FILE_ATTRIBUTE_DATA data;
        if (GetFileAttributesExA(path, GetFileExInfoStandard, &data)) {
            ULARGE_INTEGER ull;
            ull.LowPart = data.ftLastWriteTime.dwLowDateTime;
            ull.HighPart = data.ftLastWriteTime.dwHighDateTime;
            return ull.QuadPart;
        }
        return 0;
    #else
        struct stat attr;
        if (stat(path, &attr) == 0) {
            return (uint64_t)attr.st_mtime;
        }
        return 0;
    #endif
}

/*
    Run the compilation in the background if the compilation succeed
    the write time will change and we will reload it, if not nothing happens
    TODO: find a way to get any compilation errors into the app (pipes?)
 */
int compile_app_dll(void) 
{
    if (plat.is_compiling) {
        printf("Compilation already in progress, ignoring F5\n");
        return 0;
    }

    plat.is_compiling = true;
    
    #ifdef _WIN32
        STARTUPINFOA si = {0};
        PROCESS_INFORMATION pi = {0};
        si.cb = sizeof(si);
        
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE; // If dwFlags specifies STARTF_USESHOWWINDOW, Hides the window
        
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "cmd.exe /c cd .. && build_app.bat rel");
        
        /* Creates a new process and its primary thread. */
        if (CreateProcessA(NULL,    // name of the module to be executed (null means use cmd line)
                          cmd,      // command line to be executed(if name is NULL module name is taken from it )
                          NULL,     // security attributes (NULL gets the default)
                          NULL,     // thread attribs
                          FALSE,    // handles are not inherited
                          CREATE_NO_WINDOW, // process creation flags
                          NULL,     // environment block? NULL means use parents
                          NULL,     // current dir, NULL means same of calling process
                          &si, 
                          &pi)) 
        {
            plat.compile_process = pi.hProcess;
            CloseHandle(pi.hThread);
            printf("Compilation started in background (PID: %lu)\n", pi.dwProcessId);
        } 
        else 
        {
            printf("Failed to start compilation process\n");
            plat.is_compiling = false;
        }
    #else        
        pid_t pid = fork();
        if (pid == 0) 
        {
            chdir("..");
            execlp("gcc", "gcc", "-shared", "-fPIC", "-o", "build/app.so", 
                   "app.c", "-Iinclude", "-Iexternal/include", "-DBUILD_DLL", NULL);
            exit(1);
        } 
        else if (pid > 0) 
        {
            plat.compile_pid = pid;
            printf("Compilation started in background (PID: %d)\n", pid);
        } 
        else
        {
            printf("Failed to fork compilation process\n");
            plat.is_compiling = false;
        }
    #endif

    return 1;
}

void check_compile_finished(void) 
{
    if (!plat.is_compiling) return;
    
    #ifdef _WIN32
        DWORD exit_code;
        if (GetExitCodeProcess(plat.compile_process, &exit_code)) 
        {
            if (exit_code != STILL_ACTIVE) 
            {
                CloseHandle(plat.compile_process);
                plat.compile_process = NULL;
                plat.is_compiling = false;
                
                if (exit_code == 0) 
                {
                    printf("Compilation finished successfully!\n");
                    printf("Waiting for DLL file to be written...\n");
                } 
                else 
                {
                    printf("Compilation failed with exit code: %lu\n", exit_code);
                }
            }
        }
    #else
        int status;
        pid_t result = waitpid(plat.compile_pid, &status, WNOHANG);
        if (result > 0) 
        {
            plat.is_compiling = false;
            
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                printf("Compilation finished successfully!\n");
                printf("Waiting for DLL file to be written...\n");
            } else {
                printf("Compilation failed\n");
            }
        }
    #endif
}

void unload_app_dll(void) 
{
    if (plat.app_dll) 
    {
        if (plat.app_cleanup) 
        {
            platform_api_t api = {0};
            plat.app_cleanup(&api, &plat.app_state);
        }
        
        #ifdef _WIN32
            FreeLibrary(plat.app_dll);
        #else
            dlclose(plat.app_dll);
        #endif
        
        plat.app_dll = NULL;
        plat.app_init = NULL;
        plat.app_update = NULL;
        plat.app_render = NULL;
        plat.app_cleanup = NULL;
        plat.app_on_reload = NULL;
    }
}

int load_app_dll(void) 
{
    const char *dll_path = 
        #ifdef _WIN32
            "app.dll"
        #else
            "./app.so"
        #endif
    ;
    
    const char *temp_path = 
        #ifdef _WIN32
            "app_temp.dll"
        #else
            "./app_temp.so"
        #endif
    ;
    
    #ifdef _WIN32
    /*
        Windows prevents modifications to files that are currently open. 
        you won’t be able to recompile a .dll until your program closes it. 
        a work around is to copy the library and open the copy
     */
        CopyFileA(dll_path, temp_path, FALSE);

        plat.app_dll = LoadLibraryA(temp_path);
        if (!plat.app_dll) {
            printf("Failed to load DLL: %lu\n", GetLastError());
            return 0;
        }
        
        plat.app_init = (app_init_func)GetProcAddress(plat.app_dll, "app_init");
        plat.app_update = (app_update_func)GetProcAddress(plat.app_dll, "app_update");
        plat.app_render = (app_render_func)GetProcAddress(plat.app_dll, "app_render");
        plat.app_cleanup = (app_cleanup_func)GetProcAddress(plat.app_dll, "app_cleanup");
        plat.app_on_reload = (app_on_reload_func)GetProcAddress(plat.app_dll, "app_on_reload");
    #else
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "cp %s %s", dll_path, temp_path);
        system(cmd);
        
        plat.app_dll = dlopen(temp_path, RTLD_NOW);
        if (!plat.app_dll) {
            printf("Failed to load SO: %s\n", dlerror());
            return 0;
        }
        
        plat.app_init = (app_init_func)dlsym(plat.app_dll, "app_init");
        plat.app_update = (app_update_func)dlsym(plat.app_dll, "app_update");
        plat.app_render = (app_render_func)dlsym(plat.app_dll, "app_render");
        plat.app_cleanup = (app_cleanup_func)dlsym(plat.app_dll, "app_cleanup");
        plat.app_on_reload = (app_on_reload_func)dlsym(plat.app_dll, "app_on_reload");
    #endif
    
    if (!plat.app_update || !plat.app_render) 
    {
        printf("Failed to find required functions (app_update, app_render)\n");
        unload_app_dll();
        return 0;
    }
    
    plat.last_dll_write_time = get_file_write_time(dll_path);
    printf("Loaded app DLL successfully\n");
    return 1;
}

void check_for_reload(void) 
{
    const char *dll_path = 
        #ifdef _WIN32
            "app.dll"
        #else
            "./app.so"
        #endif
    ;
    
    uint64_t current_time = get_file_write_time(dll_path);
    
    if (current_time != plat.last_dll_write_time && current_time != 0) 
    {
        printf("\n========================================\n");
        printf("DLL changed! Reloading...\n");
        printf("========================================\n");
        
        unload_app_dll();
        
        #ifdef _WIN32
            Sleep(100);
        #else
            usleep(100000);
        #endif
        
        if (load_app_dll()) 
        {
            printf("Reload successful!\n");
            
            if (plat.app_on_reload) {
                platform_api_t api = {0};
                plat.app_on_reload(&api, &plat.app_state);
            }
        } 
        else 
        {
            printf("Reload failed!\n");
        }
        printf("========================================\n\n");
    }
}

#define TGA_HEADER(buf,w,h,b) \
    header[2]  = 2;\
    header[12] = (w) & 0xFF;\
    header[13] = ((w) >> 8) & 0xFF;\
    header[14] = (h) & 0xFF;\
    header[15] = ((h) >> 8) & 0xFF;\
    header[16] = (b);\
    header[17] |= 0x20 
    
void save_screenshot(void) 
{
    FILE *f = fopen("screenshot.tga", "wb");
    if (!f) return;
    
    uint8_t header[18] = {0};
    TGA_HEADER(header, plat.screen_width, plat.screen_height, 32);

    fwrite(header, sizeof(uint8_t), 18, f);
    
    for (uint32_t i = 0; i < plat.screen_width * plat.screen_height; i++) 
    {
        fputc(plat.pixels[i].b, f);
        fputc(plat.pixels[i].g, f);
        fputc(plat.pixels[i].r, f);
        fputc(plat.pixels[i].a, f);
    }
    
    fclose(f);
    printf("Screenshot saved: screenshot.tga\n");
}

void init_framebuffer(void) 
{
    glGenTextures(1, &plat.texture);
    glBindTexture(GL_TEXTURE_2D, plat.texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 
                 1920, 1080, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    
    glGenFramebuffers(1, &plat.read_fbo);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, plat.read_fbo);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, plat.texture, 0);
}

void blit_to_screen(void) 
{
    glBindTexture(GL_TEXTURE_2D, plat.texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 
                    plat.screen_width, plat.screen_height,
                    GL_RGBA, GL_UNSIGNED_BYTE, plat.pixels);
    
    glBindFramebuffer(GL_READ_FRAMEBUFFER, plat.read_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    
    glBlitFramebuffer(0, 0, plat.screen_width, plat.screen_height,
                      0, plat.screen_height, plat.screen_width, 0,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) 
{
    (void)window;
    plat.screen_width = width;
    plat.screen_height = height;
    glViewport(0, 0, width, height);
    
    free(plat.pixels);
    plat.pixels = malloc(plat.screen_width * plat.screen_height * sizeof(color_t));
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    (void)window; (void)scancode; (void)mods;
    
    if (key < 0 || key >= 512) return;
    
    if (action == GLFW_PRESS) 
    {
        plat.keys[key] = true;
        plat.keys_pressed[key] = true;
        
        if (key == GLFW_KEY_F5) 
        {
            compile_app_dll();
        }
    } 
    else if (action == GLFW_RELEASE) 
    {
        plat.keys[key] = false;
        plat.keys_released[key] = true;
    }
}

void char_callback(GLFWwindow* window, unsigned int codepoint) 
{
    (void)window;
    if (codepoint >= 32 && codepoint <= 126) 
    {
        if (plat.input_char_count < 31) 
        {
            plat.input_chars[plat.input_char_count++] = (char)codepoint;
        }
    }
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) 
{
    (void)window; (void)mods;
    
    if (button == GLFW_MOUSE_BUTTON_LEFT) 
    {
        bool was_down = plat.left_button_down;
        plat.left_button_down = (action == GLFW_PRESS);
        
        if (plat.left_button_down && !was_down) {
            plat.left_button_pressed = true;
        } else if (!plat.left_button_down && was_down) {
            plat.left_button_released = true;
        }
    } 
    else if (button == GLFW_MOUSE_BUTTON_RIGHT) 
    {
        bool was_down = plat.right_button_down;
        plat.right_button_down = (action == GLFW_PRESS);
        
        if (plat.right_button_down && !was_down) {
            plat.right_button_pressed = true;
        } else if (!plat.right_button_down && was_down) {
            plat.right_button_released = true;
        }
    }
}

void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos) 
{
    (void)window;
    
    if (plat.first_mouse) {
        plat.mouse_last_x = (int32_t)xpos;
        plat.mouse_last_y = (int32_t)ypos;
        plat.first_mouse = false;
    }
    
    plat.mouse_x = (int32_t)xpos;
    plat.mouse_y = (int32_t)ypos;
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) 
{
    (void)window;
    plat.scroll_x = (float)xoffset;
    plat.scroll_y = (float)yoffset;
}

void fill_platform_api(platform_api_t *api)
{
    api->screen_width = plat.screen_width;
    api->screen_height = plat.screen_height;
    api->pixels = plat.pixels;
    
    api->mouse_x = plat.mouse_x;
    api->mouse_y = plat.mouse_y;
    api->mouse_delta_x = plat.mouse_x - plat.mouse_last_x;
    api->mouse_delta_y = plat.mouse_y - plat.mouse_last_y;
    api->left_button_down = plat.left_button_down;
    api->right_button_down = plat.right_button_down;
    api->left_button_pressed = plat.left_button_pressed;
    api->right_button_pressed = plat.right_button_pressed;
    api->left_button_released = plat.left_button_released;
    api->right_button_released = plat.right_button_released;
    api->scroll_x = plat.scroll_x;
    api->scroll_y = -plat.scroll_y;
    
    memcpy(api->keys, plat.keys, sizeof(plat.keys));
    memcpy(api->keys_pressed, plat.keys_pressed, sizeof(plat.keys_pressed));
    memcpy(api->keys_released, plat.keys_released, sizeof(plat.keys_released));
    
    memcpy(api->input_chars, plat.input_chars, sizeof(plat.input_chars));
    api->input_char_count = plat.input_char_count;
    
    api->time = plat.time;
    api->dt = plat.dt;
    
    api->should_quit = false;
    api->capture_frame = false;
}

void apply_platform_api(platform_api_t *api) 
{
    if (api->should_quit) {
        plat.running = false;
    }
    
    if (api->capture_frame) {
        save_screenshot();
    }
}

void reset_input_for_frame(void) 
{
    memset(plat.keys_pressed, 0, sizeof(plat.keys_pressed));
    memset(plat.keys_released, 0, sizeof(plat.keys_released));
    plat.left_button_pressed = false;
    plat.right_button_pressed = false;
    plat.left_button_released = false;
    plat.right_button_released = false;
    plat.input_char_count = 0;
    plat.scroll_x = 0.0f;
    plat.scroll_y = 0.0f;
    
    plat.mouse_last_x = plat.mouse_x;
    plat.mouse_last_y = plat.mouse_y;
}

#ifdef _WIN32
    void set_dark_mode(GLFWwindow *window) 
    {
        HWND hwnd = glfwGetWin32Window(window);
        if (!hwnd) return;
        BOOL value = TRUE;
        DwmSetWindowAttribute(hwnd, 20, &value, sizeof(value));
    }
#endif

int main(void) 
{
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return 1;
    }
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    
    plat.screen_width = 1100;
    plat.screen_height = 800;
    plat.first_mouse = true;
    
    plat.window = glfwCreateWindow(plat.screen_width, plat.screen_height, 
                                    "Mandelbrot Viz", NULL, NULL);
    if (!plat.window) {
        fprintf(stderr, "Failed to create window\n");
        glfwTerminate();
        return 1;
    }
    
    glfwMakeContextCurrent(plat.window);
    glfwSwapInterval(1);
    
    #ifdef _WIN32
        set_dark_mode(plat.window);
    #endif
    
    glfwSetKeyCallback(plat.window, key_callback);
    glfwSetCharCallback(plat.window, char_callback);
    glfwSetMouseButtonCallback(plat.window, mouse_button_callback);
    glfwSetCursorPosCallback(plat.window, cursor_pos_callback);
    glfwSetScrollCallback(plat.window, scroll_callback);
    glfwSetFramebufferSizeCallback(plat.window, framebuffer_size_callback);
    
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        fprintf(stderr, "Failed to initialize GLEW\n");
        return 1;
    }
    
    printf("OpenGL Version: %s\n", glGetString(GL_VERSION));
    
    init_framebuffer();
    
    plat.pixels = malloc(plat.screen_width * plat.screen_height * sizeof(color_t));
    
    if (!load_app_dll()) {
        fprintf(stderr, "Failed to load app DLL\n");
        return 1;
    }
    
    if (plat.app_init) {
        platform_api_t api = {0};
        fill_platform_api(&api);
        plat.app_init(&api, &plat.app_state);
    }
    
    plat.running = true;
    plat.last_time = glfwGetTime();
    
    while (!glfwWindowShouldClose(plat.window) && plat.running)
    {
        reset_input_for_frame();
        
        double current_time = glfwGetTime();
        plat.dt = current_time - plat.last_time;
        plat.last_time = current_time;
        plat.time += plat.dt;

        glfwPollEvents();
        
        check_compile_finished();
        
        check_for_reload();
        
        platform_api_t api = {0};
        fill_platform_api(&api);
        
        if (plat.app_update) {
            plat.app_update(&api, &plat.app_state);
        }
        
        if (plat.app_render) {
            plat.app_render(&api, &plat.app_state);
        }
        
        apply_platform_api(&api);
        
        blit_to_screen();
        glfwSwapBuffers(plat.window);
    }
    
    unload_app_dll();
    free(plat.pixels);
    glfwTerminate();
    
    return 0;
}