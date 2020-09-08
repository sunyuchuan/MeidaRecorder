//
// Created by sunyc on 18-10-26.
//
#include "ijksdl/ijksdl_log.h"
#include "ijksdl/android/ijksdl_android_jni.h"
#include "ijksdl/ijksdl_misc.h"
#include "../ijkyuv/include/libyuv.h"
#include "../xm_media_recorder/xm_media_recorder.h"

#define TAG "xm_media_recorder_jni"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

#define JNI_CLASS_XM_MEDIA_RECORDER "com/xmly/media/camera/view/recorder/XMMediaRecorder"

typedef struct xm_media_recorder_fields_t {
    pthread_mutex_t mutex;
    jclass clazz;
    jfieldID field_mNativeXMMediaRecorder;
    jmethodID method_postEventFromNative;
} xm_media_recorder_fields_t;

static xm_media_recorder_fields_t g_clazz;
static JavaVM* g_jvm;
static XMMediaRecorder *jni_get_media_recorder(JNIEnv* env, jobject thiz);

static void
XMMediaRecorder_NV21toABGR(JNIEnv *env, jobject obj, jbyteArray yuv420sp, jint width, jint height, jbyteArray rgbaOut)
{
    unsigned char *rgbaData = (unsigned char *) ((*env)->GetPrimitiveArrayCritical(env, rgbaOut, 0));
    unsigned char *yuv = (unsigned char *) (*env)->GetPrimitiveArrayCritical(env, yuv420sp, 0);
    unsigned char *temp = (unsigned char *)av_mallocz(sizeof(char)*width*height*4);

    NV21ToARGB(yuv, width,
               yuv + width*height, ((width + 1) / 2)*2,
               temp, width*4,
               width, height);

    ARGBToABGR(temp, width*4,
               rgbaData, width*4,
               width, height);

    (*env)->ReleasePrimitiveArrayCritical(env, rgbaOut, rgbaData, 0);
    (*env)->ReleasePrimitiveArrayCritical(env, yuv420sp, yuv, 0);
    av_free(temp);
}

jlong jni_nativeXMMediaRecorder_get(JNIEnv *env, jobject thiz)
{
    return (*env)->GetLongField(env, thiz, g_clazz.field_mNativeXMMediaRecorder);
}

static void jni_nativeXMMediaRecorder_set(JNIEnv *env, jobject thiz, jlong value)
{
    (*env)->SetLongField(env, thiz, g_clazz.field_mNativeXMMediaRecorder, value);
}

inline static void
post_event(JNIEnv *env, jobject weak_this, int what, int arg1, int arg2)
{
    (*env)->CallStaticVoidMethod(env, g_clazz.clazz, g_clazz.method_postEventFromNative, weak_this, what, arg1, arg2, NULL);
}

static XMMediaRecorder *jni_get_media_recorder(JNIEnv* env, jobject thiz)
{
    pthread_mutex_lock(&g_clazz.mutex);

    XMMediaRecorder *mr = (XMMediaRecorder *) (intptr_t) jni_nativeXMMediaRecorder_get(env, thiz);
    if (mr) {
        xmmr_inc_ref(mr);
    }

    pthread_mutex_unlock(&g_clazz.mutex);
    return mr;
}

static XMMediaRecorder *jni_set_media_recorder(JNIEnv* env, jobject thiz, XMMediaRecorder *mr)
{
    pthread_mutex_lock(&g_clazz.mutex);

    XMMediaRecorder *oldmr = (XMMediaRecorder*) (intptr_t) jni_nativeXMMediaRecorder_get(env, thiz);
    if (mr) {
        xmmr_inc_ref(mr);
    }
    jni_nativeXMMediaRecorder_set(env, thiz, (intptr_t) mr);

    pthread_mutex_unlock(&g_clazz.mutex);

    // NOTE: xmmr_dec_ref may block thread
    if (oldmr != NULL) {
        xmmr_dec_ref_p(&oldmr);
    }

    return oldmr;
}

static void message_loop_n(JNIEnv *env, XMMediaRecorder *mr)
{
    jobject weak_thiz = (jobject) xmmr_get_weak_thiz(mr);
    JNI_CHECK_GOTO(weak_thiz, env, NULL, "mrjni: message_loop_n: null weak_thiz", LABEL_RETURN);

    while (1) {
        AVMessage msg;

        int retval = xmmr_get_msg(mr, &msg, 1);
        if (retval < 0)
            break;

        // block-get should never return 0
        assert(retval > 0);

        switch (msg.what) {
        case MR_MSG_FLUSH:
            LOGD("FFP_MSG_FLUSH\n");
            post_event(env, weak_thiz, RECORDER_NOP, 0, 0);
            break;
        case MR_MSG_ERROR:
            LOGD("MR_MSG_ERROR: 0x%x\n", msg.arg1);
            post_event(env, weak_thiz, RECORDER_ERROR, msg.arg1, msg.arg2);
            break;
        case MR_MSG_PREPARED:
            LOGD("MR_MSG_PREPARED:\n");
            post_event(env, weak_thiz, RECORDER_PREPARED, msg.arg1, msg.arg2);
            break;
        case MR_MSG_STARTED:
            LOGD("MR_MSG_STARTED: %d\n", msg.arg1);
            post_event(env, weak_thiz, RECORDER_INFO, MR_MSG_STARTED, msg.arg1);
            break;
        case MR_MSG_COMPLETED:
            LOGD("MR_MSG_COMPLETED:\n");
            post_event(env, weak_thiz, RECORDER_COMPLETED, msg.arg1, msg.arg2);
            break;
        case MR_MSG_STOPPED:
            LOGD("MR_MSG_STOPPED: %d\n", msg.arg1);
            post_event(env, weak_thiz, RECORDER_INFO, MR_MSG_STOPPED, msg.arg1);
            break;
        case MR_MSG_STATE_CHANGED:
            break;
        default:
            ALOGE("unknown MR_MSG_xxx(%d)\n", msg.what);
            break;
        }
    }

LABEL_RETURN:
    ;
}

static int message_loop(void *arg)
{
    LOGD("%s\n", __func__);

    JNIEnv *env = NULL;
    (*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL );

    XMMediaRecorder *mr = (XMMediaRecorder*) arg;
    JNI_CHECK_GOTO(mr, env, NULL, "mrjni: message_loop: null mr", LABEL_RETURN);

    message_loop_n(env, mr);

LABEL_RETURN:
    //inc when msg_loop thread is created
    xmmr_dec_ref_p(&mr);
    (*g_jvm)->DetachCurrentThread(g_jvm);

    LOGD("message_loop exit");
    return 0;
}

static void XMMediaRecorder_release(JNIEnv *env, jobject thiz)
{
    LOGD("%s\n", __func__);
    XMMediaRecorder *mr = jni_get_media_recorder(env, thiz);
    //JNI_CHECK_GOTO(mr, env, "java/lang/IllegalStateException", "mrjni: release: null mr", LABEL_RETURN);
    if(mr == NULL) {
        LOGI("XMMediaRecorder_release mr is NULL\n");
        goto LABEL_RETURN;
    }

    xm_media_recorder_msg_thread_exit(mr);
    (*env)->DeleteGlobalRef(env, (jobject)xmmr_set_weak_thiz(mr, NULL));
    jni_set_media_recorder(env, thiz, NULL);
LABEL_RETURN:
    xmmr_dec_ref_p(&mr);
}

static void XMMediaRecorder_native_finalize(JNIEnv *env, jobject thiz)
{
    XMMediaRecorder_release(env, thiz);
}

static void XMMediaRecorder_reset(JNIEnv *env, jobject thiz)
{
    XMMediaRecorder_release(env, thiz);
    //XMMediaRecorder_native_setup(env, thiz, weak_thiz);
}

static void XMMediaRecorder_stop(JNIEnv *env, jobject thiz)
{
    LOGD("%s\n", __func__);
    XMMediaRecorder *mr = jni_get_media_recorder(env, thiz);
    JNI_CHECK_GOTO(mr, env, "java/lang/IllegalStateException", "mrjni: stop: null mr", LABEL_RETURN);

    xm_media_recorder_stop(mr);
LABEL_RETURN:
    xmmr_dec_ref_p(&mr);
}

static int XMMediaRecorder_queue_sizes(JNIEnv* env, jobject thiz)
{
    int ret = 0;
    XMMediaRecorder *mr = jni_get_media_recorder(env, thiz);
    JNI_CHECK_GOTO(mr, env, "java/lang/IllegalStateException", "mrjni: queue_sizes: null mr", LABEL_RETURN);

    ret = xm_media_recorder_queue_sizes(mr);
LABEL_RETURN:
    xmmr_dec_ref_p(&mr);
    return ret;
}

static void XMMediaRecorder_put(JNIEnv* env, jobject thiz, jbyteArray rgba, jint w,
    jint h, jint pixelStride, jint rowPadding, jint rotate_degree, jboolean flipHorizontal, jboolean flipVertical)
{
    //LOGD("%s\n", __func__);
    XMMediaRecorder *mr = jni_get_media_recorder(env, thiz);
    JNI_CHECK_GOTO(mr, env, "java/lang/IllegalStateException", "mrjni: put: null mr", LABEL_RETURN);

    jbyte* rgbaData = (*env)->GetByteArrayElements(env, rgba, NULL);
    xm_media_recorder_put(mr, (const unsigned char *)rgbaData, w, h, pixelStride, rowPadding, rotate_degree, flipHorizontal, flipVertical);
    (*env)->ReleaseByteArrayElements(env, rgba, rgbaData, 0);
LABEL_RETURN:
    xmmr_dec_ref_p(&mr);
}

static void XMMediaRecorder_start(JNIEnv *env, jobject thiz)
{
    LOGD("%s\n", __func__);
    XMMediaRecorder *mr = jni_get_media_recorder(env, thiz);
    JNI_CHECK_GOTO(mr, env, "java/lang/IllegalStateException", "mrjni: start: null mr", LABEL_RETURN);

    xm_media_recorder_start(mr);
LABEL_RETURN:
    xmmr_dec_ref_p(&mr);
}

static void
XMMediaRecorder_prepareAsync(JNIEnv *env, jobject thiz)
{
    LOGD("%s\n", __func__);
    XMMediaRecorder *mr = jni_get_media_recorder(env, thiz);
    JNI_CHECK_GOTO(mr, env, "java/lang/IllegalStateException", "mrjni: prepareAsync: null mp", LABEL_RETURN);

    xm_media_recorder_prepareAsync(mr);
    //IJK_CHECK_MPRET_GOTO(retval, env, LABEL_RETURN);
LABEL_RETURN:
    xmmr_dec_ref_p(&mr);
}

static bool XMMediaRecorder_setConfigParams(JNIEnv *env, jobject thiz, jobject hashMap)
{
    LOGD("%s\n", __func__);
    bool ret = false;
    jclass mapClass = NULL;
    jobject set = NULL;
    jclass setClass = NULL;
    jobject iter = NULL;
    jclass iteratorClass = NULL;
    jclass entryClass = NULL;

    mapClass = (*env)->FindClass(env, "java/util/Map");
    if (mapClass == NULL) {
        goto end;
    }

    jmethodID entrySet = (*env)->GetMethodID(env, mapClass, "entrySet", "()Ljava/util/Set;");
    if (entrySet == NULL) {
        goto end;
    }

    set = (*env)->CallObjectMethod(env, hashMap, entrySet);
    if (set == NULL) {
        goto end;
    }

    setClass = (*env)->FindClass(env, "java/util/Set");
    if (setClass == NULL) {
        goto end;
    }

    jmethodID iterator = (*env)->GetMethodID(env, setClass, "iterator", "()Ljava/util/Iterator;");
    if (iterator == NULL) {
        goto end;
    }

    iter = (*env)->CallObjectMethod(env, set, iterator);
    if (iter == NULL) {
        goto end;
    }

    iteratorClass = (*env)->FindClass(env, "java/util/Iterator");
    if (iteratorClass == NULL) {
        goto end;
    }

    jmethodID hasNext = (*env)->GetMethodID(env, iteratorClass, "hasNext", "()Z");
    if (hasNext == NULL) {
        goto end;
    }

    jmethodID next = (*env)->GetMethodID(env, iteratorClass, "next", "()Ljava/lang/Object;");
    if (next == NULL) {
        goto end;
    }

    entryClass = (*env)->FindClass(env, "java/util/Map$Entry");
    if (entryClass == NULL) {
        goto end;
    }

    jmethodID getKey = (*env)->GetMethodID(env, entryClass, "getKey", "()Ljava/lang/Object;");
    if (getKey == NULL) {
        goto end;
    }

    jmethodID getValue = (*env)->GetMethodID(env, entryClass, "getValue", "()Ljava/lang/Object;");
    if (getValue == NULL) {
        goto end;
    }

    XMMediaRecorder *mr = jni_get_media_recorder(env, thiz);
    JNI_CHECK_GOTO(mr, env, "java/lang/IllegalStateException", "mrjni: start: null mr", LABEL_RETURN);

    xm_media_recorder_initConfigParams(mr);
    while ((*env)->CallBooleanMethod(env, iter, hasNext)) {
        jobject entry = (*env)->CallObjectMethod(env, iter, next);
        jstring key = (jstring) (*env)->CallObjectMethod(env, entry, getKey);
        jstring value = (jstring) (*env)->CallObjectMethod(env, entry, getValue);
        const char* keyStr = (*env)->GetStringUTFChars(env, key, NULL);
        if (!keyStr) {
            (*env)->DeleteLocalRef(env, value);
            (*env)->DeleteLocalRef(env, key);
            (*env)->DeleteLocalRef(env, entry);
            goto LABEL_RETURN;
        }

        const char* valueStr = (*env)->GetStringUTFChars(env, value, NULL);
        if (!valueStr) {
            (*env)->DeleteLocalRef(env, value);
            (*env)->ReleaseStringUTFChars(env, key, keyStr);
            (*env)->DeleteLocalRef(env, key);
            (*env)->DeleteLocalRef(env, entry);
            goto LABEL_RETURN;
        }

        ret = xm_media_recorder_setConfigParams(mr, keyStr, valueStr);
        (*env)->ReleaseStringUTFChars(env, value, valueStr);
        (*env)->DeleteLocalRef(env, value);
        (*env)->ReleaseStringUTFChars(env, key, keyStr);
        (*env)->DeleteLocalRef(env, key);
        (*env)->DeleteLocalRef(env, entry);
    }

LABEL_RETURN:
    xmmr_dec_ref_p(&mr);
end:
    if(entryClass != NULL)
        (*env)->DeleteLocalRef(env, entryClass);
    if(iteratorClass != NULL)
        (*env)->DeleteLocalRef(env, iteratorClass);
    if(iter != NULL)
        (*env)->DeleteLocalRef(env, iter);
    if(setClass != NULL)
        (*env)->DeleteLocalRef(env, setClass);
    if(set != NULL)
        (*env)->DeleteLocalRef(env, set);
    if(mapClass != NULL)
        (*env)->DeleteLocalRef(env, mapClass);
    return ret;
}

static void
XMMediaRecorder_native_setup(JNIEnv *env, jobject thiz, jobject weak_this, jboolean useSoftEncoder, jboolean audioEnable, jboolean videoEnable)
{
    LOGD("%s\n", __func__);
    XMMediaRecorder *mr = xm_media_recorder_create(message_loop, useSoftEncoder, audioEnable, videoEnable);
    JNI_CHECK_GOTO(mr, env, "java/lang/OutOfMemoryError", "mrjni: native_setup: xmmr_create() failed", LABEL_RETURN);

    jni_set_media_recorder(env, thiz, mr);
    xmmr_set_weak_thiz(mr, (*env)->NewGlobalRef(env, weak_this));
LABEL_RETURN:
    xmmr_dec_ref_p(&mr);
}

static JNINativeMethod g_methods[] = {
    { "native_setup",           "(Ljava/lang/Object;ZZZ)V",   (void *) XMMediaRecorder_native_setup },
    { "native_finalize",        "()V",                        (void *) XMMediaRecorder_native_finalize },
    { "_setConfigParams",       "(Ljava/util/HashMap;)Z",     (void *) XMMediaRecorder_setConfigParams },
    { "_start",                 "()V",                        (void *) XMMediaRecorder_start },
    { "_stop",                  "()V",                        (void *) XMMediaRecorder_stop },
    { "_put",                   "([BIIIIIZZ)V",               (void *) XMMediaRecorder_put },
    { "_queue_sizes",           "()I",                        (void *) XMMediaRecorder_queue_sizes },
    { "_reset",                 "()V",                        (void *) XMMediaRecorder_reset },
    { "_release",               "()V",                        (void *) XMMediaRecorder_release },
    { "_prepareAsync",          "()V",                        (void *) XMMediaRecorder_prepareAsync },
    { "NV21toABGR",             "([BII[B)V",                  (void *) XMMediaRecorder_NV21toABGR },
};

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved)
{
    JNIEnv* env = NULL;

    g_jvm = vm;
    if ((*vm)->GetEnv(vm, (void**) &env, JNI_VERSION_1_4) != JNI_OK) {
        return -1;
    }
    assert(env != NULL);

    pthread_mutex_init(&g_clazz.mutex, NULL );

    IJK_FIND_JAVA_CLASS(env, g_clazz.clazz, JNI_CLASS_XM_MEDIA_RECORDER);
    (*env)->RegisterNatives(env, g_clazz.clazz, g_methods, NELEM(g_methods));

    g_clazz.field_mNativeXMMediaRecorder = (*env)->GetFieldID(env, g_clazz.clazz, "mNativeXMMediaRecorder", "J");

    g_clazz.method_postEventFromNative = (*env)->GetStaticMethodID(env, g_clazz.clazz, "postEventFromNative", "(Ljava/lang/Object;IIILjava/lang/Object;)V");

    xm_media_recorder_ffmpeg_init();

    return JNI_VERSION_1_4;
}

JNIEXPORT void JNI_OnUnload(JavaVM *jvm, void *reserved)
{
    pthread_mutex_destroy(&g_clazz.mutex);
}

