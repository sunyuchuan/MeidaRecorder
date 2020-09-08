LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := ijkffmpeg-$(TARGET_ARCH_ABI)
LOCAL_SRC_FILES := $(MY_SHARED_LIBRARIES_OUTPUT_PATH)/libijkffmpeg-$(TARGET_ARCH_ABI).so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := ijksdl-$(TARGET_ARCH_ABI)
LOCAL_SRC_FILES := $(MY_SHARED_LIBRARIES_OUTPUT_PATH)/libijksdl-$(TARGET_ARCH_ABI).so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := ijkplayer-$(TARGET_ARCH_ABI)
LOCAL_SRC_FILES := $(MY_SHARED_LIBRARIES_OUTPUT_PATH)/libijkplayer-$(TARGET_ARCH_ABI).so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
LOCAL_CFLAGS += -mfloat-abi=softfp
endif

ifneq ($(TARGET_ARCH_ABI),armeabi)
LOCAL_CFLAGS += -DSUPPORT_OPENGLES30
LOCAL_LDLIBS += -lGLESv3
LOCAL_ARM_NEON := true
LOCAL_CFLAGS += -DSUPPORT_ARM_NEON
endif

LOCAL_CFLAGS += -std=c99
LOCAL_CFLAGS += -Wno-deprecated-declarations
LOCAL_LDLIBS += -llog -landroid

LOCAL_C_INCLUDES += $(LOCAL_PATH)
LOCAL_C_INCLUDES += $(realpath $(LOCAL_PATH)/../..)
LOCAL_C_INCLUDES += $(realpath $(LOCAL_PATH)/../../ijkplayer)
LOCAL_C_INCLUDES += $(realpath $(LOCAL_PATH)/../../ijkj4a)
LOCAL_C_INCLUDES += $(MY_APP_FFMPEG_INCLUDE_PATH)

ifeq ($(TARGET_ARCH_ABI),armeabi)
LOCAL_SRC_FILES += armv7a/xm_memcpy_neon.c
endif
ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
LOCAL_SRC_FILES += armv7a/xm_memcpy_neon.c
endif
ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
LOCAL_SRC_FILES += arm64/xm_memcpy_neon64.c
endif
ifeq ($(TARGET_ARCH_ABI),x86)
LOCAL_SRC_FILES += x86/xm_memcpy_neon_x86.c
endif
ifeq ($(TARGET_ARCH_ABI),x86_64)
LOCAL_SRC_FILES += x86/xm_memcpy_neon_x86.c
endif

LOCAL_SRC_FILES += xm_media_recorder_jni.c
LOCAL_SRC_FILES += xm_rgba_data.c
LOCAL_SRC_FILES += xm_rgba_queue.c
LOCAL_SRC_FILES += xm_thread.c
LOCAL_SRC_FILES += xm_iencoder.c
LOCAL_SRC_FILES += xm_video_encoder.c
LOCAL_SRC_FILES += xm_encoder_factory.c
LOCAL_SRC_FILES += xm_media_recorder.c
LOCAL_SRC_FILES += xm_media_muxer.c
LOCAL_SRC_FILES += xm_packet_queue.c

LOCAL_SHARED_LIBRARIES := ijkffmpeg-$(TARGET_ARCH_ABI) ijksdl-$(TARGET_ARCH_ABI) ijkplayer-$(TARGET_ARCH_ABI)

LOCAL_STATIC_LIBRARIES := yuv_static

LOCAL_MODULE := xmrecorder-$(TARGET_ARCH_ABI)

include $(BUILD_SHARED_LIBRARY)

include $(realpath $(LOCAL_PATH))/../../ijkyuv/Android.mk
