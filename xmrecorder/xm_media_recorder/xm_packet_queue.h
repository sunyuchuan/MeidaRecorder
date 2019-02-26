#ifndef XM_PACKET_QUEUE_H
#define XM_PACKET_QUEUE_H
#include "libavformat/avformat.h"
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>

#define PKT_QUEUE_MAX_SIZE 4

typedef struct XMPacketQueue
{
    AVPacketList *mFirst;
    AVPacketList *mLast;
    int	 mNumbers;
    size_t mSize;
    bool mAbortRequest;
    pthread_mutex_t mLock;
    pthread_cond_t mCondition;
} XMPacketQueue;

void packet_queue_init(XMPacketQueue *queue);
void packet_queue_flush(XMPacketQueue *queue);
int packet_queue_put(XMPacketQueue *queue, AVPacket *pkt);
int packet_queue_get(XMPacketQueue *queue, AVPacket *pkt, bool block);
int packet_queue_size(XMPacketQueue *queue);
void packet_queue_abort(XMPacketQueue *queue);
void packet_queue_start(XMPacketQueue *queue);
void packet_queue_destroy(XMPacketQueue *queue);
void packet_queue_notify(XMPacketQueue *queue);
void packet_queue_waitOnNotify(XMPacketQueue *queue);
#endif
