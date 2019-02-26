#include "xm_rgba_queue.h"

static bool rgba_data_isValid(RgbaData *data)
{
    if(!data || !data->rgba)
        return false;

    if(data->w <= 0 || data->h <= 0)
        return false;
    return true;
}

RgbaQueue *rgba_queue_create()
{
    RgbaQueue *queue = (RgbaQueue *)calloc(1, sizeof(RgbaQueue));
    if (!queue)
        return NULL;

    pthread_mutex_init(&queue->mLock, NULL);
    pthread_cond_init(&queue->mCondition, NULL);
    return queue;
}

void rgba_queue_flush(RgbaQueue *queue)
{
    if(!queue)
        return;

    RgbaDataList *data, *next;

    pthread_mutex_lock(&queue->mLock);
    for(data = queue->mFirst; data != NULL; data = next) {
        next = data->next;
        rgba_data_free(&data->data);
        free(data);
    }
    queue->mLast = NULL;
    queue->mFirst = NULL;
    queue->mNumbers = 0;
    queue->mSize = 0;
    pthread_mutex_unlock(&queue->mLock);
}

int rgba_queue_put(RgbaQueue *queue, RgbaData *data)
{
    RgbaDataList *dataList;

    if (!queue || !rgba_data_isValid(data))
        return -1;

    dataList = (RgbaDataList *)malloc(sizeof(RgbaDataList));
    if (!dataList)
        return -1;
    //memcpy(&dataList->data, data, sizeof(RgbaData));
    dataList->data = *data;
    dataList->next = NULL;

    pthread_mutex_lock(&queue->mLock);
    if(!queue->mLast) {
        queue->mFirst = dataList;
    } else {
        queue->mLast->next = dataList;
    }
    queue->mLast = dataList;
    queue->mNumbers++;
    queue->mSize += dataList->data.rgba_size + sizeof(*dataList);

    pthread_cond_signal(&queue->mCondition);
    pthread_mutex_unlock(&queue->mLock);
    return 0;
}

int rgba_queue_get(RgbaQueue *queue, RgbaData *data, bool block)
{
    RgbaDataList *dataList;
    int ret = -1;

    if(!queue || !data)
        return ret;

    pthread_mutex_lock(&queue->mLock);
    for(;;) {
        if (queue->mAbortRequest) {
            ret = -1;
            break;
        }

        dataList = queue->mFirst;
        if(dataList) {
            queue->mFirst = dataList->next;
            if (!queue->mFirst)
                queue->mLast = NULL;
            queue->mNumbers--;
            queue->mSize -= dataList->data.rgba_size + sizeof(*dataList);
            //memcpy(data, &dataList->data, sizeof(RgbaData));
            *data = dataList->data;
            free(dataList);
            ret = 1;
            break;
        } else if (!block) {
            ret = -1;
            break;
        } else {
            pthread_cond_wait(&queue->mCondition, &queue->mLock);
        }
    }
    pthread_mutex_unlock(&queue->mLock);
    return ret;
}

int rgba_queue_size(RgbaQueue *queue)
{
    if(!queue)
        return -1;

    pthread_mutex_lock(&queue->mLock);
    int size = queue->mNumbers;
    pthread_mutex_unlock(&queue->mLock);
    return size;
}

void rgba_queue_abort(RgbaQueue *queue)
{
    if(!queue)
        return;

    pthread_mutex_lock(&queue->mLock);
    queue->mAbortRequest = true;
    pthread_cond_signal(&queue->mCondition);
    pthread_mutex_unlock(&queue->mLock);
}

void rgba_queue_start(RgbaQueue *queue)
{
    if(!queue)
        return;

    pthread_mutex_lock(&queue->mLock);
    queue->mAbortRequest = false;
    pthread_cond_signal(&queue->mCondition);
    pthread_mutex_unlock(&queue->mLock);
}

void rgba_queue_free(RgbaQueue *queue)
{
    if(!queue)
        return;

    rgba_queue_flush(queue);
    pthread_mutex_destroy(&queue->mLock);
    pthread_cond_destroy(&queue->mCondition);
}

void rgba_queue_freep(RgbaQueue **queue)
{
    if(!queue || !*queue)
        return;

    rgba_queue_free(*queue);
    free(*queue);
    *queue = NULL;
}

