#ifndef XM_THREAD_H
#define XM_THREAD_H
#include <android/log.h>
#include <pthread.h>
#include <stdbool.h>
#include "ijksdl/ijksdl_thread.h"

typedef struct XMThread {
    SDL_Thread *thread;
    SDL_Thread _thread;
    pthread_mutex_t mLock;
    pthread_cond_t mCondition;
    bool mRunning;
    void *opaque;

    void (*func_handleRun)(void *opaque);
} XMThread;

XMThread *XMThread_create();
void XMThread_start(XMThread *thread);
void XMThread_startAsync(XMThread *thread);
int XMThread_wait(XMThread *thread);
void XMThread_waitOnNotify(XMThread *thread);
void XMThread_notify(XMThread *thread);
void XMThread_free(XMThread *thread);
void XMThread_freep(XMThread **thread);

#endif
