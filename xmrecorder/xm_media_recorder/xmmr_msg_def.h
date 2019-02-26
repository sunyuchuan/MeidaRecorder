#ifndef XMMR_MSG_DEF_H
#define XMMR_MSG_DEF_H
#include "ff_ffmsg_queue.h"

#define MR_STATE_UNINIT  0
#define MR_STATE_INITIALIZED  1
#define MR_STATE_ASYNC_PREPARING 2
#define MR_STATE_PREPARED  3
#define MR_STATE_REQ_START  4
#define MR_STATE_STARTED  5
#define MR_STATE_REQ_STOP  6
#define MR_STATE_STOPPED  7
#define MR_STATE_COMPLETED  8
#define MR_STATE_ERROR  9

#define MR_MSG_FLUSH         0
#define MR_MSG_PREPARED      100
#define MR_MSG_STARTED       200
#define MR_MSG_STOPPED       300
#define MR_MSG_COMPLETED     400
#define MR_MSG_ERROR         500
#define MR_MSG_STATE_CHANGED 600

#define MR_REQ_START  700
#define MR_REQ_STOP  800

enum recorder_event_type {
    RECORDER_NOP = 0,
    RECORDER_PREPARED = 1,
    RECORDER_COMPLETED = 2,
    RECORDER_ERROR = 100,
    RECORDER_INFO = 200,
};

inline static void xmmr_notify_msg1(MessageQueue *msg_queue, int what) {
    if(msg_queue)
        msg_queue_put_simple3(msg_queue, what, 0, 0);
}

inline static void xmmr_notify_msg2(MessageQueue *msg_queue, int what, int arg1) {
    if(msg_queue)
        msg_queue_put_simple3(msg_queue, what, arg1, 0);
}

inline static void xmmr_notify_msg3(MessageQueue *msg_queue, int what, int arg1, int arg2) {
    if(msg_queue)
        msg_queue_put_simple3(msg_queue, what, arg1, arg2);
}

#endif
