#ifndef XM_ENCODER_FACTORY_H
#define XM_ENCODER_FACTORY_H
#include "xm_video_encoder.h"

enum xm_encoder_type {
    XM_ENCODER_NONE = -1,
    XM_ENCODER_VIDEO,
    XM_ENCODER_AUDIO,
    XM_ENCODER_SUBTITLE,
};

IEncoder *xm_create_encoder(enum xm_encoder_type type, bool useSoftEncoder, XMPacketQueue *pktq);
#endif
