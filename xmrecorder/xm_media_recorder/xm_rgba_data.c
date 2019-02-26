#include "xm_rgba_data.h"

RgbaData *rgba_data_creare(const unsigned char *rgba, int w, int h, int rotate_degree, bool flipHorizontal, bool flipVertical)
{
    if(!rgba)
        return NULL;

    RgbaData *data = (RgbaData *)calloc(1, sizeof(RgbaData));
    if (!data)
        return NULL;

    data->rgba_size = w*h*4;
    data->rgba = (unsigned char *)malloc(sizeof(char)*data->rgba_size);
    if(!data->rgba)
    {
        free(data);
        return NULL;
    }

    memcpy(data->rgba, rgba, data->rgba_size);
    data->w = w;
    data->h = h;
    data->rotate_degree = rotate_degree;
    data->flipHorizontal = flipHorizontal;
    data->flipVertical = flipVertical;
    return data;
}

bool rgba_data_fill(RgbaData *data, const unsigned char *rgba, int w, int h, int rotate_degree, bool flipHorizontal, bool flipVertical)
{
    if(!data || !rgba)
        return false;

    if(data->rgba)
        free(data->rgba);
    memset(data, 0, sizeof(RgbaData));

    data->rgba_size = w*h*4;
    data->rgba = (unsigned char *)malloc(sizeof(char)*data->rgba_size);
    if(!data->rgba)
    {
        return false;
    }

    memcpy(data->rgba, rgba, data->rgba_size);
    data->w = w;
    data->h = h;
    data->rotate_degree = rotate_degree;
    data->flipHorizontal = flipHorizontal;
    data->flipVertical = flipVertical;
    return true;
}

void rgba_data_free(RgbaData *data)
{
    if(!data)
        return;

    if(!data->rgba)
    {
        //free(data);
        return;
    }

    free(data->rgba);
    //free(data);
}

void rgba_data_freep(RgbaData **data)
{
    if(!data || !*data)
        return;

    rgba_data_free(*data);
    free(*data);
    *data = NULL;
}

