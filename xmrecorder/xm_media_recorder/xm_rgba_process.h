#ifndef XM_RGBA_PROCESS_H
#define XM_RGBA_PROCESS_H

#include "libyuv.h"

static void RgbaFlipVertical(unsigned char *output, const unsigned char *input, int w, int h)
{
    for(int i = 0; i < h; i++)
    {
        memcpy(output+i*w*RGBA_CHANNEL, input+(h -1 -i)*w*RGBA_CHANNEL, RGBA_CHANNEL*w);
    }
}

static void RgbaFlipHoriz(unsigned char *output, const unsigned char *input, int w, int h)
{
    for(int i = 0; i < h; i++)
    {
        for(int j = 0; j < w; j++)
        {
            output[(i*w + j)*RGBA_CHANNEL + 0] = input[(i*w + ((w - 1) - j))*RGBA_CHANNEL + 0];
            output[(i*w + j)*RGBA_CHANNEL + 1] = input[(i*w + ((w - 1) - j))*RGBA_CHANNEL + 1];
            output[(i*w + j)*RGBA_CHANNEL + 2] = input[(i*w + ((w - 1) - j))*RGBA_CHANNEL + 2];
            if(RGBA_CHANNEL == 4)
                output[(i*w + j)*RGBA_CHANNEL + 3] = input[(i*w + ((w - 1) - j))*RGBA_CHANNEL + 3];
        }
    }
}

__unused static bool RgbaProcess(unsigned char *output, RgbaData *data, bool *processed)
{
    bool ret = false;
    int dst_w = data->w;
    int dst_h = data->h;
    enum RotationMode mode = kRotate0;
    bool needRotate = true;
    unsigned char *flipInput = data->rgba;
    //unsigned char *flipOutput = output;
    unsigned char *rotateTemp = (unsigned char *)malloc(sizeof(char)*dst_w*dst_h*RGBA_CHANNEL);
    unsigned char *flipTemp = (unsigned char *)malloc(sizeof(char)*dst_w*dst_h*RGBA_CHANNEL);

    switch(data->rotate_degree) {
        case 90:
            mode = kRotate90;
            dst_w = data->h;
            dst_h = data->w;
            break;
        case 180:
            mode = kRotate180;
            break;
        case 270:
            mode = kRotate270;
            dst_w = data->h;
            dst_h = data->w;
            break;
        default:
            needRotate = false;
            break;
    }

    if(needRotate) {
        ret = true;
        if(!data->flipHorizontal && !data->flipVertical) {
            ARGBRotate(data->rgba, data->w * RGBA_CHANNEL, output, dst_w * RGBA_CHANNEL, data->w, data->h, mode);
        } else {
            ARGBRotate(data->rgba, data->w * RGBA_CHANNEL, rotateTemp, dst_w * RGBA_CHANNEL, data->w, data->h, mode);
            flipInput = rotateTemp;
        }
    }

    if(data->flipHorizontal) {
        ret = true;
	if(data->flipVertical) {
	    RgbaFlipHoriz(flipTemp, flipInput, data->w, data->h);
	    flipInput = flipTemp;
        } else {
            RgbaFlipHoriz(output, flipInput, data->w, data->h);
        }
    }

    if(data->flipVertical) {
        ret = true;
        RgbaFlipVertical(output, flipInput, data->w, data->h);
    }

    *processed = true;
    free(rotateTemp);
    free(flipTemp);
    return ret;
}

#endif
