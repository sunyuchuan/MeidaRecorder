#ifndef XM_I_ENCODER_H
#define XM_I_ENCODER_H
#include <android/log.h>
#include <pthread.h>
#include <stdbool.h>
#include "xm_thread.h"
#include "xm_rgba_data.h"
#include "xmmr_msg_def.h"
#include "xm_encoder_config.h"

#define RGBA_CHANNEL 4

typedef struct IEncoder_Opaque IEncoder_Opaque;
typedef struct IEncoder_QueueData {
    RgbaData rgba_data; //video data
    //pcm data //audio data
    //etc
} IEncoder_QueueData;

typedef struct IEncoder
{
    IEncoder_Opaque *opaque;
    XMThread *thread;
    MessageQueue *msg_queue;

    void (*func_stop)(IEncoder_Opaque *opaque);
    int (*func_enqueue)(IEncoder_Opaque *opaque, IEncoder_QueueData *qdata);
    void (*func_flush)(IEncoder_Opaque *opaque);
    int (*func_queue_sizes)(IEncoder_Opaque *opaque);
    bool (*func_prepare)(IEncoder_Opaque *opaque);
    bool (*func_encode)(IEncoder_Opaque *opaque);
    bool (*func_free)(IEncoder_Opaque *opaque);
    int (*func_config)(IEncoder_Opaque *opaque, XMEncoderConfig *config);
    void (*func_init)(IEncoder_Opaque *opaque);
} IEncoder;

IEncoder *IEncoder_create(size_t opaque_size);
int IEncoder_enqueue(IEncoder *encoder, IEncoder_QueueData *qdata);
void IEncoder_flush(IEncoder *encoder);
int IEncoder_queue_sizes(IEncoder *encoder);
void IEncoder_free(IEncoder *encoder);
void IEncoder_freep(IEncoder **encoder);
int IEncoder_config(IEncoder *encoder, XMEncoderConfig *config);
void IEncoder_init(IEncoder *encoder);
void IEncoder_start(IEncoder *encoder);
void IEncoder_startAsync(IEncoder *encoder);
int IEncoder_wait(IEncoder *encoder);
void IEncoder_stop(IEncoder *encoder);
void IEncoder_waitOnNotify(IEncoder *encoder);
void IEncoder_notify(IEncoder *encoder);

#endif
