#include "xm_encoder_factory.h"

IEncoder *xm_create_encoder(enum xm_encoder_type type, bool useSoftEncoder, XMPacketQueue *pktq)
{
    IEncoder *encoder = NULL;
    switch(type)
    {
        case XM_ENCODER_VIDEO:
            encoder = VideoEncoder_create(useSoftEncoder, pktq);
            break;
        default:
            encoder = NULL;
            break;
    }

    return encoder;
}

