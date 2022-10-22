
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SHARED_LIBRARIES := libgui_vendor
LOCAL_MODULE := libfui
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_VENDOR_MODULE := true
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := shim_NubiaImageAlgorithm.cpp
LOCAL_SHARED_LIBRARIES := libui libutils
LOCAL_MODULE := libNubiaImageAlgorithmShim
LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true
include $(BUILD_SHARED_LIBRARY)
