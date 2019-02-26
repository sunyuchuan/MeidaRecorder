#ifndef XM_VIDEO_ENCODER_H
#define XM_VIDEO_ENCODER_H
#include "xm_iencoder.h"
#include "xm_packet_queue.h"

typedef struct VideoEncoder
{
    IEncoder *encoder;
} VideoEncoder;

IEncoder *VideoEncoder_create(bool useSoftEncoder, XMPacketQueue *pktq);

#endif
