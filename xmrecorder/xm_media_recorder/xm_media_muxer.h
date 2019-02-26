#ifndef XM_MEDIA_MUXER_H
#define XM_MEDIA_MUXER_H
#include "ijksdl/ijksdl_thread.h"
#include "ff_ffplay_def.h"
#include "xm_packet_queue.h"
#include "xm_encoder_config.h"
#include "xmmr_msg_def.h"

typedef struct XMMediaMuxer
{
    volatile bool abort_muxer;
    void *opaque;
    bool mRunning;
    XMPacketQueue videoq;
    XMPacketQueue audioq;
    XMEncoderConfig config;

    SDL_Thread *mux_thread;
    SDL_Thread _mux_thread;
    pthread_mutex_t mutex;
    pthread_cond_t mCondition;
    MessageQueue *msg_queue;
} XMMediaMuxer;

void muxer_free(XMMediaMuxer *mm);
void muxer_freep(XMMediaMuxer **mm);
void muxer_waitOnNotify(XMMediaMuxer *mm);
void muxer_notify(XMMediaMuxer *mm);
void muxer_abort(XMMediaMuxer *mm);
int muxer_wait(XMMediaMuxer *mm);
void muxer_stop(XMMediaMuxer *mm);
int muxer_startAsync(XMMediaMuxer *mm);
void muxer_config(XMMediaMuxer *mm, XMEncoderConfig *config);
XMMediaMuxer *muxer_create(void *mr);
#endif
