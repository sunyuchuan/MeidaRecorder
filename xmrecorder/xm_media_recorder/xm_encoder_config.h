#ifndef XM_ENCODER_CONFIG_H
#define XM_ENCODER_CONFIG_H
#include "ff_ffplay_def.h"

typedef struct XMEncoderConfig {
    //video data
    int w;
    int h;
    int fps;
    int bit_rate;
    int gop_size;
    char *mime;
    char *output_filename;
    char *preset;
    char *tune;
    int crf;
    int multiple;
    int max_b_frames;
    enum AVPixelFormat pix_format;
    enum AVCodecID codec_id;
    AVRational time_base;
    //audio data
    //....
    //etc
} XMEncoderConfig;

#endif
