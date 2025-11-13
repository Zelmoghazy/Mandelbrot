#include "util.h"

#ifdef _WIN32
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

thread_handle_t create_thread(thread_func_t func, thread_func_param_t data)
{
    #ifdef _WIN32
        return CreateThread(NULL,  // security attribute -no idea- NULL means default 
                             0,    // stack size zero means default size 
                             func, // pointer to the function to be executed by the thread
                             data, // pointer to the params passed to the thread
                              0,   // run immediately
                              NULL // dont return identifier
                            );
    #else
        pthread_t thread;
        pthread_create(&thread, NULL, func, data);
        return thread;
    #endif
}

void join_thread(thread_handle_t thread) 
{
    #ifdef _WIN32
        WaitForSingleObject(thread, INFINITE);
        CloseHandle(thread);
    #else
        pthread_join(thread, NULL);
    #endif
}

int get_core_count(void) 
{
    #ifdef _WIN32
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        return sysinfo.dwNumberOfProcessors;
    #else
        return sysconf(_SC_NPROCESSORS_ONLN);
    #endif
}