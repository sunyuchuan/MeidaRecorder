#include "xm_iencoder.h"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "XMMediaEncoder", __VA_ARGS__)

static void IEncoder_handleRun(void *opaque);

IEncoder *IEncoder_create(size_t opaque_size)
{
    IEncoder *encoder = (IEncoder *)calloc(1, sizeof(IEncoder));
    if (!encoder)
        return NULL;

    encoder->thread = XMThread_create();
    if(!encoder->thread)
    {
        free(encoder);
        return NULL;
    }
    encoder->thread->func_handleRun = IEncoder_handleRun;
    encoder->thread->opaque = (void *)encoder;

    encoder->opaque = calloc(1, opaque_size);
    if (!encoder->opaque) {
        XMThread_freep(&encoder->thread);
        free(encoder);
        return NULL;
    }

    return encoder;
}

void IEncoder_stop(IEncoder *encoder)
{
    if(!encoder)
        return;

    if(encoder->func_stop)
        encoder->func_stop(encoder->opaque);

    int ret = 0;
    if((ret = IEncoder_wait(encoder)) != 0) {
        LOGD("Couldn't cancel IEncoder: %d", ret);
        return;
    }
}

int IEncoder_enqueue(IEncoder *encoder, IEncoder_QueueData *qdata)
{
    int ret = 0;
    if(!encoder || !qdata)
        return ret;

    if(encoder->func_enqueue)
        ret = encoder->func_enqueue(encoder->opaque, qdata);

    return ret;
}

void IEncoder_flush(IEncoder *encoder)
{
    if(!encoder)
        return;

    if(encoder->func_flush)
        encoder->func_flush(encoder->opaque);
}

int IEncoder_queue_sizes(IEncoder *encoder)
{
    int ret = 0;
    if(!encoder)
        return ret;

    if(encoder->func_queue_sizes)
        ret = encoder->func_queue_sizes(encoder->opaque);

    return ret;
}

bool IEncoder_prepare(IEncoder *encoder)
{
    bool ret = false;
    if(!encoder)
        return ret;

    if(encoder->func_prepare)
        ret = encoder->func_prepare(encoder->opaque);

    return ret;
}

bool IEncoder_encode(IEncoder *encoder)
{
    bool ret = false;
    if(!encoder)
        return ret;

    if(encoder->func_encode)
        ret = encoder->func_encode(encoder->opaque);

    return ret;
}

static void IEncoder_handleRun(void *opaque)
{
    if(!opaque)
        return;

    IEncoder *encoder = (IEncoder *)opaque;
    xmmr_notify_msg1(encoder->msg_queue, MR_MSG_STARTED);

    if(!IEncoder_prepare(encoder))
    {
        LOGD("Couldn't prepare encoder");
        xmmr_notify_msg1(encoder->msg_queue, MR_MSG_ERROR);
        return;
    }

    if(!IEncoder_encode(encoder))
    {
        LOGD("Couldn't encode");
        xmmr_notify_msg1(encoder->msg_queue, MR_MSG_ERROR);
        return;
    }
}

int IEncoder_config(IEncoder *encoder, XMEncoderConfig *config)
{
    int ret = -1;
    if(!encoder || !config)
        return ret;

    if(encoder->func_config)
        ret = encoder->func_config(encoder->opaque, config);

    return ret;
}

void IEncoder_init(IEncoder *encoder)
{
    if(!encoder)
        return;

    if(encoder->func_init)
        encoder->func_init(encoder->opaque);
}

void IEncoder_start(IEncoder *encoder)
{
    if(!encoder)
        return;

    XMThread_start(encoder->thread);
}

void IEncoder_startAsync(IEncoder *encoder)
{
    if(!encoder)
        return;

    XMThread_startAsync(encoder->thread);
}

int IEncoder_wait(IEncoder *encoder)
{
    if(!encoder)
        return 0;

    return XMThread_wait(encoder->thread);
}

void IEncoder_waitOnNotify(IEncoder *encoder)
{
    if(!encoder)
        return;

    XMThread_waitOnNotify(encoder->thread);
}

void IEncoder_notify(IEncoder *encoder)
{
    if(!encoder)
        return;

    XMThread_notify(encoder->thread);
}

void IEncoder_free(IEncoder *encoder)
{
    if(!encoder)
        return;

    IEncoder_stop(encoder);
    XMThread_freep(&encoder->thread);

    if(encoder->func_free)
        encoder->func_free(encoder->opaque);

    free(encoder->opaque);
}

void IEncoder_freep(IEncoder **encoder)
{
    if(!encoder || !*encoder)
        return;

    IEncoder_free(*encoder);
    free(*encoder);
    *encoder = NULL;
}

