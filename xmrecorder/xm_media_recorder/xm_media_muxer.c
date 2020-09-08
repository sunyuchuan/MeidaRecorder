#include "xm_media_muxer.h"
#include "ff_encoder.h"
#include "ijksdl/android/ijksdl_android_jni.h"
#include "ijksdl/ijksdl_thread.h"

#include <jni.h>
#include <libavcodec/avcodec.h>
#include <android/log.h>


#define TAG "xm_media_muxer"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG,TAG,__VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,TAG,__VA_ARGS__)

static void init_packet(AVPacket *pkt)
{
    av_init_packet(pkt);
    pkt->data = NULL;
    pkt->size = 0;
}

typedef struct StreamContext {
    int out_stream_index;
    AVCodecContext *enc_ctx;
    enum AVCodecID codec_id;
} StreamContext;

typedef struct MuxContext {
    AVFormatContext *ofmt_ctx;
    StreamContext audio_stream;
    StreamContext video_stream;
    int progress;
    int progress_now;
} MuxContext;

void muxer_MuxContext_free(MuxContext *mc)
{
    LOGD("mux_thread exit");
    //avcodec_close(mc->video_stream.enc_ctx);
    //avcodec_close(mc->audio_stream.enc_ctx);
    //av_free(mc->audio_stream.enc_ctx);
    if(mc->video_stream.enc_ctx)
        avcodec_free_context(&(mc->video_stream.enc_ctx));

    if (mc->ofmt_ctx && !(mc->ofmt_ctx->oformat->flags & AVFMT_NOFILE))
        avio_closep(&(mc->ofmt_ctx->pb));
    avformat_free_context(mc->ofmt_ctx);
}

void muxer_free(XMMediaMuxer *mm)
{
    if(!mm)
        return;

    muxer_stop(mm);
    packet_queue_destroy(&mm->videoq);
    packet_queue_destroy(&mm->audioq);
    pthread_mutex_destroy(&mm->mutex);
    pthread_cond_destroy(&mm->mCondition);
    LOGD("muxer_free");
}

void muxer_freep(XMMediaMuxer **mm)
{
    if(!mm || !*mm)
        return;

    muxer_free(*mm);
    free(*mm);
    *mm = NULL;
}

void muxer_notify(XMMediaMuxer *mm)
{
    if(!mm)
        return;

    pthread_mutex_lock(&mm->mutex);
    pthread_cond_signal(&mm->mCondition);
    pthread_mutex_unlock(&mm->mutex);
}

void muxer_waitOnNotify(XMMediaMuxer *mm)
{
    if(!mm)
        return;

    pthread_mutex_lock(&mm->mutex);
    pthread_cond_wait(&mm->mCondition, &mm->mutex);
    pthread_mutex_unlock(&mm->mutex);
}

void muxer_start(XMMediaMuxer *mm)
{
    if(!mm)
        return;

    packet_queue_flush(&mm->videoq);
    packet_queue_flush(&mm->audioq);
    packet_queue_start(&mm->videoq);
    packet_queue_start(&mm->audioq);

    pthread_mutex_lock(&mm->mutex);
    mm->abort_muxer = false;
    pthread_cond_signal(&mm->mCondition);
    pthread_mutex_unlock(&mm->mutex);
}

void muxer_abort(XMMediaMuxer *mm)
{
    if(!mm)
        return;

    if(packet_queue_size(&mm->videoq) <= 0 &&
        packet_queue_size(&mm->audioq) <= 0)
    {
        packet_queue_abort(&mm->videoq);
        packet_queue_abort(&mm->audioq);
        LOGD("muxer_abort abort");
    }

    pthread_mutex_lock(&mm->mutex);
    mm->abort_muxer = true;
    pthread_cond_signal(&mm->mCondition);
    pthread_mutex_unlock(&mm->mutex);
}

int muxer_wait(XMMediaMuxer *mm)
{
    if(!mm)
        return 0;

    if(!mm->mRunning)
    {
        return 0;
    }

    SDL_WaitThread(mm->mux_thread, NULL);
    mm->mRunning = false;
    return 0;
}

void muxer_stop(XMMediaMuxer *mm)
{
    if(!mm)
        return;

    muxer_abort(mm);

    int ret = -1;
    if((ret = muxer_wait(mm)) != 0)
        LOGD("Couldn't cancel mux_thread %d", ret);
}

static int write_video_packet(MuxContext *mc, AVPacket *pkt, XMEncoderConfig *config)
{
    int ret = 0;
    AVStream *out_stream = mc->ofmt_ctx->streams[mc->video_stream.out_stream_index];
    AVRational time_base = config->time_base;

    pkt->stream_index = mc->video_stream.out_stream_index;
    pkt->pts = av_rescale_q_rnd(pkt->pts, time_base, out_stream->time_base, AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
    pkt->dts = av_rescale_q_rnd(pkt->dts, time_base, out_stream->time_base, AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
    pkt->duration = av_rescale_q(pkt->duration, time_base, out_stream->time_base);
    pkt->pos = -1;
    //av_packet_rescale_ts(pkt, time_base, out_stream->time_base);

    if((ret = av_interleaved_write_frame(mc->ofmt_ctx, pkt)) < 0)
    {
        LOGE("Error muxing video packet");
        return -1;
    }

    return 0;
}

static int write_frame(XMMediaMuxer *mm, MuxContext *mc)
{
    if(!mm || !mc)
        return -1;

    AVPacket pPacket;
    init_packet(&pPacket);

    while(1)
    {
        /*if(packet_queue_size(&mm->videoq) < PKT_QUEUE_MAX_SIZE)
        {
            packet_queue_notify(&mm->videoq);
        }*/

        if(mm->abort_muxer)
        {
            if(packet_queue_size(&mm->videoq) <= 0 &&
                packet_queue_size(&mm->audioq) <= 0)
            {
                packet_queue_abort(&mm->videoq);
                packet_queue_abort(&mm->audioq);
                LOGD("write_frame abort");
            }
        }

        if(packet_queue_get(&mm->videoq, &pPacket, true) < 0) {
            LOGE("mux_thread PacketQueue can't get packet & return");
            break;
        }

        if(write_video_packet(mc, &pPacket, &mm->config) < 0) {
            LOGE("write_video_packet ERROR & return");
            av_packet_unref(&pPacket);
            break;
        }

	av_packet_unref(&pPacket);
    }

    packet_queue_flush(&mm->videoq);
    packet_queue_flush(&mm->audioq);
    return 0;
}

static int add_stream(MuxContext *mc, StreamContext *stream, XMEncoderConfig *config)
{
    int ret = 0;
    AVStream *out_stream = NULL;
    AVCodec *enc = NULL;
    AVCodecContext *avctx = NULL;

    if(!(enc = avcodec_find_encoder(stream->codec_id)))
    {
        LOGE("avcodec_find_encoder fail");
        ret = AVERROR(EINVAL);
        goto end;
    }

    if(!(avctx = avcodec_alloc_context3(enc)))
    {
        LOGE("avcodec_alloc_context3 fail");
        ret = AVERROR(EINVAL);
        goto end;
    }

    avctx->codec_id = stream->codec_id;
    avctx->bit_rate = config->bit_rate;
    avctx->width = config->w;
    avctx->height = config->h;
    avctx->time_base = config->time_base;
    avctx->gop_size = config->gop_size;
    avctx->pix_fmt = config->pix_format;
    avctx->framerate = (AVRational){ config->fps, 1 };
    avctx->max_b_frames = config->max_b_frames;
    if(config->preset != NULL) {
        av_opt_set(avctx->priv_data, "preset", config->preset, 0);
    }
    if(config->tune != NULL) {
        av_opt_set(avctx->priv_data, "tune", config->tune, 0);
    }
    av_opt_set(mc->ofmt_ctx, "movflags", "faststart", AV_OPT_SEARCH_CHILDREN);
    av_opt_set_double(avctx->priv_data, "crf", (double)config->crf, 0);
    avctx->codec_tag = 0;
    if (mc->ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        avctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (avcodec_open2(avctx, enc, NULL) < 0) {
        LOGE("Could not open codec\n");
        goto end;
    }

    if(!(out_stream = avformat_new_stream(mc->ofmt_ctx, enc)))
    {
        LOGE("avformat_new_stream fail");
        ret = AVERROR_UNKNOWN;
        goto end;
    }
    out_stream->id = mc->ofmt_ctx->nb_streams - 1;
    stream->out_stream_index = out_stream->id;
    out_stream->time_base = config->time_base;

    if((ret = avcodec_parameters_from_context(out_stream->codecpar, avctx)) < 0)
    {
        LOGE("Could not initialize out_stream parameters\n");
        goto end;
    }

    out_stream->codec->codec_tag = 0;
    if (mc->ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        out_stream->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    mc->video_stream.enc_ctx = avctx;

end:
    return ret;
}

static int open_output_file(XMMediaMuxer *mm, MuxContext *mc)
{
    int ret = 0;
    XMEncoderConfig *config = &mm->config;
    if((ret = avformat_alloc_output_context2(&mc->ofmt_ctx, NULL, "mp4", config->output_filename)) < 0)
    {
        LOGE("Could not alloc output context %s", config->output_filename);
        goto end;
    }

    mc->video_stream.codec_id = config->codec_id;
    //add_stream(mc, &mc->audio_stream);
    add_stream(mc, &mc->video_stream, config);

    if(!(mc->ofmt_ctx->oformat->flags & AVFMT_NOFILE))
    {
        if((ret = avio_open(&mc->ofmt_ctx->pb, config->output_filename, AVIO_FLAG_WRITE)) < 0)
        {
            LOGE("Could not open output file %s", config->output_filename);
            goto end;
        }
    }

end:
    return ret;
}

static int mux_thread(void *arg)
{
    JNIEnv *env = NULL;
    int ret = 0;
    XMMediaMuxer *mm = NULL;
    MuxContext mc = { 0 };

    if (!arg)
        return -1;
    if (JNI_OK != SDL_JNI_SetupThreadEnv(&env)) {
        LOGE("xm_media_muxer: SetupThreadEnv failed\n");
        goto fail;
    }

    mm = (XMMediaMuxer *)arg;
    mm->mRunning = true;
    muxer_start(mm);
    LOGD("mux_thread running");

    SDL_SetThreadPriority(SDL_THREAD_PRIORITY_HIGH);

    if ((ret = open_output_file(mm, &mc)) < 0)
        goto fail;

    if((ret = avformat_write_header(mc.ofmt_ctx, NULL)) < 0)
    {
        LOGE("Error occurred when avformat_write_header");
        goto fail;
    }

    if((ret = write_frame(mm, &mc)) < 0)
        goto fail;

    if((ret = av_write_trailer(mc.ofmt_ctx)) < 0)
    {
        LOGE("Error occurred when av_write_trailer");
        goto fail;
    }

    mm->mRunning = false;
    muxer_MuxContext_free(&mc);
    return ret;

fail:
    xmmr_notify_msg1(mm->msg_queue, MR_MSG_ERROR);
    mm->mRunning = false;
    muxer_MuxContext_free(&mc);
    return ret;
}

int muxer_startAsync(XMMediaMuxer *mm)
{
    assert(mm);

    mm->mux_thread = SDL_CreateThreadEx(&mm->_mux_thread, mux_thread, mm, "xm_media_muxer thread");
    if (!mm->mux_thread) {
        av_log(NULL, AV_LOG_FATAL, "mux_thread SDL_CreateThread() failed : %s\n", SDL_GetError());
        muxer_freep(&mm);
        return -1;
    }

    return 0;
}

void muxer_config(XMMediaMuxer *mm, XMEncoderConfig *config)
{
    if(!mm || !config)
        return;

    mm->config = *config;
}

XMMediaMuxer *muxer_create(void *mr)
{
    XMMediaMuxer *mm = (XMMediaMuxer *)calloc(1, sizeof(XMMediaMuxer));
    if (!mm) {
        av_log(NULL, AV_LOG_ERROR, "Struct XMMediaMuxer malloc error!!!\n");
        return NULL;
    }

    mm->abort_muxer= false;
    mm->mRunning = false;
    mm->opaque = mr;
    pthread_mutex_init(&mm->mutex, NULL);
    pthread_cond_init(&mm->mCondition, NULL);
    packet_queue_init(&mm->videoq);
    packet_queue_init(&mm->audioq);
    return mm;
}

