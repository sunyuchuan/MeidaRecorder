LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
LOCAL_CFLAGS += -mfloat-abi=soft
endif
LOCAL_CFLAGS += -std=c99
LOCAL_CFLAGS += -Wno-deprecated-declarations
LOCAL_LDLIBS += -llog -landroid

LOCAL_C_INCLUDES += $(realpath $(LOCAL_PATH)/xm_media_recorder)
LOCAL_C_INCLUDES += $(realpath $(LOCAL_PATH)/..)
LOCAL_C_INCLUDES += $(realpath $(LOCAL_PATH)/../ijkplayer)
LOCAL_C_INCLUDES += $(realpath $(LOCAL_PATH)/../ijkj4a)
LOCAL_C_INCLUDES += $(MY_APP_FFMPEG_INCLUDE_PATH)

LOCAL_SRC_FILES += xm_media_recorder_jni.c
LOCAL_SRC_FILES += xm_media_recorder/xm_rgba_data.c
LOCAL_SRC_FILES += xm_media_recorder/xm_rgba_queue.c
LOCAL_SRC_FILES += xm_media_recorder/xm_thread.c
LOCAL_SRC_FILES += xm_media_recorder/xm_iencoder.c
LOCAL_SRC_FILES += xm_media_recorder/xm_video_encoder.c
LOCAL_SRC_FILES += xm_media_recorder/xm_encoder_factory.c
LOCAL_SRC_FILES += xm_media_recorder/xm_media_recorder.c
LOCAL_SRC_FILES += xm_media_recorder/xm_media_muxer.c
LOCAL_SRC_FILES += xm_media_recorder/xm_packet_queue.c

LOCAL_SHARED_LIBRARIES := ijkffmpeg-$(TARGET_ARCH_ABI) ijksdl-$(TARGET_ARCH_ABI) ijkplayer-$(TARGET_ARCH_ABI)

LOCAL_STATIC_LIBRARIES := yuv_static

LOCAL_MODULE := xmrecorder-$(TARGET_ARCH_ABI)

include $(BUILD_SHARED_LIBRARY)
