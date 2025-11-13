#ifndef UTIL_H_
#define UTIL_H_


#ifdef _WIN32
    #include <windows.h>
    typedef HANDLE thread_handle_t;
    typedef DWORD (WINAPI *thread_func_t)(LPVOID);
    typedef LPVOID thread_func_param_t;
    typedef DWORD WINAPI thread_func_ret_t;
#else
    #include <pthread.h>
    typedef pthread_t thread_handle_t;
    typedef void* (*thread_func_t)(void*);
    typedef void* thread_func_param_t;
    typedef void* thread_func_ret_t;
#endif

thread_handle_t create_thread(thread_func_t func, thread_func_param_t data);
void join_thread(thread_handle_t thread);
int get_core_count(void);

#endif