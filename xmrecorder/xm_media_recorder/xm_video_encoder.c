#include "xm_video_encoder.h"
#include "xm_rgba_queue.h"
#include "xm_rgba_process.h"
#include "ijksdl/ijksdl_thread.h"

#include "pipeline/ff_encoder_sw.h"
#include "android/ff_encoder_mediacodec.h"

#define TAG "VideoEncoder"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG,TAG,__VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,TAG,__VA_ARGS__)

typedef struct MixFrame {
    AVFrame *frame;
    AVBufferRef *frame_buffer;
} MixFrame;

static void init_packet(AVPacket *pkt)
{
    av_init_packet(pkt);
    pkt->data = NULL;
    pkt->size = 0;
}

static AVFrame *obtain_frame_buffer(MixFrame *frame)
{
    if (frame->frame_buffer != NULL)
        return frame->frame;

    AVFrame *managed_frame = frame->frame;
    int buf_width = IJKALIGN(managed_frame->width, 16);
    int frame_bytes = av_image_get_buffer_size(managed_frame->format, buf_width, managed_frame->height, 1);
    AVBufferRef *frame_buffer_ref = av_buffer_alloc(frame_bytes);
    if (!frame_buffer_ref)
        return NULL;

    av_image_fill_arrays(managed_frame->data, managed_frame->linesize,
                         frame_buffer_ref->data, managed_frame->format, buf_width, managed_frame->height, 1);
    frame->frame_buffer  = frame_buffer_ref;
    return managed_frame;
}

static AVFrame *setup_frame(enum AVPixelFormat format, int width, int height)
{
    AVFrame *frame= av_frame_alloc();
    if (!frame) {
        return NULL;
    }
    frame->format = format;
    frame->width  = width;
    frame->height = height;
    av_image_fill_arrays(frame->data, frame->linesize ,NULL,
                    format, IJKALIGN(width, 16), height, 1);

    return frame;
}

typedef struct IEncoder_Opaque {
    RgbaQueue *queue;
    Encoder *video_encoder;
    XMPacketQueue *pktq;
    MixFrame src_frame;
    unsigned char *rgba;
    XMEncoderConfig config;
    int64_t cur_pts;
    int64_t last_time; //microseconds
    int64_t interval_pts;
    volatile bool abort;
    int64_t frame_num;
} IEncoder_Opaque;

static int VEncoder_queue_sizes(IEncoder_Opaque *opaque);
static int VEncoder_config_l(Encoder *encoder, XMEncoderConfig *config);
static void VEncoder_reset_l(IEncoder_Opaque *opaque);

static bool VEncoder_free(IEncoder_Opaque *opaque)
{
    if (!opaque)
        return false;

    if (opaque->src_frame.frame)
        av_frame_free(&opaque->src_frame.frame);
    opaque->src_frame.frame = NULL;
    if (opaque->src_frame.frame_buffer)
        av_buffer_unref(&opaque->src_frame.frame_buffer);
    opaque->src_frame.frame_buffer = NULL;
    if (opaque->rgba) {
        av_free(opaque->rgba);
    }
    opaque->rgba = NULL;

    if (opaque->video_encoder) {
        ff_encoder_free_p(&opaque->video_encoder);
    }
    opaque->video_encoder = NULL;

    rgba_queue_freep(&opaque->queue);
    return true;
}

static void VEncoder_start_l(IEncoder_Opaque *opaque)
{
    if(!opaque)
        return;

    opaque->abort = false;
    rgba_queue_start(opaque->queue);
}

static void VEncoder_abort_l(IEncoder_Opaque *opaque)
{
    if(!opaque)
        return;

    opaque->abort = true;
    rgba_queue_abort(opaque->queue);
}

static void VEncoder_stop(IEncoder_Opaque *opaque)
{
    if(!opaque)
        return;

    opaque->abort = true;
    if(VEncoder_queue_sizes(opaque) <= 0)
    {
        packet_queue_notify(opaque->pktq);
        rgba_queue_abort(opaque->queue);
        LOGD("VEncoder_stop abort");
    }
    //packet_queue_notify(opaque->pktq);
    //rgba_queue_abort(opaque->queue);
}

static void VEncoder_RgbaQueue_flush(IEncoder_Opaque *opaque)
{
    if(!opaque)
        return;

    rgba_queue_flush(opaque->queue);
}

static int VEncoder_queue_sizes(IEncoder_Opaque *opaque)
{
    if(!opaque)
        return 0;

    return rgba_queue_size(opaque->queue);
}

static int VEncoder_enqueue(IEncoder_Opaque *opaque, IEncoder_QueueData *qdata)
{
    int ret = -1;
    if(!opaque || !qdata)
        return ret;

    if(opaque->abort)
    {
        LOGE("VEncoder_enqueue abort\n");
        rgba_data_free(&qdata->rgba_data);
        return ret;
    }

    int cur_w = IJKALIGN(qdata->rgba_data.w, 2);
    int cur_h = IJKALIGN(qdata->rgba_data.h, 2);
    if (qdata->rgba_data.format == FORMAT_YUY2) {
        cur_w = qdata->rgba_data.w * 2;
    }

    if(opaque->config.w != cur_w || opaque->config.h != cur_h)
    {
        LOGE("rgba width or height changed, recorder exit, src w %d, h %d, cur w %d, h %d",
                    opaque->config.w, opaque->config.h, cur_w, cur_h);
        //opaque->config.w = qdata->rgba_data.w;
        //opaque->config.h = qdata->rgba_data.h;
        rgba_data_free(&qdata->rgba_data);
        //VEncoder_stop(opaque);
        return ret;
    }

    /*if(VEncoder_queue_sizes(opaque) > PKT_QUEUE_MAX_SIZE)
    {
        LOGD("VEncoder_queue_sizes > 4 drop");
        rgba_data_free(&qdata->rgba_data);
        return 0;
    }*/

    if(opaque->last_time == -1 && opaque->cur_pts == -1)
    {
        qdata->rgba_data.pts = 0;
        opaque->cur_pts = 0;
    } else {
        if(opaque->config.CFR) {
            qdata->rgba_data.pts = opaque->cur_pts + opaque->interval_pts;
        } else {
            int64_t num_timebase = (int64_t)((av_gettime() - opaque->last_time) / ((float)(1000000 * opaque->config.time_base.num) / (float)opaque->config.time_base.den));
            qdata->rgba_data.pts = opaque->cur_pts + num_timebase;
        }
        opaque->cur_pts = qdata->rgba_data.pts;
    }
    opaque->last_time = av_gettime();
    if((ret = rgba_queue_put(opaque->queue, &qdata->rgba_data)) < 0)
    {
        rgba_data_free(&qdata->rgba_data);
    }
    return ret;
}

static bool VEncoder_flush(IEncoder_Opaque *opaque)
{
    int got_packet = 0;
    int i = 0, ret = 0;

    for (got_packet = 1; got_packet; i++)
    {
        AVPacket pkt;
        init_packet(&pkt);
        ret = ff_encoder_encode_frame(opaque->video_encoder, NULL, &pkt, &got_packet);
        if (ret < 0) {
            LOGE("Error ff_encoder_encode_frame\n");
            return false;
        }

        if(got_packet) {
            /*if(packet_queue_size(opaque->pktq) > PKT_QUEUE_MAX_SIZE)
            {
                LOGD("packet_queue_size > 4, vencoder thread sleep\n");
                packet_queue_waitOnNotify(opaque->pktq);
            }*/
            if(packet_queue_put(opaque->pktq, &pkt) < 0)
            {
                opaque->frame_num --;
                av_packet_unref(&pkt);
            }
        }
    }

    return true;
}

static bool VEncoder_process(IEncoder_Opaque *opaque, RgbaData *data)
{
    if(!opaque || !data)
        return false;

    int ret, got_packet;
    AVPacket pkt;
    init_packet(&pkt);
    AVFrame *src_frame = obtain_frame_buffer(&opaque->src_frame);
    unsigned char *rgba = data->rgba;

    if(!data->processed) {
        if(RgbaProcess(opaque->rgba, data, &data->processed)) {
            rgba = opaque->rgba;
        } else {
            rgba = data->rgba;
        }
    }

    if(src_frame)
    {
        if(RGBA_CHANNEL == 4) {
            if(data->format == FORMAT_YUY2) {
                YUY2ToI420(rgba, 2 * data->w * 2,
                        src_frame->data[0], src_frame->linesize[0],
                        src_frame->data[1], src_frame->linesize[1],
                        src_frame->data[2], src_frame->linesize[2],
                        opaque->config.w, opaque->config.h);
            } else if (data->format == FORMAT_RGBA8888) {
                ABGRToI420(rgba, RGBA_CHANNEL * data->w,
                        src_frame->data[0], src_frame->linesize[0],
                        src_frame->data[1], src_frame->linesize[1],
                        src_frame->data[2], src_frame->linesize[2],
                        opaque->config.w, opaque->config.h);
            }
        } else {
            RGB24ToI420(rgba, RGBA_CHANNEL * data->w,
                    src_frame->data[0], src_frame->linesize[0],
                    src_frame->data[1], src_frame->linesize[1],
                    src_frame->data[2], src_frame->linesize[2],
                    opaque->config.w, opaque->config.h);
        }
    }

    src_frame->pts = data->pts;
    ret = ff_encoder_encode_frame(opaque->video_encoder, src_frame, &pkt, &got_packet);
    if (ret < 0) {
        LOGE("Error ff_encoder_encode_frame\n");
        return false;
    }
    opaque->frame_num ++;

    if(got_packet) {
        /*if(packet_queue_size(opaque->pktq) > PKT_QUEUE_MAX_SIZE)
        {
            LOGD("packet_queue_size > 4, vencoder thread sleep\n");
            packet_queue_waitOnNotify(opaque->pktq);
        }*/
        if(packet_queue_put(opaque->pktq, &pkt) < 0)
        {
            opaque->frame_num --;
            av_packet_unref(&pkt);
        }
    }

    return true;
}

static bool VEncoder_encode(IEncoder_Opaque *opaque)
{
    if(!opaque)
        return false;

    SDL_SetThreadPriority(SDL_THREAD_PRIORITY_HIGH);
    RgbaData data;
    opaque->frame_num = 0;
    while(1)
    {
        if(opaque->abort)
        {
            if(VEncoder_queue_sizes(opaque) <= 0)
            {
                packet_queue_notify(opaque->pktq);
                rgba_queue_abort(opaque->queue);
                LOGD("VEncoder_encode abort");
            }
        }

        if(rgba_queue_get(opaque->queue, &data, true) < 0) {
            LOGD("VEncoder_encode Queue can't get queue data & return");
            if(opaque->frame_num > 0) {
                LOGD("VEncoder_flush frame_num is %"PRId64"\n", opaque->frame_num);
                VEncoder_flush(opaque);
            }
            break;
        }

        if(!VEncoder_process(opaque, &data)) {
            LOGE("VEncoder_process ERROR & return");
            rgba_data_free(&data);
            break;
        }

        rgba_data_free(&data);//release xm_media_recorder_put malloc
    }
    VEncoder_reset_l(opaque);
    VEncoder_RgbaQueue_flush(opaque);
    VEncoder_abort_l(opaque);
    LOGD("VEncoder_encode thread end");
    return true;
}

static void VEncoder_reset_l(IEncoder_Opaque *opaque)
{
    if(!opaque)
        return;

    VEncoder_start_l(opaque);

    if (opaque->src_frame.frame)
        av_frame_free(&opaque->src_frame.frame);
    opaque->src_frame.frame = NULL;
    if (opaque->src_frame.frame_buffer)
        av_buffer_unref(&opaque->src_frame.frame_buffer);
    opaque->src_frame.frame_buffer = NULL;
    if (opaque->rgba) {
        av_free(opaque->rgba);
    }
    opaque->rgba = NULL;

    if (opaque->video_encoder) {
        ff_encoder_free_p(&opaque->video_encoder);
    }
    opaque->video_encoder = NULL;
}

static bool VEncoder_prepare(IEncoder_Opaque *opaque)
{
    if(!opaque)
        return false;

    VEncoder_reset_l(opaque);

    opaque->video_encoder = ff_encoder_sw_create();
    if(!opaque->video_encoder)
        return false;
    if(VEncoder_config_l(opaque->video_encoder, &opaque->config) < 0)
        return false;

    LOGD("frame width %d, height %d", opaque->config.w, opaque->config.h);
    opaque->src_frame.frame = setup_frame(AV_PIX_FMT_YUV420P, opaque->config.w, opaque->config.h);
    opaque->rgba = (unsigned char *)av_mallocz(opaque->config.w*opaque->config.h*RGBA_CHANNEL);
    if(!opaque->rgba)
    {
        LOGE("opaque->rgba calloc failed\n");
        return false;
    }
    return true;
}

static int VEncoder_config_l(Encoder *encoder, XMEncoderConfig *config)
{
    int ret = -1;
    if(!encoder || !config)
        return ret;

    AVDictionary *video_opt = NULL;
    av_dict_set(&video_opt, "mime", config->mime, 0);
    av_dict_set_int(&video_opt, "bit_rate", config->bit_rate, 0);
    av_dict_set_int(&video_opt, "width", config->w, 0);
    av_dict_set_int(&video_opt, "height", config->h, 0);
    av_dict_set_int(&video_opt, "pix_format", config->pix_format, 0);
    av_dict_set_int(&video_opt, "gop_size", config->gop_size, 0);
    av_dict_set_int(&video_opt, "framerate", config->fps, 0);
    if(config->preset != NULL) {
        av_dict_set(&video_opt, "preset", config->preset, 0);
    }
    if(config->tune != NULL) {
        av_dict_set(&video_opt, "tune", config->tune, 0);
    }
    av_dict_set_int(&video_opt, "crf", config->crf, 0);
    av_dict_set_int(&video_opt, "multiple", config->multiple, 0);
    av_dict_set_int(&video_opt, "max_b_frames", config->max_b_frames, 0);
    if ((ret = ff_encoder_config(encoder, video_opt)) < 0) {
        LOGE("encoder config error %s\n", av_err2str(ret));
    }

    return ret;
}

static int VEncoder_config(IEncoder_Opaque *opaque, XMEncoderConfig *config)
{
    int ret = -1;
    if(!opaque)
        return ret;

    opaque->interval_pts = av_rescale(1, config->time_base.den, config->time_base.num * config->fps);
    opaque->config = *config;

    return 0;
}

static void VEncoder_init(IEncoder_Opaque *opaque)
{
    if(!opaque)
        return;

    opaque->cur_pts = -1;
    opaque->last_time = -1;
    opaque->frame_num = 0;
    opaque->config.CFR = false;
    opaque->config.tune = NULL;
    opaque->config.preset = NULL;
}

IEncoder *VideoEncoder_create(bool useSoftEncoder, XMPacketQueue *pktq)
{
    IEncoder *encoder = IEncoder_create(sizeof(IEncoder_Opaque));
    if (!encoder)
        return NULL;

    IEncoder_Opaque *opaque = encoder->opaque;
    opaque->queue = rgba_queue_create();
    if (!opaque->queue)
    {
        IEncoder_freep(&encoder);
        return NULL;
    }
    opaque->pktq = pktq;

    encoder->func_stop = VEncoder_stop;
    encoder->func_enqueue = VEncoder_enqueue;
    encoder->func_flush = VEncoder_RgbaQueue_flush;
    encoder->func_queue_sizes = VEncoder_queue_sizes;
    encoder->func_prepare = VEncoder_prepare;
    encoder->func_encode = VEncoder_encode;
    encoder->func_free = VEncoder_free;
    encoder->func_config = VEncoder_config;
    encoder->func_init = VEncoder_init;
    LOGD("create");
    return encoder;
}

