#include "config.h"
#include "constants.h"
#include "sc_string.h"

#include <iniparser.h>

#include <linux/input-event-codes.h>
#include <linux/uinput.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifndef BTN_GRIPL
#define BTN_GRIPL BTN_0
#endif

#ifndef BTN_GRIPR
#define BTN_GRIPR BTN_1
#endif

#ifndef BTN_GRIPL2
#define BTN_GRIPL2 BTN_2
#endif

#ifndef BTN_GRIPR2
#define BTN_GRIPR2 BTN_3
#endif

/// TODO: move me! VVV
static int timeval_now(struct timeval* const tv) {
    struct timespec ts;
    memset(&ts, 0, sizeof(ts));

    if (!clock_gettime(CLOCK_MONOTONIC, &ts)) {
        tv->tv_sec  = ts.tv_sec;
        tv->tv_usec = ts.tv_nsec / 1000;
        return RET_OKAY;
    }
    return RET_ERROR;
}

static int uinput_emit(
    int fd,
    uint16_t type,
    uint16_t code,
    int32_t value
) {
    struct input_event event = {
        .type  = type,
        .code  = code,
        .value = value,
    };

    struct timeval tv;
    if (timeval_now(&tv)) {
        return RET_ERROR;
    }
    event.input_event_sec  = tv.tv_sec;
    event.input_event_usec = tv.tv_usec;
    if (write(fd, &event, sizeof(event)) != sizeof(event)) {
        return RET_ERROR;
    }
    return RET_OKAY;
}
/// TODO: move me ^^^

static int Mapping_button_init(
    struct Mapping* mapping,
    char const* const val
);

static int Mapping_axis_init(
    struct Mapping* mapping,
    char const* const val
);

static int Mapping_match_disabled(
    struct Mapping* mapping,
    char const* const val
);

static int Mapping_match_button(
    struct Mapping* mapping,
    char const* const key,
    char const* const val,
    int32_t button
);

static int Mapping_match_mouse_button(
    struct Mapping* mapping,
    char const* const key,
    char const* const val,
    int32_t button
);

static int Mapping_match_axis(
    struct Mapping* mapping,
    char const* const key,
    char const* const val,
    int32_t button
);

static int ui_btn(
    int fd,
    int16_t code,
    bool value
);

static int ui_abs(
    struct VirtualAbs const* const abs,
    int fd,
    int16_t code,
    int16_t value
);

void cleanup_dictionary(dictionary** dict) {
    iniparser_freedict(*dict);
}

struct Config config = { 0 };

int Config_init(char const* const path) {
    memset(&config, 0, sizeof(config));

    dictionary *ini __attribute__((cleanup(cleanup_dictionary))) = iniparser_load(path);
    if (!ini) {
        return RET_ERROR;
    }

    // Mappings
    char const* btn_a          = iniparser_getstring(ini, "mappings:btn_a",          "btn_south");
    char const* btn_b          = iniparser_getstring(ini, "mappings:btn_b",          "btn_east");
    char const* btn_x          = iniparser_getstring(ini, "mappings:btn_x",          "btn_west");
    char const* btn_y          = iniparser_getstring(ini, "mappings:btn_y",          "btn_north");
    char const* btn_dpad_up    = iniparser_getstring(ini, "mappings:btn_dpad_up",    "btn_dpad_up");
    char const* btn_dpad_down  = iniparser_getstring(ini, "mappings:btn_dpad_down",  "btn_dpad_down");
    char const* btn_dpad_left  = iniparser_getstring(ini, "mappings:btn_dpad_left",  "btn_dpad_left");
    char const* btn_dpad_right = iniparser_getstring(ini, "mappings:btn_dpad_right", "btn_dpad_right");
    char const* btn_l1         = iniparser_getstring(ini, "mappings:btn_l1",         "btn_tl");
    char const* btn_l2         = iniparser_getstring(ini, "mappings:btn_l2",         "btn_tl2");
    char const* btn_l3         = iniparser_getstring(ini, "mappings:btn_l3",         "btn_thumbl");
    char const* btn_l4         = iniparser_getstring(ini, "mappings:btn_l4",         "btn_gripl");
    char const* btn_l5         = iniparser_getstring(ini, "mappings:btn_l5",         "btn_gripl2");
    char const* btn_r1         = iniparser_getstring(ini, "mappings:btn_r1",         "btn_tr");
    char const* btn_r2         = iniparser_getstring(ini, "mappings:btn_r2",         "btn_tr2");
    char const* btn_r3         = iniparser_getstring(ini, "mappings:btn_r3",         "btn_thumbr");
    char const* btn_r4         = iniparser_getstring(ini, "mappings:btn_r4",         "btn_gripr");
    char const* btn_r5         = iniparser_getstring(ini, "mappings:btn_r5",         "btn_gripr2");
    char const* btn_steam      = iniparser_getstring(ini, "mappings:btn_steam",      "btn_mode");
    char const* btn_menu       = iniparser_getstring(ini, "mappings:btn_menu",       "btn_start");
    char const* btn_view       = iniparser_getstring(ini, "mappings:btn_view",       "btn_select");
    char const* btn_quick      = iniparser_getstring(ini, "mappings:btn_quick",      "btn_base");
    char const* btn_touchpadl  = iniparser_getstring(ini, "mappings:btn_touchpadl",  "mouse_btn_right");
    char const* btn_touchpadr  = iniparser_getstring(ini, "mappings:btn_touchpadr",  "mouse_btn_left");
    if (Mapping_button_init(&config.mappings.btn_a,          btn_a)          ||
        Mapping_button_init(&config.mappings.btn_b,          btn_b)          ||
        Mapping_button_init(&config.mappings.btn_x,          btn_x)          ||
        Mapping_button_init(&config.mappings.btn_y,          btn_y)          ||
        Mapping_button_init(&config.mappings.btn_dpad_up,    btn_dpad_up)    ||
        Mapping_button_init(&config.mappings.btn_dpad_down,  btn_dpad_down)  ||
        Mapping_button_init(&config.mappings.btn_dpad_left,  btn_dpad_left)  ||
        Mapping_button_init(&config.mappings.btn_dpad_right, btn_dpad_right) ||
        Mapping_button_init(&config.mappings.btn_l1,         btn_l1)         ||
        Mapping_button_init(&config.mappings.btn_l2,         btn_l2)         ||
        Mapping_button_init(&config.mappings.btn_l3,         btn_l3)         ||
        Mapping_button_init(&config.mappings.btn_l4,         btn_l4)         ||
        Mapping_button_init(&config.mappings.btn_l5,         btn_l5)         ||
        Mapping_button_init(&config.mappings.btn_r1,         btn_r1)         ||
        Mapping_button_init(&config.mappings.btn_r2,         btn_r2)         ||
        Mapping_button_init(&config.mappings.btn_r3,         btn_r3)         ||
        Mapping_button_init(&config.mappings.btn_r4,         btn_r4)         ||
        Mapping_button_init(&config.mappings.btn_r5,         btn_r5)         ||
        Mapping_button_init(&config.mappings.btn_steam,      btn_steam)      ||
        Mapping_button_init(&config.mappings.btn_menu,       btn_menu)       ||
        Mapping_button_init(&config.mappings.btn_view,       btn_view)       ||
        Mapping_button_init(&config.mappings.btn_quick,      btn_quick)      ||
        Mapping_button_init(&config.mappings.btn_touchpadl,  btn_touchpadl)  ||
        Mapping_button_init(&config.mappings.btn_touchpadr,  btn_touchpadr)) {
        return RET_ERROR;
    }
    char const* axis_l2            = iniparser_getstring(ini, "mappings:axis_l2",            "abs_z");
    char const* axis_r2            = iniparser_getstring(ini, "mappings:axis_r2",            "abs_rz");
    char const* axis_thumbstickl_x = iniparser_getstring(ini, "mappings:axis_thumbstickl_x", "abs_x");
    char const* axis_thumbstickl_y = iniparser_getstring(ini, "mappings:axis_thumbstickl_y", "abs_y");
    char const* axis_thumbstickr_x = iniparser_getstring(ini, "mappings:axis_thumbstickr_x", "abs_rx");
    char const* axis_thumbstickr_y = iniparser_getstring(ini, "mappings:axis_thumbstickr_y", "abs_ry");
    if (Mapping_axis_init(&config.mappings.axis_l2,            axis_l2)            ||
        Mapping_axis_init(&config.mappings.axis_r2,            axis_r2)            ||
        Mapping_axis_init(&config.mappings.axis_thumbstickl_x, axis_thumbstickl_x) ||
        Mapping_axis_init(&config.mappings.axis_thumbstickl_y, axis_thumbstickl_y) ||
        Mapping_axis_init(&config.mappings.axis_thumbstickr_x, axis_thumbstickr_x) ||
        Mapping_axis_init(&config.mappings.axis_thumbstickr_y, axis_thumbstickr_y)) {
        return RET_ERROR;
    }

    // Virtual Devices
    struct VirtualDevice* const gamepad = &config.virtual.gamepad;
    gamepad->vid     = iniparser_getuint64(ini, "virtual.gamepad:vid",     VIRTUAL_GAMEPAD_VID);
    gamepad->pid     = iniparser_getuint64(ini, "virtual.gamepad:pid",     VIRTUAL_GAMEPAD_PID);
    gamepad->version = iniparser_getuint64(ini, "virtual.gamepad:version", VIRTUAL_GAMEPAD_VERSION);

    char const* gamepad_name = iniparser_getstring(ini, "virtual.gamepad:name", VIRTUAL_GAMEPAD_NAME);
    if (safe_strcpy(gamepad->name, gamepad_name, VIRTUAL_DEVICE_NAME_MAXLEN)) {
        return RET_ERROR;
    }
    if (gamepad->vid > 0xffff) {
        fprintf(stderr, "virtual.gamepad.vid must be <= 0xffff\n");
        return RET_ERROR;
    }
    if (gamepad->pid > 0xffff) {
        fprintf(stderr, "virtual.gamepad.pid must be <= 0xffff\n");
        return RET_ERROR;
    }

    struct VirtualDevice* const mouse = &config.virtual.mouse;
    mouse->vid     = iniparser_getuint64(ini, "virtual.mouse:vid",     VIRTUAL_MOUSE_VID);
    mouse->pid     = iniparser_getuint64(ini, "virtual.mouse:pid",     VIRTUAL_MOUSE_PID);
    mouse->version = iniparser_getuint64(ini, "virtual.mouse:version", VIRTUAL_MOUSE_VERSION);

    char const* mouse_name = iniparser_getstring(ini, "virtual.mouse:name", VIRTUAL_MOUSE_NAME);
    if (safe_strcpy(mouse->name, mouse_name, VIRTUAL_DEVICE_NAME_MAXLEN)) {
        return RET_ERROR;
    }
    if (mouse->vid > 0xffff) {
        fprintf(stderr, "virtual.mouse.vid must be <= 0xffff\n");
        return RET_ERROR;
    }
    if (mouse->pid > 0xffff) {
        fprintf(stderr, "virtual.mouse.pid must be <= 0xffff\n");
        return RET_ERROR;
    }

    struct VirtualAbs* const abs_x = &config.virtual.abs_x;
    abs_x->minimum =     iniparser_getint(ini, "virtual.gamepad.abs_x:minimum", DEFAULT_2POLE_ABS_MINIMUM);
    abs_x->maximum =     iniparser_getint(ini, "virtual.gamepad.abs_x:maximum", DEFAULT_2POLE_ABS_MAXIMUM);
    abs_x->flipped = iniparser_getboolean(ini, "virtual.gamepad.abs_x:flipped", DEFAULT_2POLE_ABS_FLIPPED);

    struct VirtualAbs* const abs_y = &config.virtual.abs_y;
    abs_y->minimum =     iniparser_getint(ini, "virtual.gamepad.abs_y:minimum", DEFAULT_2POLE_ABS_MINIMUM);
    abs_y->maximum =     iniparser_getint(ini, "virtual.gamepad.abs_y:maximum", DEFAULT_2POLE_ABS_MAXIMUM);
    abs_y->flipped = iniparser_getboolean(ini, "virtual.gamepad.abs_y:flipped", DEFAULT_2POLE_ABS_FLIPPED);

    struct VirtualAbs* const abs_z = &config.virtual.abs_z;
    abs_z->minimum =     iniparser_getint(ini, "virtual.gamepad.abs_z:minimum", DEFAULT_1POLE_ABS_MINIMUM);
    abs_z->maximum =     iniparser_getint(ini, "virtual.gamepad.abs_z:maximum", DEFAULT_1POLE_ABS_MAXIMUM);
    abs_z->flipped = iniparser_getboolean(ini, "virtual.gamepad.abs_z:flipped", DEFAULT_1POLE_ABS_FLIPPED);

    struct VirtualAbs* const abs_rx = &config.virtual.abs_rx;
    abs_rx->minimum =     iniparser_getint(ini, "virtual.gamepad.abs_rx:minimum", DEFAULT_2POLE_ABS_MINIMUM);
    abs_rx->maximum =     iniparser_getint(ini, "virtual.gamepad.abs_rx:maximum", DEFAULT_2POLE_ABS_MAXIMUM);
    abs_rx->flipped = iniparser_getboolean(ini, "virtual.gamepad.abs_rx:flipped", DEFAULT_2POLE_ABS_FLIPPED);

    struct VirtualAbs* const abs_ry = &config.virtual.abs_ry;
    abs_ry->minimum =     iniparser_getint(ini, "virtual.gamepad.abs_ry:minimum", DEFAULT_2POLE_ABS_MINIMUM);
    abs_ry->maximum =     iniparser_getint(ini, "virtual.gamepad.abs_ry:maximum", DEFAULT_2POLE_ABS_MAXIMUM);
    abs_ry->flipped = iniparser_getboolean(ini, "virtual.gamepad.abs_ry:flipped", DEFAULT_2POLE_ABS_FLIPPED);

    struct VirtualAbs* const abs_rz = &config.virtual.abs_rz;
    abs_rz->minimum =     iniparser_getint(ini, "virtual.gamepad.abs_rz:minimum", DEFAULT_1POLE_ABS_MINIMUM);
    abs_rz->maximum =     iniparser_getint(ini, "virtual.gamepad.abs_rz:maximum", DEFAULT_1POLE_ABS_MAXIMUM);
    abs_rz->flipped = iniparser_getboolean(ini, "virtual.gamepad.abs_rz:flipped", DEFAULT_1POLE_ABS_FLIPPED);

    return RET_OKAY;
}

int Mapping_button_init(struct Mapping* mapping, char const* val) {
    if (!Mapping_match_disabled(mapping, val)) {
        return RET_OKAY;
    }
    if (!Mapping_match_button(mapping, "btn_north",      val, GAMEPAD_BTN_NORTH)      ||
        !Mapping_match_button(mapping, "btn_south",      val, GAMEPAD_BTN_SOUTH)      ||
        !Mapping_match_button(mapping, "btn_east",       val, GAMEPAD_BTN_EAST)       ||
        !Mapping_match_button(mapping, "btn_west",       val, GAMEPAD_BTN_WEST)       ||
        !Mapping_match_button(mapping, "btn_dpad_up",    val, GAMEPAD_BTN_DPAD_UP)    ||
        !Mapping_match_button(mapping, "btn_dpad_down",  val, GAMEPAD_BTN_DPAD_DOWN)  ||
        !Mapping_match_button(mapping, "btn_dpad_left",  val, GAMEPAD_BTN_DPAD_LEFT)  ||
        !Mapping_match_button(mapping, "btn_dpad_right", val, GAMEPAD_BTN_DPAD_RIGHT) ||
        !Mapping_match_button(mapping, "btn_gripl",      val, GAMEPAD_BTN_GRIPL)      ||
        !Mapping_match_button(mapping, "btn_gripl2",     val, GAMEPAD_BTN_GRIPL2)     ||
        !Mapping_match_button(mapping, "btn_gripr",      val, GAMEPAD_BTN_GRIPR)      ||
        !Mapping_match_button(mapping, "btn_gripr2",     val, GAMEPAD_BTN_GRIPR2)     ||
        !Mapping_match_button(mapping, "btn_thumbl",     val, GAMEPAD_BTN_THUMBL)     ||
        !Mapping_match_button(mapping, "btn_thumbr",     val, GAMEPAD_BTN_THUMBR)     ||
        !Mapping_match_button(mapping, "btn_tl",         val, GAMEPAD_BTN_TL)         ||
        !Mapping_match_button(mapping, "btn_tl2",        val, GAMEPAD_BTN_TL2)        ||
        !Mapping_match_button(mapping, "btn_tr",         val, GAMEPAD_BTN_TR)         ||
        !Mapping_match_button(mapping, "btn_tr2",        val, GAMEPAD_BTN_TR2)        ||
        !Mapping_match_button(mapping, "btn_mode",       val, GAMEPAD_BTN_MODE)       ||
        !Mapping_match_button(mapping, "btn_start",      val, GAMEPAD_BTN_START)      ||
        !Mapping_match_button(mapping, "btn_select",     val, GAMEPAD_BTN_SELECT)     ||
        !Mapping_match_button(mapping, "btn_base",       val, GAMEPAD_BTN_BASE)       ||
        !Mapping_match_button(mapping, "btn_thumb",      val, GAMEPAD_BTN_THUMB)      ||
        !Mapping_match_button(mapping, "btn_thumb2",     val, GAMEPAD_BTN_THUMB2)) {
        return RET_OKAY;
    }
    if (!Mapping_match_mouse_button(mapping, "mouse_btn_left",   val, MOUSE_BTN_LEFT)  ||
        !Mapping_match_mouse_button(mapping, "mouse_btn_right",  val, MOUSE_BTN_RIGHT) ||
        !Mapping_match_mouse_button(mapping, "mouse_btn_middle", val, MOUSE_BTN_MIDDLE)) {
        return RET_OKAY;
    }
    fprintf(stderr, "could not parse button mapping \"%s\"\n", val);
    return RET_ERROR;
}

int Mapping_axis_init(struct Mapping* mapping, char const* val) {
    if (!Mapping_match_disabled(mapping, val)) {
        return RET_OKAY;
    }
    if (!Mapping_match_axis(mapping, "abs_z",  val, GAMEPAD_ABS_Z)  ||
        !Mapping_match_axis(mapping, "abs_rz", val, GAMEPAD_ABS_RZ) ||
        !Mapping_match_axis(mapping, "abs_x",  val, GAMEPAD_ABS_X)  ||
        !Mapping_match_axis(mapping, "abs_y",  val, GAMEPAD_ABS_Y)  ||
        !Mapping_match_axis(mapping, "abs_rx", val, GAMEPAD_ABS_RX) ||
        !Mapping_match_axis(mapping, "abs_ry", val, GAMEPAD_ABS_RY)) {
        return RET_OKAY;
    }
    fprintf(stderr, "could not parse axis mapping \"%s\"\n", val);
    return RET_ERROR;
}


int Mapping_send(
    struct Mapping const* const mapping,
    int ui_gamepad,
    int ui_mouse,
    int32_t value
) {
    if (mapping->type == MAPPING_TYPE_GAMEPAD_ABS) {
        switch (mapping->code) {
            case GAMEPAD_ABS_X:  return ui_abs(&config.virtual.abs_x,  ui_gamepad, ABS_X,  value);
            case GAMEPAD_ABS_Y:  return ui_abs(&config.virtual.abs_y,  ui_gamepad, ABS_Y,  value);
            case GAMEPAD_ABS_Z:  return ui_abs(&config.virtual.abs_z,  ui_gamepad, ABS_Z,  value);
            case GAMEPAD_ABS_RX: return ui_abs(&config.virtual.abs_rx, ui_gamepad, ABS_RX, value);
            case GAMEPAD_ABS_RY: return ui_abs(&config.virtual.abs_ry, ui_gamepad, ABS_RY, value);
            case GAMEPAD_ABS_RZ: return ui_abs(&config.virtual.abs_rz, ui_gamepad, ABS_RZ, value);
        }
    }
    if (mapping->type == MAPPING_TYPE_GAMEPAD_BTN) {
        switch (mapping->code) {
            case GAMEPAD_BTN_NORTH:      return ui_btn(ui_gamepad, BTN_NORTH,      value);
            case GAMEPAD_BTN_SOUTH:      return ui_btn(ui_gamepad, BTN_SOUTH,      value);
            case GAMEPAD_BTN_EAST:       return ui_btn(ui_gamepad, BTN_EAST,       value);
            case GAMEPAD_BTN_WEST:       return ui_btn(ui_gamepad, BTN_WEST,       value);
            case GAMEPAD_BTN_DPAD_UP:    return ui_btn(ui_gamepad, BTN_DPAD_UP,    value);
            case GAMEPAD_BTN_DPAD_DOWN:  return ui_btn(ui_gamepad, BTN_DPAD_DOWN,  value);
            case GAMEPAD_BTN_DPAD_LEFT:  return ui_btn(ui_gamepad, BTN_DPAD_LEFT,  value);
            case GAMEPAD_BTN_DPAD_RIGHT: return ui_btn(ui_gamepad, BTN_DPAD_RIGHT, value);
            case GAMEPAD_BTN_GRIPL2:     return ui_btn(ui_gamepad, BTN_GRIPL2,     value);
            case GAMEPAD_BTN_GRIPL:      return ui_btn(ui_gamepad, BTN_GRIPL,      value);
            case GAMEPAD_BTN_GRIPR2:     return ui_btn(ui_gamepad, BTN_GRIPR2,     value);
            case GAMEPAD_BTN_GRIPR:      return ui_btn(ui_gamepad, BTN_GRIPR,      value);
            case GAMEPAD_BTN_THUMB2:     return ui_btn(ui_gamepad, BTN_THUMB2,     value);
            case GAMEPAD_BTN_THUMB:      return ui_btn(ui_gamepad, BTN_THUMB,      value);
            case GAMEPAD_BTN_THUMBL:     return ui_btn(ui_gamepad, BTN_THUMBL,     value);
            case GAMEPAD_BTN_THUMBR:     return ui_btn(ui_gamepad, BTN_THUMBR,     value);
            case GAMEPAD_BTN_TL2:        return ui_btn(ui_gamepad, BTN_TL2,        value);
            case GAMEPAD_BTN_TL:         return ui_btn(ui_gamepad, BTN_TL,         value);
            case GAMEPAD_BTN_TR2:        return ui_btn(ui_gamepad, BTN_TR2,        value);
            case GAMEPAD_BTN_TR:         return ui_btn(ui_gamepad, BTN_TR,         value);
            case GAMEPAD_BTN_MODE:       return ui_btn(ui_gamepad, BTN_MODE,       value);
            case GAMEPAD_BTN_START:      return ui_btn(ui_gamepad, BTN_START,      value);
            case GAMEPAD_BTN_SELECT:     return ui_btn(ui_gamepad, BTN_SELECT,     value);
            case GAMEPAD_BTN_BASE:       return ui_btn(ui_gamepad, BTN_BASE,       value);
        }
    }
    if (mapping->type == MAPPING_TYPE_MOUSE_BTN) {
        switch (mapping->code) {
            case MOUSE_BTN_LEFT:   return ui_btn(ui_mouse, BTN_LEFT,   value);
            case MOUSE_BTN_RIGHT:  return ui_btn(ui_mouse, BTN_RIGHT,  value);
            case MOUSE_BTN_MIDDLE: return ui_btn(ui_mouse, BTN_MIDDLE, value);
        }
    }
    return RET_OKAY;
}

bool Mapping_is_btn(struct Mapping const* const mapping, int32_t code) {
    return (
        mapping->type == MAPPING_TYPE_GAMEPAD_BTN &&
        mapping->code == code
    );
}

bool Mapping_is_abs(struct Mapping const* const mapping, int32_t code) {
    return (
        mapping->type == MAPPING_TYPE_GAMEPAD_ABS &&
        mapping->code == code
    );
}

bool Mappings_is_btn_used(int32_t code) {
    return (
        Mapping_is_btn(&config.mappings.btn_a,          code) ||
        Mapping_is_btn(&config.mappings.btn_b,          code) ||
        Mapping_is_btn(&config.mappings.btn_x,          code) ||
        Mapping_is_btn(&config.mappings.btn_y,          code) ||
        Mapping_is_btn(&config.mappings.btn_dpad_up,    code) ||
        Mapping_is_btn(&config.mappings.btn_dpad_down,  code) ||
        Mapping_is_btn(&config.mappings.btn_dpad_left,  code) ||
        Mapping_is_btn(&config.mappings.btn_dpad_right, code) ||
        Mapping_is_btn(&config.mappings.btn_l1,         code) ||
        Mapping_is_btn(&config.mappings.btn_l2,         code) ||
        Mapping_is_btn(&config.mappings.btn_l3,         code) ||
        Mapping_is_btn(&config.mappings.btn_l4,         code) ||
        Mapping_is_btn(&config.mappings.btn_l5,         code) ||
        Mapping_is_btn(&config.mappings.btn_r1,         code) ||
        Mapping_is_btn(&config.mappings.btn_r2,         code) ||
        Mapping_is_btn(&config.mappings.btn_r3,         code) ||
        Mapping_is_btn(&config.mappings.btn_r4,         code) ||
        Mapping_is_btn(&config.mappings.btn_r5,         code) ||
        Mapping_is_btn(&config.mappings.btn_steam,      code) ||
        Mapping_is_btn(&config.mappings.btn_menu,       code) ||
        Mapping_is_btn(&config.mappings.btn_view,       code) ||
        Mapping_is_btn(&config.mappings.btn_quick,      code) ||
        Mapping_is_btn(&config.mappings.btn_touchpadl,  code) ||
        Mapping_is_btn(&config.mappings.btn_touchpadr,  code)
    );
}

bool Mappings_is_abs_used(int32_t code) {
    return (
        Mapping_is_abs(&config.mappings.axis_l2,            code) ||
        Mapping_is_abs(&config.mappings.axis_r2,            code) ||
        Mapping_is_abs(&config.mappings.axis_thumbstickl_x, code) ||
        Mapping_is_abs(&config.mappings.axis_thumbstickl_y, code) ||
        Mapping_is_abs(&config.mappings.axis_thumbstickr_x, code) ||
        Mapping_is_abs(&config.mappings.axis_thumbstickr_y, code)
    );
}

static int Mapping_match_disabled(
    struct Mapping* mapping,
    char const* const val
) {
    if (!strcmp("", val)) {
        mapping->type = MAPPING_TYPE_DISABLED;
        return RET_ERROR;
    }
    if (!strcmp("disabled", val)) {
        mapping->type = MAPPING_TYPE_DISABLED;
        return RET_OKAY;
    }
    return RET_ERROR;
}

static int Mapping_match_button(
    struct Mapping* mapping,
    char const* const key,
    char const* const val,
    int32_t code
) {
    if (!strcmp(key, val)) {
        mapping->type = MAPPING_TYPE_GAMEPAD_BTN;
        mapping->code = code;
        return RET_OKAY;
    }
    return RET_ERROR;
}

static int Mapping_match_axis(
    struct Mapping* mapping,
    char const* const key,
    char const* const val,
    int32_t code
) {
    if (!strcmp(key, val)) {
        mapping->type = MAPPING_TYPE_GAMEPAD_ABS;
        mapping->code = code;
        return RET_OKAY;
    }
    return RET_ERROR;
}

static int Mapping_match_mouse_button(
    struct Mapping* mapping,
    char const* const key,
    char const* const val,
    int32_t code
) {
    if (!strcmp(key, val)) {
        mapping->type = MAPPING_TYPE_MOUSE_BTN;
        mapping->code = code;
        return RET_OKAY;
    }
    return RET_ERROR;
}

static int ui_btn(int fd, int16_t code, bool value) {
    return uinput_emit(fd, EV_KEY, code, value);
}

static int ui_abs(struct VirtualAbs const* const abs, int fd, int16_t code, int16_t value) {
    return uinput_emit(fd, EV_ABS, code, abs->flipped ? -value : value);
}

