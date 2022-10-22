LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := android.hardware.light@2.0-service.nubia
LOCAL_INIT_RC := android.hardware.light@2.0-service.nubia.rc
LOCAL_VINTF_FRAGMENTS := android.hardware.light@2.0-service.nubia.xml
LOCAL_VENDOR_MODULE := true
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_SRC_FILES := \
    service.cpp \
    Light.cpp

LOCAL_SHARED_LIBRARIES := \
    liblog \
    libhardware \
    libhidlbase \
    libutils \
    android.hardware.light@2.0
    
# define led type for light 2.0 hidl service
ifeq ($(BOARD_HAVE_NUBIA_HOME_LED),true)
    LOCAL_CFLAGS += -DHOME_LED
else ifeq ($(BOARD_HAVE_NUBIA_TOP_LED),true)
    LOCAL_CFLAGS += -DTOP_LED
endif

include $(BUILD_EXECUTABLE)

include $(call all-makefiles-under,$(LOCAL_PATH))
