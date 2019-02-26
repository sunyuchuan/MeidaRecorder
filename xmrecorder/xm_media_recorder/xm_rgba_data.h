#ifndef XM_RGBA_DATA_H
#define XM_RGBA_DATA_H
#include <stdbool.h>
#include <stddef.h>
#include "libavutil/frame.h"

typedef struct RgbaData
{
    unsigned char *rgba;
    int w;
    int h;
    int rotate_degree;
    bool flipHorizontal;
    bool flipVertical;
    bool processed;
    size_t rgba_size;
    int64_t pts;
} RgbaData;

RgbaData *rgba_data_creare(const unsigned char *rgba, int w, int h, int rotate_degree, bool flipHorizontal, bool flipVertical);
bool rgba_data_fill(RgbaData *data, const unsigned char *rgba, int w, int h, int rotate_degree, bool flipHorizontal, bool flipVertical);
void rgba_data_free(RgbaData *data);
void rgba_data_freep(RgbaData **data);

#endif
