#ifndef XM_MEDIA_RECORDER_H
#define XM_MEDIA_RECORDER_H
#include "xm_encoder_factory.h"
#include "ijksdl/ijksdl_log.h"
#include "xm_media_muxer.h"
#include "xmmr_msg_def.h"

typedef struct XMMediaRecorder
{
    volatile int ref_count;
    pthread_mutex_t mutex;
    XMMediaMuxer *mm;
    XMEncoderConfig config;

    IEncoder *VEncoder;
    IEncoder *AEncoder;

    int (*msg_loop)(void*);
    SDL_Thread *msg_thread;
    SDL_Thread _msg_thread;

    int output_w;
    int output_h;
    int mr_state;
    void *weak_thiz;
    MessageQueue msg_queue;
    volatile bool start_mux;
} XMMediaRecorder;

void xm_media_recorder_freep(XMMediaRecorder **mr);
void xm_media_recorder_msg_thread_exit(XMMediaRecorder *mr);
void xm_media_recorder_stop(XMMediaRecorder *mr);
int xm_media_recorder_queue_sizes(XMMediaRecorder *mr);
void xm_media_recorder_put(XMMediaRecorder *mr, const unsigned char *rgba, int w, int h,
        int pixelStride, int rowPadding, int rotate_degree, bool flipHorizontal, bool flipVertical);
void xm_media_recorder_start(XMMediaRecorder *mr);
int xm_media_recorder_prepareAsync(XMMediaRecorder *mr);
void xm_media_recorder_initConfigParams(XMMediaRecorder *mr);
bool xm_media_recorder_setConfigParams(XMMediaRecorder *mr, const char *key, const char *value);
XMMediaRecorder *xm_media_recorder_create(int(*msg_loop)(void*), bool useSoftEncoder,
        bool audioEnable, bool videoEnable);
void xm_media_recorder_ffmpeg_init();

void *xmmr_get_weak_thiz(XMMediaRecorder *mr);
void *xmmr_set_weak_thiz(XMMediaRecorder *mr, void *weak_thiz);
void xmmr_dec_ref(XMMediaRecorder *mr);
void xmmr_dec_ref_p(XMMediaRecorder **mr);
void xmmr_inc_ref(XMMediaRecorder *mr);
int xmmr_get_msg(XMMediaRecorder *mr, AVMessage *msg, int block);

#endif
