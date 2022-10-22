/*
 * Copyright (C) 2018 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "LightService"

#include <log/log.h>

#include "Light.h"

#include <fstream>

#define LCD_LED         "/sys/class/leds/lcd-backlight/brightness"
#define LED_IN_MODE_BLINK    "/sys/class/leds/nubia_led/blink_mode"
#define LED_IN_SWITCH        "/sys/class/leds/nubia_led/outn"
#define LED_IN_GRADE_PARA    "/sys/class/leds/nubia_led/grade_parameter"
#define LED_IN_FADE_PARA     "/sys/class/leds/nubia_led/fade_parameter"
#define BATTERY_STATUS_FILE       "/sys/class/power_supply/battery/status"
#define BATTERY_CAPACITY          "/sys/class/power_supply/battery/capacity"
#define BATTERY_STATUS_DISCHARGING  "Discharging"
#define BATTERY_STATUS_NOT_CHARGING "Not charging"
#define BATTERY_STATUS_CHARGING     "Charging"

#ifdef HOME_LED
/* start define home led */
#define BLINK_MODE_ON			1
#define BLINK_MODE_OFF			2
#define BLINK_MODE_BREATH		3
#define BLINK_MODE_BREATH_ONCE	6
#define BLINK_MODE_UNKNOWN		0xff
#define LED_CHANNEL_HOME	16
#define LED_CHANNEL_BUTTON	8
#define LED_GRADE_BUTTON			8
#define LED_GRADE_HOME				8
#define LED_GRADE_HOME_BATTERY_LOW	0
#define LED_GRADE_HOME_NOTIFICATION	6
#define LED_GRADE_HOME_BATTERY		6
/* start define home led */
#endif

#ifdef TOP_LED
/* start define top led */
#define BLINK_MODE_ON    3
#define BLINK_MODE_OFF   0
#define BLINK_MODE_CONST 1
#define NUBIA_LED_DISABLE 16
#define NUBIA_LED_RED     48
#define NUBIA_LED_GREEN   64
/* start define top led */
#endif

#define BREATH_SOURCE_NOTIFICATION	0x01
#define BREATH_SOURCE_BATTERY		0x02
#define BREATH_SOURCE_BUTTONS		0x04
#define BREATH_SOURCE_ATTENTION		0x08
#define BREATH_SOURCE_NONE			0x00

#define MAX_LED_BRIGHTNESS    255
#define MAX_LCD_BRIGHTNESS    4095

static int32_t active_status = 0;

enum battery_status {
    BATTERY_UNKNOWN = 0,
    BATTERY_LOW,
    BATTERY_FREE,
    BATTERY_CHARGING,
    BATTERY_FULL,
};
#ifdef HOME_LED
/* start define home led */
struct led_data
{
	int status;
	int min_grade;
	int fade_time;
	int fade_on_time;
	int fade_off_time;
};

static struct led_data current_home_led_status = {BLINK_MODE_UNKNOWN, -1, -1, -1, -1};		//status=BLINK_MODE_UNKNOWN, force write data on boot
static struct led_data current_button_led_status = {BLINK_MODE_UNKNOWN, -1, -1, -1, -1};		//status=BLINK_MODE_UNKNOWN, force write data on boot
/* start define home led */
#endif

namespace {
/*
 * Write value to path and close file.
 */
static void set(std::string path, std::string value) {
    std::ofstream file(path);

    if (!file.is_open()) {
        ALOGW("failed to write %s to %s", value.c_str(), path.c_str());
        return;
    }

    file << value;
}

static int get(std::string path) {
    std::ifstream file(path);
    int value;

    if (!file.is_open()) {
        ALOGW("failed to read from %s", path.c_str());
        return 0;
    }

    file >> value;
    return value;
}

static int readStr(std::string path, char *buffer, size_t size)
{

    std::ifstream file(path);

    if (!file.is_open()) {
        ALOGW("failed to read %s", path.c_str());
        return -1;
    }

    file.read(buffer, size);
    file.close();
    return 1;
}

static void set(std::string path, int value) {
    set(path, std::to_string(value));
}

static uint32_t getBrightness(const LightState& state) {
    uint32_t alpha, red, green, blue;

    /*
     * Extract brightness from AARRGGBB.
     */
    alpha = (state.color >> 24) & 0xFF;
    red = (state.color >> 16) & 0xFF;
    green = (state.color >> 8) & 0xFF;
    blue = state.color & 0xFF;

    /*
     * Scale RGB brightness if Alpha brightness is not 0xFF.
     */
    if (alpha != 0xFF) {
        red = red * alpha / 0xFF;
        green = green * alpha / 0xFF;
        blue = blue * alpha / 0xFF;
    }

    return (77 * red + 150 * green + 29 * blue) >> 8;
}

int getBatteryStatus()
{
    int err;

    char status_str[16];
    int capacity = 0;

    err = readStr(BATTERY_STATUS_FILE, status_str, sizeof(status_str));
    if (err <= 0) {
        ALOGI("failed to read battery status: %d", err);
        return BATTERY_UNKNOWN;
    }

    //ALOGI("battery status: %d, %s", err, status_str);

    capacity = get(BATTERY_CAPACITY);

    //ALOGI("battery capacity: %d", capacity);

    if (0 == strncmp(status_str, BATTERY_STATUS_CHARGING, 8)) {
        if (capacity < 90) {
            return BATTERY_CHARGING;
        } else {
            return BATTERY_FULL;
        }
    } else {
        if (capacity < 10) {
            return BATTERY_LOW;
        } else {
            return BATTERY_FREE;
        }
    }
}

static inline uint32_t scaleBrightness(uint32_t brightness, uint32_t maxBrightness) {
    return brightness * maxBrightness / 0xFF;
}

static inline uint32_t getScaledBrightness(const LightState& state, uint32_t maxBrightness) {
    return scaleBrightness(getBrightness(state), maxBrightness);
}

static void handleBacklight(const LightState& state) {
    uint32_t brightness = getScaledBrightness(state, MAX_LCD_BRIGHTNESS);
    set(LCD_LED, brightness);
}

#ifdef HOME_LED
/* start define home led */
inline int get_max(int a, int b)
{
	return a > b ? a : b;
}

/*
 * select a led
 * args:
 *	id: identify of led
 */
static void set_led_selected(int id)
{
	set(LED_IN_SWITCH, id);
}

/*
 * set min grade of a led
 * need to select a led using set_led_selected(id)
 * args:
 * 	min_grade: min grade of this led (brightness/min brightness in breath)
 */
static void set_led_current_grade(int min_grade)
{
	set(LED_IN_GRADE_PARA, min_grade);
}

static std::string getFadeStr(int fade_time, int on_time, int off_time) {
    std::string buf;

    buf = std::to_string(fade_time) + " " + std::to_string(on_time) + " " + std::to_string(off_time);

    return buf;
}

/*
 * set fade of a led
 * need to select a led using select_led_selected(id)
 * Warning: these args only take effect in mode BREATH_ONCE
 * args:
 *	fade_time
 *	on_time
 *	off_time
 */
static void set_led_current_fade(int fade_time, int on_time, int off_time)
{
	set(LED_IN_FADE_PARA, getFadeStr(fade_time, on_time, off_time));
}

/*
 * set mode of a led
 * need to select a led using select_led_selected(id)
 * args:
 *	mode: mode of this led
 */
static void set_led_current_mode(int mode)
{
	set(LED_IN_MODE_BLINK, mode);
}

/*
 * compare two led status
 * args:
 *	*led1, *led2: led_data
 * return:
 *	1: equal
 *	0: not equal
 */
static int compare_led_status(struct led_data *led1, struct led_data *led2)
{
	if (led1 == NULL || led2 == NULL) return false;
	if (led1->status != led2->status) return false;
	if (led1->min_grade != led2->min_grade) return false;
	if (led1->fade_time != led2->fade_time) return false;
	if (led1->fade_on_time != led2->fade_on_time) return false;
	if (led1->fade_off_time != led2->fade_off_time) return false;
	return true;
}

/*
 * copy led status
 * args:
 *	*to: copy to
 *	*from: copy from
 *	taken: is this status already be taken? affect status when status is BREATH_ONCE
 */
static void copy_led_status(struct led_data *to, struct led_data *from, int taken)
{
	if (to == NULL || from == NULL) return;
	if(taken && from->status == BLINK_MODE_BREATH_ONCE)		//after BREATH_ONCE, LED will always on
		to->status = BLINK_MODE_ON;
	else to->status = from->status;
	to->min_grade = from->min_grade;
	to->fade_time = from->fade_time;
	to->fade_on_time = from->fade_on_time;
	to->fade_off_time = from->fade_off_time;
}

/*
 * set home led status
 * args:
 *	*mode: led mode
 */
static void set_led_home_status(struct led_data *mode)
{
	if(mode == NULL) return;
	if( !compare_led_status(mode, &current_home_led_status) )
	{
		//ALOGE("Write Home Status");
		set_led_selected(LED_CHANNEL_HOME);
		if(mode->min_grade >= 0)
			set_led_current_grade(mode->min_grade);
		if(mode->fade_time >= 0)
			set_led_current_fade(mode->fade_time, mode->fade_on_time, mode->fade_off_time);
		set_led_current_mode(mode->status);
		copy_led_status(&current_home_led_status, mode, true);
	}
}

/*
 * set button led status
 * args:
 *	*mode: led mode
 */
static void set_led_button_status(struct led_data *mode)
{
	if(mode == NULL) return;
	if( !compare_led_status(mode, &current_button_led_status) )
	{
		//ALOGE("Write Button Status");
		set_led_selected(LED_CHANNEL_BUTTON);
		if(mode->min_grade >= 0)
			set_led_current_grade(mode->min_grade);
		//button led not support fade
		set_led_current_mode(mode->status);
		copy_led_status(&current_button_led_status, mode, true);
	}
}
/* start define home led */
#endif

static uint32_t setBreathLightLocked(uint32_t event_source, const LightState& state){
    uint32_t brightness = getScaledBrightness(state, MAX_LED_BRIGHTNESS);
#ifdef HOME_LED
/* start define home led */
    struct led_data home_status = {BLINK_MODE_OFF, -1, -1, -1, -1};
    struct led_data button_status = {BLINK_MODE_OFF, -1, -1, -1, -1};
/* start define home led */
#endif

    if (brightness > 0) {
        active_status |= event_source;
    } else {
        active_status &= ~event_source;
    }

#ifdef HOME_LED
/* start define home led */
    if(active_status == 0) //nothing, close all
    {
        set_led_home_status(&home_status);
        set_led_button_status(&button_status);
/* start define home led */
#endif

#ifdef TOP_LED
/* start define top led */
    if(active_status == 0) { //nothing, close all
        // disable green led
        set(LED_IN_SWITCH, NUBIA_LED_GREEN);
        set(LED_IN_MODE_BLINK, BLINK_MODE_OFF);
        // disable red led
        set(LED_IN_SWITCH, NUBIA_LED_RED);
        set(LED_IN_MODE_BLINK, BLINK_MODE_OFF);
        // set disable led
        set(LED_IN_SWITCH, NUBIA_LED_DISABLE);
        set(LED_IN_MODE_BLINK, BLINK_MODE_OFF);
        set(LED_IN_FADE_PARA, "0 0 0");
        set(LED_IN_GRADE_PARA, "100 255");
/* start define top led */
#endif
        return 0;
    }
#ifdef HOME_LED
/* start define home led */
    if(active_status & BREATH_SOURCE_BUTTONS) //button backlight, turn all on
    {
        home_status.status = BLINK_MODE_ON;
        home_status.min_grade = LED_GRADE_HOME;
		
        //Support the closing of small dots on both sides
        //char prop[PROPERTY_VALUE_MAX];
        //int rc = property_get("persist.nubiabutton.disabled", prop , "0");
        //if(strncmp(prop, "1", PROP_VALUE_MAX) == 0){
          //  button_status.status = BLINK_MODE_OFF;
          //  button_status.min_grade = LED_GRADE_BUTTON;
        //}else {
            button_status.status = BLINK_MODE_ON;
            button_status.min_grade = LED_GRADE_BUTTON;
       // }
    }

    if(active_status & BREATH_SOURCE_BATTERY) //battery status, set home on
    {
        home_status.status = BLINK_MODE_ON;
        home_status.min_grade = get_max(home_status.min_grade, LED_GRADE_HOME_BATTERY);
    }

    if( (active_status & BREATH_SOURCE_NOTIFICATION ) || (active_status & BREATH_SOURCE_ATTENTION)) //notification, set home breath
    {
        home_status.status = BLINK_MODE_BREATH;
        home_status.min_grade = get_max(home_status.min_grade, LED_GRADE_HOME_NOTIFICATION);
    }

    set_led_home_status(&home_status);
    set_led_button_status(&button_status);
/* start define home led */
#endif

#ifdef TOP_LED
/* start define top led */
    if(active_status & BREATH_SOURCE_BATTERY) { //battery status
	    int battery_state = getBatteryStatus();
	    if(battery_state == BATTERY_CHARGING || battery_state == BATTERY_LOW){
            set(LED_IN_SWITCH, NUBIA_LED_RED);
            set(LED_IN_FADE_PARA, "0 0 0");
            set(LED_IN_GRADE_PARA, "100 255");
            set(LED_IN_MODE_BLINK, BLINK_MODE_CONST);
        }else if (battery_state == BATTERY_FULL){
            set(LED_IN_SWITCH, NUBIA_LED_GREEN);
            set(LED_IN_FADE_PARA, "0 0 0");
            set(LED_IN_GRADE_PARA, "100 255");
            set(LED_IN_MODE_BLINK, BLINK_MODE_CONST);
        }

        return 0;
    }

    if( (active_status & BREATH_SOURCE_NOTIFICATION ) || (active_status & BREATH_SOURCE_ATTENTION)) { //notification, set home breath
        int32_t onMS = state.flashOnMs;
        int32_t offMS = state.flashOffMs;
        switch(onMS){
        case 5000:
            onMS = 5;
            break;
        case 2000:
            onMS = 4;
            break;
        case 1000:
            onMS = 3;
            break;
        case 500:
            onMS = 2;
            break;
        case 250:
            onMS = 1;
            break;
        default:
            onMS = 1;
        }

        switch(offMS){
        case 5000:
            offMS = 5;
            break;
        case 2000:
            offMS = 4;
            break;
        case 1000:
            offMS = 3;
            break;
        case 500:
            offMS = 2;
            break;
        case 250:
            offMS = 1;
            break;
        case 1:
            offMS = 0;
            break;
        default:
            offMS = 0;
        }
        std::string fade_params = std::to_string(offMS) + " " + std::to_string(onMS) + " " + std::to_string(onMS);
        set(LED_IN_SWITCH, NUBIA_LED_GREEN);
        set(LED_IN_FADE_PARA, fade_params);
        set(LED_IN_GRADE_PARA, "0 100");
        set(LED_IN_MODE_BLINK, BLINK_MODE_ON);
    }
/* start define top led */
#endif
    return 0;
}

static inline bool isLit(const LightState& state) {
    return state.color & 0x00ffffff;
}

#ifdef HOME_LED
/* start define home led */
static void handleButtons(const LightState& state) {
    setBreathLightLocked(BREATH_SOURCE_BUTTONS, state);
}
/* start define home led */
#endif

static void handleNotification(const LightState& state) {
    setBreathLightLocked(BREATH_SOURCE_NOTIFICATION, state);
}

static void handleBattery(const LightState& state){
    setBreathLightLocked(BREATH_SOURCE_BATTERY, state);
}

/* Keep sorted in the order of importance. */
static std::vector<LightBackend> backends = {
    { Type::ATTENTION, handleNotification },
    { Type::NOTIFICATIONS, handleNotification },
    { Type::BATTERY, handleBattery },
    { Type::BACKLIGHT, handleBacklight },
#ifdef HOME_LED
/* start define home led */
    { Type::BUTTONS, handleButtons },
/* start define home led */
#endif	
};

}  // anonymous namespace

namespace android {
namespace hardware {
namespace light {
namespace V2_0 {
namespace implementation {

Return<Status> Light::setLight(Type type, const LightState& state) {
    LightStateHandler handler;
    bool handled = false;

    /* Lock global mutex until light state is updated. */
    std::lock_guard<std::mutex> lock(globalLock);

    /* Update the cached state value for the current type. */
    for (LightBackend& backend : backends) {
        if (backend.type == type) {
            backend.state = state;
            handler = backend.handler;
        }
    }

    /* If no handler has been found, then the type is not supported. */
    if (!handler) {
        return Status::LIGHT_NOT_SUPPORTED;
    }

    /* Light up the type with the highest priority that matches the current handler. */
    for (LightBackend& backend : backends) {
        if (handler == backend.handler && isLit(backend.state)) {
            handler(backend.state);
            handled = true;
            break;
        }
    }

    /* If no type has been lit up, then turn off the hardware. */
    if (!handled) {
        handler(state);
    }

    return Status::SUCCESS;
}

Return<void> Light::getSupportedTypes(getSupportedTypes_cb _hidl_cb) {
    std::vector<Type> types;

    for (const LightBackend& backend : backends) {
        types.push_back(backend.type);
    }

    _hidl_cb(types);

    return Void();
}

}  // namespace implementation
}  // namespace V2_0
}  // namespace light
}  // namespace hardware
}  // namespace android
