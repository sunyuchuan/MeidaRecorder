#include "xm_thread.h"
#include "ijksdl/ijksdl_error.h"
#include "ijksdl/android/ijksdl_android_jni.h"
#include <stdlib.h>

#include <jni.h>

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "XMThread", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "XMThread", __VA_ARGS__)

XMThread *XMThread_create()
{
    XMThread *thread = (XMThread *)calloc(1, sizeof(XMThread));
    if (!thread)
        return NULL;

    pthread_mutex_init(&thread->mLock, NULL);
    pthread_cond_init(&thread->mCondition, NULL);
    return thread;
}

static int startThread(void* ptr)
{
    XMThread* thread = NULL;
    JNIEnv *env = NULL;

    if (!ptr)
        return -1;

    if (JNI_OK != SDL_JNI_SetupThreadEnv(&env)) {
        LOGE("xm_thread: SetupThreadEnv failed\n");
        goto fail;
    }

    LOGD("starting thread");
    thread = (XMThread *)ptr;
    thread->mRunning = true;
    if (thread->func_handleRun)
        thread->func_handleRun(thread->opaque);

fail:
    thread->mRunning = false;
    LOGD("thread ended");
    return 0;
}

void XMThread_start(XMThread *thread)
{
    if(!thread)
        return;

    if (thread->func_handleRun)
        thread->func_handleRun(thread->opaque);
}

void XMThread_startAsync(XMThread *thread)
{
    if(!thread)
        return;

    thread->thread = SDL_CreateThreadEx(&thread->_thread, startThread, thread, "xm_thread");
    if (!thread->thread) {
        LOGD("xm_thread SDL_CreateThreadEx() failed : %s\n", SDL_GetError());
        XMThread_freep(&thread);
    }
}

int XMThread_wait(XMThread *thread)
{
    if (!thread)
        return 0;

    if (!thread->mRunning) {
        return 0;
    }

    SDL_WaitThread(thread->thread, NULL);
    thread->mRunning = false;
    return 0;
}

void XMThread_waitOnNotify(XMThread *thread)
{
    if(!thread)
        return;

    pthread_mutex_lock(&thread->mLock);
    pthread_cond_wait(&thread->mCondition, &thread->mLock);
    pthread_mutex_unlock(&thread->mLock);
}

void XMThread_notify(XMThread *thread)
{
    if(!thread)
        return;

    pthread_mutex_lock(&thread->mLock);
    pthread_cond_signal(&thread->mCondition);
    pthread_mutex_unlock(&thread->mLock);
}

void XMThread_free(XMThread *thread)
{
    if(!thread)
        return;

    pthread_mutex_destroy(&thread->mLock);
    pthread_cond_destroy(&thread->mCondition);
    memset(thread, 0, sizeof(XMThread));
}

void XMThread_freep(XMThread **thread)
{
    if (!thread || !*thread)
        return;

    XMThread_free(*thread);
    free(*thread);
    *thread = NULL;
}

