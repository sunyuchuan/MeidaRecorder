#include "xm_packet_queue.h"

void packet_queue_init(XMPacketQueue *queue)
{
    memset(queue, 0, sizeof(XMPacketQueue));
    queue->mAbortRequest = true;
    pthread_mutex_init(&queue->mLock, NULL);
    pthread_cond_init(&queue->mCondition, NULL);
}

static bool packet_isValid(AVPacket *pkt)
{
    if(!pkt || !pkt->data || pkt->size <= 0)
        return false;

    return true;
}

void packet_queue_flush(XMPacketQueue *queue)
{
    if(!queue)
        return;

    AVPacketList *pktList, *next;

    pthread_mutex_lock(&queue->mLock);
    for(pktList = queue->mFirst; pktList != NULL; pktList = next) {
        next = pktList->next;
        av_packet_unref(&pktList->pkt);
        av_freep(&pktList);
    }
    queue->mLast = NULL;
    queue->mFirst = NULL;
    queue->mNumbers = 0;
    queue->mSize = 0;
    pthread_mutex_unlock(&queue->mLock);
}

int packet_queue_put(XMPacketQueue *queue, AVPacket *pkt)
{
    AVPacketList *pktList;

    if (!queue || !packet_isValid(pkt))
        return -1;

    pktList = (AVPacketList *)malloc(sizeof(AVPacketList));
    if (!pktList)
        return -1;
    //memcpy(&pktList->pkt, pkt, sizeof(AVPacket));
    pktList->pkt = *pkt;
    pktList->next = NULL;

    pthread_mutex_lock(&queue->mLock);
    if(!queue->mLast) {
        queue->mFirst = pktList;
    } else {
        queue->mLast->next = pktList;
    }
    queue->mLast = pktList;
    queue->mNumbers++;
    queue->mSize += pktList->pkt.size + sizeof(*pktList);

    pthread_cond_signal(&queue->mCondition);
    pthread_mutex_unlock(&queue->mLock);
    return 0;
}

int packet_queue_get(XMPacketQueue *queue, AVPacket *pkt, bool block)
{
    AVPacketList *pktList;
    int ret = -1;

    if(!queue || !pkt)
        return ret;

    pthread_mutex_lock(&queue->mLock);
    for(;;) {
        if (queue->mAbortRequest) {
            ret = -1;
            break;
        }

        pktList = queue->mFirst;
        if(pktList) {
            queue->mFirst = pktList->next;
            if (!queue->mFirst)
                queue->mLast = NULL;
            queue->mNumbers--;
            queue->mSize -= pktList->pkt.size + sizeof(*pktList);
            //memcpy(pkt, &pktList->pkt, sizeof(AVPacket));
            *pkt = pktList->pkt;
            free(pktList);
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

int packet_queue_size(XMPacketQueue *queue)
{
    int size = -1;
    if(!queue)
        return size;

    pthread_mutex_lock(&queue->mLock);
    size = queue->mNumbers;
    pthread_mutex_unlock(&queue->mLock);
    return size;
}

void packet_queue_abort(XMPacketQueue *queue)
{
    if(!queue)
        return;

    pthread_mutex_lock(&queue->mLock);
    queue->mAbortRequest = true;
    pthread_cond_signal(&queue->mCondition);
    pthread_mutex_unlock(&queue->mLock);
}

void packet_queue_start(XMPacketQueue *queue)
{
    pthread_mutex_lock(&queue->mLock);
    queue->mAbortRequest = false;
    pthread_cond_signal(&queue->mCondition);
    pthread_mutex_unlock(&queue->mLock);
}

void packet_queue_destroy(XMPacketQueue *queue)
{
    if(!queue)
        return;

    packet_queue_flush(queue);
    pthread_mutex_destroy(&queue->mLock);
    pthread_cond_destroy(&queue->mCondition);
}

void packet_queue_notify(XMPacketQueue *queue)
{
    if(!queue)
        return;

    pthread_mutex_lock(&queue->mLock);
    pthread_cond_signal(&queue->mCondition);
    pthread_mutex_unlock(&queue->mLock);
}

void packet_queue_waitOnNotify(XMPacketQueue *queue)
{
    if(!queue)
        return;

    pthread_mutex_lock(&queue->mLock);
    pthread_cond_wait(&queue->mCondition, &queue->mLock);
    pthread_mutex_unlock(&queue->mLock);
}

