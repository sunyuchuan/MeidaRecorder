#ifndef XM_RGBA_DATA_H
#define XM_RGBA_DATA_H
#include <stdbool.h>
#include <stddef.h>
#include "libavutil/frame.h"

enum DataFormat
{
    FORMAT_RGBA8888 = 1,
    FORMAT_YUY2 = 2
} DataFormat;

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
    enum DataFormat format;
} RgbaData;

RgbaData *rgba_data_creare(const unsigned char *rgba, int w, int h, int rotate_degree, bool flipHorizontal, bool flipVertical);
bool rgba_data_fill(RgbaData *data, const unsigned char *rgba, int w, int h, int rotate_degree, bool flipHorizontal, bool flipVertical);
void rgba_data_free(RgbaData *data);
void rgba_data_freep(RgbaData **data);

#endif
