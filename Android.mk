ifeq ($(USE_SENSOR_MEDIATION_HAL), true)
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := sensors.$(TARGET_BOARD_PLATFORM)

LOCAL_PROPRIETARY_MODULE := true

LOCAL_MODULE_RELATIVE_PATH := hw

LOCAL_CFLAGS := -DLOG_TAG=\"SensorsHal\" -Wall

LOCAL_SRC_FILES := sensor_hal.cpp
LOCAL_SRC_FILES += iio-client.cpp
LOCAL_SRC_FILES += custom-libiio-client/xml.c
LOCAL_SRC_FILES += custom-libiio-client/buffer.c
LOCAL_SRC_FILES += custom-libiio-client/context.c
LOCAL_SRC_FILES += custom-libiio-client/iiod-client.c
LOCAL_SRC_FILES += custom-libiio-client/lock.c
LOCAL_SRC_FILES += custom-libiio-client/channel.c
LOCAL_SRC_FILES += custom-libiio-client/backend.c
LOCAL_SRC_FILES += custom-libiio-client/device.c
LOCAL_SRC_FILES += custom-libiio-client/utilities.c
LOCAL_SRC_FILES += custom-libiio-client/network.c
LOCAL_SHARED_LIBRARIES := liblog libc libdl libxml2 libcutils
LOCAL_HEADER_LIBRARIES += libutils_headers libhardware_headers
LOCAL_CFLAGS += -Wno-unused-variable -Wno-unused-parameter -Wno-unused-function
LOCAL_C_INCLUDES := $(LOCAL_PATH) $(LOCAL_PATH)/custom-libiio-client
include $(BUILD_SHARED_LIBRARY)

endif
