#include "codes.h"
#include "virtual_gamepad.h"

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/ioctl.h>

#define HAT_MIN        -1
#define HAT_MAX         1
#define TRIGGER_MIN     0
#define TRIGGER_MAX     32767
#define THUMBSTICK_MIN -32767
#define THUMBSTICK_MAX  32767
#define TOUCHPAD_MIN   -32767
#define TOUCHPAD_MAX    32767

static int uinput_setup_axis(
    int fd,
    uint16_t code,
    uint32_t minimum,
    uint32_t maximum
) {
    struct uinput_abs_setup abs_setup = {
        .code               = code,
        .absinfo.minimum    = minimum,
        .absinfo.maximum    = maximum,
        .absinfo.flat       = 0,
        .absinfo.fuzz       = 0,
        .absinfo.resolution = 0,
    };

    if (ioctl(fd, UI_ABS_SETUP, &abs_setup)) {
        return 1;
    }

    return 0;
}

int virtual_gamepad_setup(int* const uinput_fd) {
    if ((*uinput_fd = open("/dev/uinput", O_RDWR)) < 0) {
        perror("failed to open uinput device");
        return 1;
    }

    if (ioctl(*uinput_fd, UI_SET_EVBIT, EV_KEY) ||
        ioctl(*uinput_fd, UI_SET_EVBIT, EV_ABS)) {
        perror("failed to set uinput device capabilities");
        return 1;
    }

    if (ioctl(*uinput_fd, UI_SET_KEYBIT, BTN_SOUTH)  || // a
        ioctl(*uinput_fd, UI_SET_KEYBIT, BTN_EAST)   || // b
        ioctl(*uinput_fd, UI_SET_KEYBIT, BTN_WEST)   || // x
        ioctl(*uinput_fd, UI_SET_KEYBIT, BTN_NORTH)  || // y
        ioctl(*uinput_fd, UI_SET_ABSBIT, ABS_HAT0X)  || // dpad x-axis
        ioctl(*uinput_fd, UI_SET_ABSBIT, ABS_HAT0Y)  || // dpad y-axis
        ioctl(*uinput_fd, UI_SET_KEYBIT, BTN_TL)     || // l1
        ioctl(*uinput_fd, UI_SET_KEYBIT, BTN_TR)     || // r1
        ioctl(*uinput_fd, UI_SET_KEYBIT, BTN_TL2)    || // l2
        ioctl(*uinput_fd, UI_SET_KEYBIT, BTN_TR2)    || // r2
        ioctl(*uinput_fd, UI_SET_KEYBIT, BTN_THUMBL) || // l3
        ioctl(*uinput_fd, UI_SET_KEYBIT, BTN_THUMBR) || // r3
        ioctl(*uinput_fd, UI_SET_KEYBIT, BTN_GRIPL)  || // l4
        ioctl(*uinput_fd, UI_SET_KEYBIT, BTN_GRIPR)  || // r4
        ioctl(*uinput_fd, UI_SET_KEYBIT, BTN_GRIPL2) || // l5
        ioctl(*uinput_fd, UI_SET_KEYBIT, BTN_GRIPR2) || // r5
        ioctl(*uinput_fd, UI_SET_KEYBIT, BTN_THUMB)  || // left touchpad (TODO: unused)
        ioctl(*uinput_fd, UI_SET_KEYBIT, BTN_THUMB2) || // right touchpad (TODO: unused)
        ioctl(*uinput_fd, UI_SET_KEYBIT, BTN_BASE)   || // quick
        ioctl(*uinput_fd, UI_SET_KEYBIT, BTN_START)  || // menu
        ioctl(*uinput_fd, UI_SET_KEYBIT, BTN_SELECT) || // view
        ioctl(*uinput_fd, UI_SET_KEYBIT, BTN_MODE)   || // steam
        ioctl(*uinput_fd, UI_SET_ABSBIT, ABS_X)      || // left thumbstick x-axis
        ioctl(*uinput_fd, UI_SET_ABSBIT, ABS_Y)      || // left thumbstick y-axis
        ioctl(*uinput_fd, UI_SET_ABSBIT, ABS_RX)     || // right thumbstick x-axis
        ioctl(*uinput_fd, UI_SET_ABSBIT, ABS_RY)     || // right thumbstick y-axis
        ioctl(*uinput_fd, UI_SET_ABSBIT, ABS_Z)      || // l2
        ioctl(*uinput_fd, UI_SET_ABSBIT, ABS_RZ)) {     // r2
        perror("failed to set uinput device buttons");
        return 1;
    }

    if (uinput_setup_axis(*uinput_fd, ABS_HAT0X, HAT_MIN,        HAT_MAX)        || // dpad x-axis
        uinput_setup_axis(*uinput_fd, ABS_HAT0Y, HAT_MIN,        HAT_MAX)        || // dpad y-axis
        uinput_setup_axis(*uinput_fd, ABS_Z,     TRIGGER_MIN,    TRIGGER_MAX)    || // l2
        uinput_setup_axis(*uinput_fd, ABS_RZ,    TRIGGER_MIN,    TRIGGER_MAX)    || // r2
        uinput_setup_axis(*uinput_fd, ABS_X,     THUMBSTICK_MIN, THUMBSTICK_MAX) || // left thumbstick x-axis
        uinput_setup_axis(*uinput_fd, ABS_Y,     THUMBSTICK_MIN, THUMBSTICK_MAX) || // left thumbstick y-axis
        uinput_setup_axis(*uinput_fd, ABS_RX,    THUMBSTICK_MIN, THUMBSTICK_MAX) || // right thumbstick x-axis
        uinput_setup_axis(*uinput_fd, ABS_RY,    THUMBSTICK_MIN, THUMBSTICK_MAX) || // right thumbstick y-axis
        uinput_setup_axis(*uinput_fd, ABS_HAT1X, TOUCHPAD_MIN,   TOUCHPAD_MAX)   || // left touchpad x-axis (TODO: unused)
        uinput_setup_axis(*uinput_fd, ABS_HAT1Y, TOUCHPAD_MIN,   TOUCHPAD_MAX)   || // left touchpad y-axis (TODO: unused)
        uinput_setup_axis(*uinput_fd, ABS_HAT2X, TOUCHPAD_MIN,   TOUCHPAD_MAX)   || // right touchpad x-axis (TODO: unused)
        uinput_setup_axis(*uinput_fd, ABS_HAT2Y, TOUCHPAD_MIN,   TOUCHPAD_MAX)) {   // right touchpad y-axis (TODO: unused)
        perror("failed to configure uinput device axis");
        return 1;
    }

    struct uinput_setup usetup = {
        .name = "Virtual Controller",
        .id = {
            .bustype = BUS_VIRTUAL,
            .vendor  = 0x0000,
            .product = 0x0000,
            .version = 1,
        }
    };

    if (ioctl(*uinput_fd, UI_DEV_SETUP, &usetup)) {
        perror("failed to setup uinput device");
        return 1;
    }

    if (ioctl(*uinput_fd, UI_DEV_CREATE)) {
        perror("failed to create uinput device");
        return 1;
    }

    return 0;
}

int virtual_gamepad_destroy(int uinput_fd) {
    int result = 0;

    if (ioctl(uinput_fd, UI_DEV_DESTROY)) {
        perror("failed to destroy uinput device");
        result = 1;
    }

    if (close(uinput_fd)) {
        perror("failed to close uinput device");
        result = 1;
    }

    return result;
}
