#ifndef XM_RGBA_QUEUE_H
#define XM_RGBA_QUEUE_H
#include "xm_rgba_data.h"
#include <pthread.h>

typedef struct RgbaDataList {
    RgbaData data;
    struct RgbaDataList *next;
} RgbaDataList;

typedef struct RgbaQueue
{
    RgbaDataList *mFirst;
    RgbaDataList *mLast;
    int	 mNumbers;
    size_t mSize;
    bool mAbortRequest;
    pthread_mutex_t mLock;
    pthread_cond_t mCondition;
} RgbaQueue;

RgbaQueue *rgba_queue_create();
void rgba_queue_flush(RgbaQueue *queue);
int rgba_queue_put(RgbaQueue *queue, RgbaData *data);
int rgba_queue_get(RgbaQueue *queue, RgbaData *data, bool block);
int rgba_queue_size(RgbaQueue *queue);
void rgba_queue_abort(RgbaQueue *queue);
void rgba_queue_start(RgbaQueue *queue);
void rgba_queue_free(RgbaQueue *queue);
void rgba_queue_freep(RgbaQueue **queue);

#endif
