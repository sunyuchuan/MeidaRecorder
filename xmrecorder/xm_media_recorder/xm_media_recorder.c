#include "xm_media_recorder.h"
#include <android/log.h>
#include "ff_ffmsg_queue.h"
#include "libyuv.h"
#include "xm_rgba_process.h"
#include "xm_encoder_config.h"

#define TAG "xm_media_recorder"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG,TAG,__VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,TAG,__VA_ARGS__)

#define XMMR_MAX_QUEUE_SIZES 7
#define XMMR_FAILED -1
#define XMMR_OUT_OF_MEMORY -2
#define XMMR_INVALID_STATE -3
#define XMMR_NULL_IS_PTR -4

#define MP_RET_IF_FAILED(ret) \
    do { \
        int retval = ret; \
        if (retval != 0) return (retval); \
    } while(0)

#define MPST_RET_IF_EQ_INT(real, expected, errcode) \
    do { \
        if ((real) == (expected)) return (errcode); \
    } while(0)

#define MPST_RET_IF_EQ(real, expected) \
    MPST_RET_IF_EQ_INT(real, expected, XMMR_INVALID_STATE)

static int xm_media_recorder_start_l(XMMediaRecorder *mr);
static int xm_media_recorder_stop_l(XMMediaRecorder *mr);
static int xm_media_recorder_freep_l(XMMediaRecorder **mr);

static void xmmr_change_state_l(XMMediaRecorder *mr, int new_state)
{
    mr->mr_state = new_state;
    xmmr_notify_msg1(&mr->msg_queue, MR_MSG_STATE_CHANGED);
}

static int xmmr_msg_loop(void *arg)
{
    XMMediaRecorder *mr = arg;
    int ret = mr->msg_loop(arg);
    return ret;
}

void *xmmr_get_weak_thiz(XMMediaRecorder *mr)
{
    if(!mr)
        return NULL;

    return mr->weak_thiz;
}

void *xmmr_set_weak_thiz(XMMediaRecorder *mr, void *weak_thiz)
{
    if(!mr)
        return NULL;

    void *prev_weak_thiz = mr->weak_thiz;

    mr->weak_thiz = weak_thiz;

    return prev_weak_thiz;
}

void xmmr_inc_ref(XMMediaRecorder *mr)
{
    assert(mr);
    __sync_fetch_and_add(&mr->ref_count, 1);
}

void xmmr_dec_ref(XMMediaRecorder *mr)
{
    if (!mr)
        return;

    int ref_count = __sync_sub_and_fetch(&mr->ref_count, 1);
    if (ref_count == 0) {
        LOGD("xmmr_dec_ref(): ref=0\n");
        xm_media_recorder_stop_l(mr);
        xm_media_recorder_freep_l(&mr);
    }
}

void xmmr_dec_ref_p(XMMediaRecorder **mr)
{
    if (!mr || !*mr)
        return;

    xmmr_dec_ref(*mr);
    *mr = NULL;
}

static int xmmr_chkst_restart_l(int mr_state)
{
    MPST_RET_IF_EQ(mr_state, MR_STATE_UNINIT);
    MPST_RET_IF_EQ(mr_state, MR_STATE_INITIALIZED);
    MPST_RET_IF_EQ(mr_state, MR_STATE_ASYNC_PREPARING);
    MPST_RET_IF_EQ(mr_state, MR_STATE_PREPARED);
    MPST_RET_IF_EQ(mr_state, MR_STATE_STARTED);
    //MPST_RET_IF_EQ(mr_state, MR_STATE_COMPLETED);
    //MPST_RET_IF_EQ(mr_state, MR_STATE_STOPPED);
    MPST_RET_IF_EQ(mr_state, MR_STATE_ERROR);

    return 0;
}

static int xmmr_chkst_start_l(int mr_state)
{
    MPST_RET_IF_EQ(mr_state, MR_STATE_UNINIT);
    MPST_RET_IF_EQ(mr_state, MR_STATE_INITIALIZED);
    MPST_RET_IF_EQ(mr_state, MR_STATE_ASYNC_PREPARING);
    // MPST_RET_IF_EQ(mr_state, MR_STATE_PREPARED);
    MPST_RET_IF_EQ(mr_state, MR_STATE_STARTED);
    MPST_RET_IF_EQ(mr_state, MR_STATE_COMPLETED);
    MPST_RET_IF_EQ(mr_state, MR_STATE_STOPPED);
    MPST_RET_IF_EQ(mr_state, MR_STATE_ERROR);

    return 0;
}

static int xmmr_chkst_stop_l(int mr_state)
{
    MPST_RET_IF_EQ(mr_state, MR_STATE_UNINIT);
    MPST_RET_IF_EQ(mr_state, MR_STATE_INITIALIZED);
    //MPST_RET_IF_EQ(mr_state, MR_STATE_ASYNC_PREPARING);
    // MPST_RET_IF_EQ(mr_state, MR_STATE_PREPARED);
    // MPST_RET_IF_EQ(mr_state, MR_STATE_STARTED);
    MPST_RET_IF_EQ(mr_state, MR_STATE_COMPLETED);
    MPST_RET_IF_EQ(mr_state, MR_STATE_STOPPED);
    //MPST_RET_IF_EQ(mr_state, MR_STATE_ERROR);

    return 0;
}

static int xmmr_chkst_put_l(int mr_state)
{
    MPST_RET_IF_EQ(mr_state, MR_STATE_UNINIT);
    //MPST_RET_IF_EQ(mr_state, MR_STATE_INITIALIZED);
    //MPST_RET_IF_EQ(mr_state, MR_STATE_ASYNC_PREPARING);
    //MPST_RET_IF_EQ(mr_state, MR_STATE_PREPARED);
    //MPST_RET_IF_EQ(mr_state, MR_STATE_REQ_START);
    //MPST_RET_IF_EQ(mr_state, MR_STATE_STARTED);
    MPST_RET_IF_EQ(mr_state, MR_STATE_COMPLETED);
    MPST_RET_IF_EQ(mr_state, MR_STATE_REQ_STOP);
    MPST_RET_IF_EQ(mr_state, MR_STATE_STOPPED);
    MPST_RET_IF_EQ(mr_state, MR_STATE_ERROR);

    return 0;
}

int xmmr_get_msg(XMMediaRecorder *mr, AVMessage *msg, int block)
{
    assert(mr);
    while (1) {
        int continue_wait_next_msg = 0;
        int retval = msg_queue_get(&mr->msg_queue, msg, block);
        if (retval <= 0)
            return retval;

        switch (msg->what) {
	case FFP_MSG_FLUSH:
            LOGD("xmmr_get_msg: FFP_MSG_FLUSH\n");
            pthread_mutex_lock(&mr->mutex);
            if (mr->mr_state == MR_STATE_ASYNC_PREPARING)
            {
                xmmr_notify_msg1(&mr->msg_queue, MR_MSG_PREPARED);
            } else {
                xmmr_notify_msg1(&mr->msg_queue, MR_MSG_ERROR);
                LOGE("FFP_MSG_FLUSH: expecting mr_state == MR_STATE_ASYNC_PREPARING\n");
            }
            pthread_mutex_unlock(&mr->mutex);
            break;

        case MR_MSG_PREPARED:
            LOGD("xmmr_get_msg: MR_MSG_PREPARED\n");
            pthread_mutex_lock(&mr->mutex);
            if (mr->mr_state == MR_STATE_ASYNC_PREPARING) {
                xmmr_change_state_l(mr, MR_STATE_PREPARED);
            } else {
                xmmr_notify_msg1(&mr->msg_queue, MR_MSG_ERROR);
                LOGE("MR_MSG_PREPARED: expecting mr_state == MR_STATE_ASYNC_PREPARING\n");
            }
            pthread_mutex_unlock(&mr->mutex);
            break;

        case MR_REQ_START:
            LOGD("xmmr_get_msg: MR_REQ_START\n");
            continue_wait_next_msg = 1;
            pthread_mutex_lock(&mr->mutex);
            if (0 == xmmr_chkst_start_l(mr->mr_state)) {
                if(xm_media_recorder_start_l(mr) < 0)
                {
                    xmmr_notify_msg1(&mr->msg_queue, MR_MSG_ERROR);
                }
            } else {
                xmmr_notify_msg1(&mr->msg_queue, MR_MSG_ERROR);
                LOGE("MR_REQ_START: expecting mr_state == prepared\n");
            }
            pthread_mutex_unlock(&mr->mutex);
            break;

        case MR_MSG_STARTED:
            LOGD("xmmr_get_msg: MR_MSG_STARTED\n");
            pthread_mutex_lock(&mr->mutex);
            xmmr_change_state_l(mr, MR_STATE_STARTED);
            pthread_mutex_unlock(&mr->mutex);
            break;

        case MR_REQ_STOP:
            LOGD("xmmr_get_msg: MR_REQ_STOP\n");
            continue_wait_next_msg = 1;
            pthread_mutex_lock(&mr->mutex);
            if (0 == xmmr_chkst_stop_l(mr->mr_state)) {
                 xm_media_recorder_stop_l(mr);
            } else {
                 xmmr_notify_msg1(&mr->msg_queue, MR_MSG_STOPPED);
            }
            pthread_mutex_unlock(&mr->mutex);
            break;

        case MR_MSG_STOPPED:
            LOGD("xmmr_get_msg: MR_MSG_STOPPED\n");
            pthread_mutex_lock(&mr->mutex);
            xmmr_change_state_l(mr, MR_STATE_STOPPED);
            pthread_mutex_unlock(&mr->mutex);
            break;

        case MR_MSG_COMPLETED:
            LOGD("xmmr_get_msg: MR_MSG_COMPLETED\n");
            pthread_mutex_lock(&mr->mutex);
            xmmr_change_state_l(mr, MR_STATE_COMPLETED);
            pthread_mutex_unlock(&mr->mutex);
            break;

        case MR_MSG_ERROR:
            LOGD("xmmr_get_msg: MR_MSG_ERROR\n");
            pthread_mutex_lock(&mr->mutex);
            xmmr_change_state_l(mr, MR_STATE_ERROR);
            pthread_mutex_unlock(&mr->mutex);
            break;
        }

        if (continue_wait_next_msg)
            continue;

        return retval;
    }

    return -1;
}

static void xm_media_recorder_free_l(XMMediaRecorder *mr)
{
    LOGD("xm_media_recorder_free_l");
    if(!mr)
        return;

    if(mr->config.output_filename)
        av_free(mr->config.output_filename);
    mr->config.output_filename = NULL;

    if(mr->config.preset)
        av_free(mr->config.preset);
    mr->config.preset = NULL;

    if(mr->config.tune)
        av_free(mr->config.tune);
    mr->config.tune = NULL;

    IEncoder_freep(&mr->VEncoder);
    IEncoder_freep(&mr->AEncoder);
    muxer_freep(&mr->mm);

    pthread_mutex_destroy(&mr->mutex);
    msg_queue_destroy(&mr->msg_queue);
}

static int xm_media_recorder_freep_l(XMMediaRecorder **mr)
{
    if(!mr || !*mr)
        return -1;

    //MPST_RET_IF_EQ((*mr)->mr_state, MR_STATE_UNINIT);
    //MPST_RET_IF_EQ((*mr)->mr_state, MR_STATE_INITIALIZED);
    MPST_RET_IF_EQ((*mr)->mr_state, MR_STATE_ASYNC_PREPARING);
    MPST_RET_IF_EQ((*mr)->mr_state, MR_STATE_PREPARED);
    MPST_RET_IF_EQ((*mr)->mr_state, MR_STATE_STARTED);
    //MPST_RET_IF_EQ((*mr)->mr_state, MR_STATE_COMPLETED);
    //MPST_RET_IF_EQ((*mr)->mr_state, MR_STATE_STOPPED);
    //MPST_RET_IF_EQ((*mr)->mr_state, MR_STATE_ERROR);

    xm_media_recorder_free_l(*mr);
    free(*mr);
    *mr = NULL;
    return 0;
}

void xm_media_recorder_freep(XMMediaRecorder **mr)
{
    LOGD("xm_media_recorder_freep()\n");
    int retval = xm_media_recorder_freep_l(mr);
    LOGD("xm_media_recorder_freep()=%d\n", retval);
}

void xm_media_recorder_msg_thread_exit(XMMediaRecorder *mr)
{
    if(!mr)
        return;

    msg_queue_abort(&mr->msg_queue);
    if (mr->msg_thread) {
        SDL_WaitThread(mr->msg_thread, NULL);
        mr->msg_thread = NULL;
    }
}

static int xm_media_recorder_stop_l(XMMediaRecorder *mr)
{
    if(!mr)
        return -1;

    MPST_RET_IF_EQ(mr->mr_state, MR_STATE_UNINIT);
    MPST_RET_IF_EQ(mr->mr_state, MR_STATE_INITIALIZED);
    //MPST_RET_IF_EQ(mr->mr_state, MR_STATE_ASYNC_PREPARING);
    //MPST_RET_IF_EQ(mr->mr_state, MR_STATE_PREPARED);
    //MPST_RET_IF_EQ(mr->mr_state, MR_STATE_STARTED);
    MPST_RET_IF_EQ(mr->mr_state, MR_STATE_COMPLETED);
    MPST_RET_IF_EQ(mr->mr_state, MR_STATE_STOPPED);
    //MPST_RET_IF_EQ(mr->mr_state, MR_STATE_ERROR);

    IEncoder_stop(mr->AEncoder);
    IEncoder_stop(mr->VEncoder);
    muxer_stop(mr->mm);

    xmmr_notify_msg1(&mr->msg_queue, MR_MSG_STOPPED);
    return 0;
}

void xm_media_recorder_stop(XMMediaRecorder *mr)
{
    assert(mr);
    LOGD("xm_media_recorder_stop()\n");

    if(mr->mr_state == MR_STATE_STOPPED) {
        LOGD("%s mr->mr_state == MR_STATE_STOPPED, exit\n", __func__);
        return;
    }

    pthread_mutex_lock(&mr->mutex);
    xmmr_change_state_l(mr, MR_STATE_REQ_STOP);
    pthread_mutex_unlock(&mr->mutex);
    xmmr_notify_msg1(&mr->msg_queue, MR_REQ_STOP);
}

int xm_media_recorder_queue_sizes(XMMediaRecorder *mr)
{
    if(mr == NULL) {
        LOGE("mr is NULL, xm_media_recorder_queue_sizes return\n");
        return 0;
    }

    return IEncoder_queue_sizes(mr->VEncoder);
}

void xm_media_recorder_put(XMMediaRecorder *mr, const unsigned char *rgba, int w, int h,
        int pixelStride, int rowPadding, int rotate_degree, bool flipHorizontal, bool flipVertical)
{
    assert(mr);
    if(xmmr_chkst_put_l(mr->mr_state) != 0)
    {
        LOGE("xmmr_chkst_put_l fail(), xm_media_recorder_put return\n");
        return;
    }

    if(xm_media_recorder_queue_sizes(mr) > XMMR_MAX_QUEUE_SIZES) {
        LOGE("queue_sizes than the max value, this frame is discarded\n");
        return;
    }

    IEncoder_QueueData qdata;
    qdata.rgba_data.w = w;
    qdata.rgba_data.h = h;
    qdata.rgba_data.rotate_degree = rotate_degree;
    qdata.rgba_data.flipHorizontal = flipHorizontal;
    qdata.rgba_data.flipVertical = flipVertical;
    qdata.rgba_data.rgba_size = RGBA_CHANNEL*w*h;
    qdata.rgba_data.processed = false;

    //release in encoder thread after rgba_queue_get
    qdata.rgba_data.rgba = (unsigned char *)av_mallocz(sizeof(char)*qdata.rgba_data.rgba_size);
    if(!qdata.rgba_data.rgba)
    {
        LOGE("mallocz rgba failed");
        xmmr_notify_msg1(&mr->msg_queue, MR_MSG_ERROR);
        return;
    }

    int offset = 0;
    for (int i = 0; i < h; ++i) {
        for (int j = 0; j < w; ++j) {
            qdata.rgba_data.rgba[(i*w + j) * RGBA_CHANNEL + 3] = (rgba[offset + 3] & 0xff); // A
            qdata.rgba_data.rgba[(i*w + j) * RGBA_CHANNEL + 2] = (rgba[offset + 2] & 0xff); // B
            qdata.rgba_data.rgba[(i*w + j) * RGBA_CHANNEL + 1] = (rgba[offset + 1] & 0xff); // G
            qdata.rgba_data.rgba[(i*w + j) * RGBA_CHANNEL + 0] = (rgba[offset] & 0xff); // R
            offset += pixelStride;
        }
        offset += rowPadding;
    }
    //IEncoder_QueueData temp = qdata;
    //temp.rgba_data.rgba = rgba;
    //RgbaProcess(qdata.rgba_data.rgba, &temp, &qdata.rgba_data.processed);
    //memcpy(qdata.rgba_data.rgba, rgba, qdata.rgba_data.rgba_size);
    if(IEncoder_enqueue(mr->AEncoder, &qdata) < 0
        ||IEncoder_enqueue(mr->VEncoder, &qdata) < 0)
    {
        if(xmmr_chkst_put_l(mr->mr_state) == 0)
        {
            LOGE("IEncoder_enqueue failed");
            xmmr_notify_msg1(&mr->msg_queue, MR_MSG_ERROR);
        }
    } else {
        if(mr->start_mux)
        {
            pthread_mutex_lock(&mr->mutex);
            mr->start_mux = false;
            pthread_mutex_unlock(&mr->mutex);
            if(muxer_startAsync(mr->mm) < 0) {
                xmmr_notify_msg1(&mr->msg_queue, MR_MSG_ERROR);
            }
        }
    }
}

static int xm_media_recorder_start_l(XMMediaRecorder *mr)
{
    if(!mr)
        return -1;

    MPST_RET_IF_EQ(mr->mr_state, MR_STATE_UNINIT);
    MPST_RET_IF_EQ(mr->mr_state, MR_STATE_INITIALIZED);
    MPST_RET_IF_EQ(mr->mr_state, MR_STATE_ASYNC_PREPARING);
    //MPST_RET_IF_EQ(mr->mr_state, MR_STATE_PREPARED);
    MPST_RET_IF_EQ(mr->mr_state, MR_STATE_STARTED);
    MPST_RET_IF_EQ(mr->mr_state, MR_STATE_COMPLETED);
    MPST_RET_IF_EQ(mr->mr_state, MR_STATE_STOPPED);
    MPST_RET_IF_EQ(mr->mr_state, MR_STATE_ERROR);

    IEncoder_startAsync(mr->VEncoder);
    IEncoder_startAsync(mr->AEncoder);
    mr->start_mux = true;
    return 0;
}

void xm_media_recorder_start(XMMediaRecorder *mr)
{
    assert(mr);
    LOGD("xm_media_recorder_start()\n");
    pthread_mutex_lock(&mr->mutex);
    xmmr_change_state_l(mr, MR_STATE_REQ_START);
    pthread_mutex_unlock(&mr->mutex);
    xmmr_notify_msg1(&mr->msg_queue, MR_REQ_START);
}

static int xm_media_recorder_prepareAsync_l(XMMediaRecorder *mr)
{
    assert(mr);

    IEncoder_init(mr->VEncoder);
    IEncoder_init(mr->AEncoder);

    if(xmmr_chkst_restart_l(mr->mr_state) == 0) {
         xmmr_change_state_l(mr, MR_STATE_ASYNC_PREPARING);
         msg_queue_start(&mr->msg_queue);
         IEncoder_config(mr->VEncoder, &mr->config);
         IEncoder_config(mr->AEncoder, &mr->config);
         muxer_config(mr->mm, &mr->config);
         LOGD("xmmr restart\n");
         return 0;
    }

    MPST_RET_IF_EQ(mr->mr_state, MR_STATE_UNINIT);
    // MPST_RET_IF_EQ(mr->mr_state, MR_STATE_INITIALIZED);
    MPST_RET_IF_EQ(mr->mr_state, MR_STATE_ASYNC_PREPARING);
    MPST_RET_IF_EQ(mr->mr_state, MR_STATE_PREPARED);
    MPST_RET_IF_EQ(mr->mr_state, MR_STATE_STARTED);
    MPST_RET_IF_EQ(mr->mr_state, MR_STATE_COMPLETED);
    MPST_RET_IF_EQ(mr->mr_state, MR_STATE_STOPPED);
    MPST_RET_IF_EQ(mr->mr_state, MR_STATE_ERROR);

    xmmr_change_state_l(mr, MR_STATE_ASYNC_PREPARING);

    msg_queue_start(&mr->msg_queue);

    // released in msg_loop
    xmmr_inc_ref(mr);
    mr->msg_thread = SDL_CreateThreadEx(&mr->_msg_thread, xmmr_msg_loop, mr, "xmmr_msg_loop");
    // msg_thread is detached inside msg_loop

    IEncoder_config(mr->VEncoder, &mr->config);
    IEncoder_config(mr->AEncoder, &mr->config);
    muxer_config(mr->mm, &mr->config);

    return 0;
}

int xm_media_recorder_prepareAsync(XMMediaRecorder *mr)
{
    assert(mr);
    LOGD("xm_media_recorder_prepareAsync()\n");
    pthread_mutex_lock(&mr->mutex);
    int retval = xm_media_recorder_prepareAsync_l(mr);
    pthread_mutex_unlock(&mr->mutex);
    LOGD("xm_media_recorder_prepareAsync()=%d\n", retval);
    return retval;
}

void xm_media_recorder_initConfigParams(XMMediaRecorder *mr)
{
    if(!mr)
        return;

    if(mr->config.output_filename)
        av_free(mr->config.output_filename);
    mr->config.output_filename = NULL;

    if(mr->config.preset)
        av_free(mr->config.preset);
    mr->config.preset = NULL;

    if(mr->config.tune)
        av_free(mr->config.tune);
    mr->config.tune = NULL;

    mr->config.w = 1280;
    mr->config.h = 720;
    mr->config.bit_rate = 700000;
    mr->config.fps = 15;
    mr->config.gop_size = mr->config.fps * 5;
    mr->config.crf = 23;
    mr->config.multiple = 1000;
    mr->config.max_b_frames = 0;
    mr->config.CFR = false;
    mr->config.time_base = (AVRational) {1, mr->config.fps * mr->config.multiple};
    mr->config.pix_format = AV_PIX_FMT_YUV420P;
    mr->config.mime = MIME_VIDEO_AVC;
    mr->config.codec_id = AV_CODEC_ID_H264;
}

bool xm_media_recorder_setConfigParams(XMMediaRecorder *mr, const char *key, const char *value)
{
    if(!mr || !key || !value)
        return false;

    if (!strcasecmp(key, "width")) {
        mr->config.w = IJKALIGN(atoi(value), 2);
        LOGD("config.width %d\n", mr->config.w);
    } else if (!strcasecmp(key, "height")) {
        mr->config.h = IJKALIGN(atoi(value), 2);
        LOGD("config.height %d\n", mr->config.h);
    } else if (!strcasecmp(key, "bit_rate")) {
        mr->config.bit_rate = atoi(value);
    } else if (!strcasecmp(key, "fps")) {
        mr->config.fps = atoi(value);
        LOGD("config.fps %d\n", mr->config.fps);
        mr->config.time_base = (AVRational) {1, mr->config.fps * mr->config.multiple};
    } else if (!strcasecmp(key, "gop_size")) {
        mr->config.gop_size = atoi(value);
        LOGD("config.gop_size %d\n", mr->config.gop_size);
    } else if (!strcasecmp(key, "crf")) {
        mr->config.crf = atoi(value);
    } else if (!strcasecmp(key, "multiple")) {
        mr->config.multiple = atoi(value);
        mr->config.time_base = (AVRational) {1, mr->config.fps * mr->config.multiple};
    } else if (!strcasecmp(key, "max_b_frames")) {
        mr->config.max_b_frames = atoi(value);
    } else if (!strcasecmp(key, "CFR")) {
        mr->config.CFR = atoi(value) == 0 ? false : true;
        LOGD("config.CFR %d\n", mr->config.CFR);
    } else if (!strcasecmp(key, "output_filename")) {
        if(mr->config.output_filename)
            av_free(mr->config.output_filename);
        mr->config.output_filename = av_strdup(value);
        LOGD("config.output_filename %s\n", mr->config.output_filename);
    } else if (!strcasecmp(key, "preset")) {
        if(mr->config.preset)
            av_free(mr->config.preset);
        mr->config.preset = av_strdup(value);
        LOGD("config.preset %s\n", mr->config.preset);
    } else if (!strcasecmp(key, "tune")) {
        if(mr->config.tune)
            av_free(mr->config.tune);
        mr->config.tune = av_strdup(value);
        LOGD("config.tune %s\n", mr->config.tune);
    }

    return true;
}

XMMediaRecorder *xm_media_recorder_create(int(*msg_loop)(void*), bool useSoftEncoder, bool audioEnable, bool videoEnable)
{
    XMMediaRecorder *mr = (XMMediaRecorder *)av_mallocz(sizeof(XMMediaRecorder));
    if (!mr)
        return NULL;

    mr->mm = muxer_create(mr);
    if(!mr->mm) {
        av_free(mr);
        return NULL;
    }
    mr->mm->msg_queue = &mr->msg_queue;

    if(audioEnable)
    {
        mr->AEncoder = xm_create_encoder(XM_ENCODER_AUDIO, useSoftEncoder, &(mr->mm->audioq));
        if(mr->AEncoder)
            mr->AEncoder->msg_queue = &mr->msg_queue;
    }
    if(videoEnable)
    {
        mr->VEncoder = xm_create_encoder(XM_ENCODER_VIDEO, useSoftEncoder, &(mr->mm->videoq));
        if(mr->VEncoder)
            mr->VEncoder->msg_queue = &mr->msg_queue;
    }

    mr->msg_loop = msg_loop;
    pthread_mutex_init(&mr->mutex, NULL);
    msg_queue_init(&mr->msg_queue);
    xmmr_inc_ref(mr);
    xmmr_change_state_l(mr, MR_STATE_INITIALIZED);

    return mr;
}

void xm_media_recorder_ffmpeg_init()
{
    LOGD("xm_media_recorder_ffmpeg_init\n");

    av_register_all();
}

