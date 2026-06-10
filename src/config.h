#ifndef SCWRAPPER_CONFIG_H
#define SCWRAPPER_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

// Mapping Type
#define MAPPING_TYPE_DISABLED    0
#define MAPPING_TYPE_GAMEPAD_BTN 1
#define MAPPING_TYPE_GAMEPAD_ABS 2
#define MAPPING_TYPE_MOUSE_BTN   3
// Gamepad Button
#define GAMEPAD_BTN_NORTH      0
#define GAMEPAD_BTN_SOUTH      1
#define GAMEPAD_BTN_EAST       2
#define GAMEPAD_BTN_WEST       3
#define GAMEPAD_BTN_DPAD_UP    4
#define GAMEPAD_BTN_DPAD_DOWN  5
#define GAMEPAD_BTN_DPAD_LEFT  6
#define GAMEPAD_BTN_DPAD_RIGHT 7
#define GAMEPAD_BTN_GRIPL      8
#define GAMEPAD_BTN_GRIPL2     9
#define GAMEPAD_BTN_GRIPR      10
#define GAMEPAD_BTN_GRIPR2     11
#define GAMEPAD_BTN_THUMB      12
#define GAMEPAD_BTN_THUMB2     13
#define GAMEPAD_BTN_THUMBL     14
#define GAMEPAD_BTN_THUMBR     15
#define GAMEPAD_BTN_TL         16
#define GAMEPAD_BTN_TL2        17
#define GAMEPAD_BTN_TR         18
#define GAMEPAD_BTN_TR2        19
#define GAMEPAD_BTN_BASE       20
#define GAMEPAD_BTN_MODE       21
#define GAMEPAD_BTN_SELECT     22
#define GAMEPAD_BTN_START      23
// Gamepad Absolute Axis
#define GAMEPAD_ABS_X  0
#define GAMEPAD_ABS_Y  1
#define GAMEPAD_ABS_Z  2
#define GAMEPAD_ABS_RX 3
#define GAMEPAD_ABS_RY 4
#define GAMEPAD_ABS_RZ 5
// Mouse Button
#define MOUSE_BTN_LEFT   0
#define MOUSE_BTN_RIGHT  1
#define MOUSE_BTN_MIDDLE 2

struct AbsRange {
    int32_t min;
    int32_t max;
};

// left
static AbsRange TRITON_ABS_RANGE_X;
static AbsRange TRITON_ABS_RANGE_Y;
static AbsRange TRITON_ABS_RANGE_Z;
// right
static AbsRange TRITON_ABS_RANGE_RX;
static AbsRange TRITON_ABS_RANGE_RY;
static AbsRange TRITON_ABS_RANGE_RZ;

struct Mapping {
    int32_t type;
    int32_t code;
};

struct Mappings {
    struct Mapping btn_a;
    struct Mapping btn_b;
    struct Mapping btn_x;
    struct Mapping btn_y;
    struct Mapping btn_dpad_up;
    struct Mapping btn_dpad_down;
    struct Mapping btn_dpad_left;
    struct Mapping btn_dpad_right;
    struct Mapping btn_l1;
    struct Mapping btn_l2;
    struct Mapping btn_l3;
    struct Mapping btn_l4;
    struct Mapping btn_l5;
    struct Mapping btn_r1;
    struct Mapping btn_r2;
    struct Mapping btn_r3;
    struct Mapping btn_r4;
    struct Mapping btn_r5;
    struct Mapping btn_steam;
    struct Mapping btn_menu;
    struct Mapping btn_view;
    struct Mapping btn_quick;
    struct Mapping btn_touchpadl;
    struct Mapping btn_touchpadr;
    struct Mapping axis_l2;
    struct Mapping axis_r2;
    struct Mapping axis_thumbstickl_x;
    struct Mapping axis_thumbstickl_y;
    struct Mapping axis_thumbstickr_x;
    struct Mapping axis_thumbstickr_y;
};

#define VIRTUAL_DEVICE_NAME_MAXLEN 256

// Virtual gamepad metadata
#define VIRTUAL_GAMEPAD_NAME    "Virtual Controller"
#define VIRTUAL_GAMEPAD_VID     0x0000
#define VIRTUAL_GAMEPAD_PID     0x0000
#define VIRTUAL_GAMEPAD_VERSION 1
// Virtual mouse metadata
#define VIRTUAL_MOUSE_NAME    "Virtual Mouse"
#define VIRTUAL_MOUSE_VID     0x0000
#define VIRTUAL_MOUSE_PID     0x0000
#define VIRTUAL_MOUSE_VERSION 1

struct VirtualDevice {
    char     name[VIRTUAL_DEVICE_NAME_MAXLEN];
    uint32_t vid;
    uint32_t pid;
    uint32_t version;
};

// 1-Pole Absolute Axis
#define DEFAULT_1POLE_ABS_MINIMUM 0
#define DEFAULT_1POLE_ABS_MAXIMUM 32767
#define DEFAULT_1POLE_ABS_FLIPPED false
// 2-Pole Absolute Axis
#define DEFAULT_2POLE_ABS_MINIMUM -32767
#define DEFAULT_2POLE_ABS_MAXIMUM 32767
#define DEFAULT_2POLE_ABS_FLIPPED false

struct VirtualAbs {
    int32_t minimum;
    int32_t maximum;
    bool    flipped;
};

struct Config {
    struct Mappings mappings;
    struct {
        struct VirtualDevice gamepad;
        struct VirtualDevice mouse;

        struct VirtualAbs abs_x;
        struct VirtualAbs abs_y;
        struct VirtualAbs abs_z;
        struct VirtualAbs abs_rx;
        struct VirtualAbs abs_ry;
        struct VirtualAbs abs_rz;
    } virtual;
};

extern struct Config config;

int Config_init(char const* const path);
bool Mapping_is_btn(struct Mapping const* const mapping, int32_t code);
bool Mapping_is_abs(struct Mapping const* const mapping, int32_t code);
bool Mappings_is_btn_used(int32_t code);
bool Mappings_is_abs_used(int32_t code);
int Mapping_send(
    struct Mapping const* const mapping,
    int ui_gamepad,
    int ui_mouse,
    int32_t value
);

#endif // SCWRAPPER_CONFIG_H
