#include "config.h"
#include "constants.h"
#include "sc_string.h"
#include "virtual_gamepad.h"

#include <fcntl.h>
#include <linux/uinput.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

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
        return RET_ERROR;
    }
    return RET_OKAY;
}

static inline int maybe_init_btn(int fd, int32_t key, int32_t val) {
    if (Mappings_is_btn_used(key)) {
        if (ioctl(fd, UI_SET_KEYBIT, val)) {
            return RET_ERROR;
        }
    }
    return RET_OKAY;
}

static inline int maybe_init_abs(int fd, int32_t key, int32_t val, struct VirtualAbs const* const abs) {
    if (Mappings_is_abs_used(key)) {
        if (ioctl(fd, UI_SET_ABSBIT, val)) {
            return RET_ERROR;
        }
        if (uinput_setup_axis(fd, val, abs->minimum, abs->maximum)) {
            return RET_ERROR;
        }
    }
    return RET_OKAY;
}

int virtual_gamepad_setup(int* const fd) {
    if ((*fd = open("/dev/uinput", O_RDWR | O_NONBLOCK)) < 0) {
        perror("failed to open uinput device");
        return RET_ERROR;
    }
    if (ioctl(*fd, UI_SET_EVBIT, EV_KEY) ||
        ioctl(*fd, UI_SET_EVBIT, EV_ABS) ||
        ioctl(*fd, UI_SET_EVBIT, EV_FF)) {
        perror("failed to set uinput device capabilities");
        return RET_ERROR;
    }
    if (maybe_init_btn(*fd, GAMEPAD_BTN_NORTH,      BTN_NORTH)      ||
        maybe_init_btn(*fd, GAMEPAD_BTN_SOUTH,      BTN_SOUTH)      ||
        maybe_init_btn(*fd, GAMEPAD_BTN_EAST,       BTN_EAST)       ||
        maybe_init_btn(*fd, GAMEPAD_BTN_WEST,       BTN_WEST)       ||
        maybe_init_btn(*fd, GAMEPAD_BTN_DPAD_UP,    BTN_DPAD_UP)    ||
        maybe_init_btn(*fd, GAMEPAD_BTN_DPAD_DOWN,  BTN_DPAD_DOWN)  ||
        maybe_init_btn(*fd, GAMEPAD_BTN_DPAD_LEFT,  BTN_DPAD_LEFT)  ||
        maybe_init_btn(*fd, GAMEPAD_BTN_DPAD_RIGHT, BTN_DPAD_RIGHT) ||
        maybe_init_btn(*fd, GAMEPAD_BTN_GRIPL,      BTN_GRIPL)      ||
        maybe_init_btn(*fd, GAMEPAD_BTN_GRIPL2,     BTN_GRIPL2)     ||
        maybe_init_btn(*fd, GAMEPAD_BTN_GRIPR,      BTN_GRIPR)      ||
        maybe_init_btn(*fd, GAMEPAD_BTN_GRIPR2,     BTN_GRIPR2)     ||
        maybe_init_btn(*fd, GAMEPAD_BTN_THUMB,      BTN_THUMB)      ||
        maybe_init_btn(*fd, GAMEPAD_BTN_THUMB2,     BTN_THUMB2)     ||
        maybe_init_btn(*fd, GAMEPAD_BTN_THUMBL,     BTN_THUMBL)     ||
        maybe_init_btn(*fd, GAMEPAD_BTN_THUMBR,     BTN_THUMBR)     ||
        maybe_init_btn(*fd, GAMEPAD_BTN_TL,         BTN_TL)         ||
        maybe_init_btn(*fd, GAMEPAD_BTN_TL2,        BTN_TL2)        ||
        maybe_init_btn(*fd, GAMEPAD_BTN_TR,         BTN_TR)         ||
        maybe_init_btn(*fd, GAMEPAD_BTN_TR2,        BTN_TR2)        ||
        maybe_init_btn(*fd, GAMEPAD_BTN_BASE,       BTN_BASE)       ||
        maybe_init_btn(*fd, GAMEPAD_BTN_START,      BTN_START)      ||
        maybe_init_btn(*fd, GAMEPAD_BTN_SELECT,     BTN_SELECT)     ||
        maybe_init_btn(*fd, GAMEPAD_BTN_MODE,       BTN_MODE)) {
        perror("failed to setup uinput device buttons");
        return RET_ERROR;
    }

    if (maybe_init_abs(*fd, GAMEPAD_ABS_X,  ABS_X,  &config.virtual.abs_x) ||
        maybe_init_abs(*fd, GAMEPAD_ABS_Y,  ABS_Y,  &config.virtual.abs_y) ||
        maybe_init_abs(*fd, GAMEPAD_ABS_Z,  ABS_Z,  &config.virtual.abs_z) ||
        maybe_init_abs(*fd, GAMEPAD_ABS_RX, ABS_RX, &config.virtual.abs_rx) ||
        maybe_init_abs(*fd, GAMEPAD_ABS_RY, ABS_RY, &config.virtual.abs_ry) ||
        maybe_init_abs(*fd, GAMEPAD_ABS_RZ, ABS_RZ, &config.virtual.abs_rz)) {
        perror("failed to setup uinput device axis");
        return RET_ERROR;
    }

    if (ioctl(*fd, UI_SET_FFBIT, FF_RUMBLE)) {
        perror("failed to configure uinput haptics");
        return RET_ERROR;
    }

    struct uinput_setup usetup = {
        .id = {
            .bustype = BUS_VIRTUAL,
            .vendor  = config.virtual.gamepad.vid,
            .product = config.virtual.gamepad.pid,
            .version = config.virtual.gamepad.version,
        },
        .ff_effects_max = 1,
    };
    if (safe_strcpy((char*)&usetup.name, config.virtual.gamepad.name, UINPUT_MAX_NAME_SIZE)) {
        return RET_ERROR;
    }

    if (ioctl(*fd, UI_DEV_SETUP, &usetup)) {
        perror("failed to setup uinput device");
        return RET_ERROR;
    }
    if (ioctl(*fd, UI_DEV_CREATE)) {
        perror("failed to create uinput device");
        return RET_ERROR;
    }
    return RET_OKAY;
}

int virtual_gamepad_destroy(int fd) {
    int result = RET_OKAY;
    if (ioctl(fd, UI_DEV_DESTROY)) {
        perror("failed to destroy uinput device");
        result = RET_ERROR;
    }
    if (close(fd)) {
        perror("failed to close uinput device");
        result = RET_ERROR;
    }
    return result;
}

int virtual_gamepad_ff_upload(int fd, struct uinput_ff_upload* const event) {
    if (ioctl(fd, UI_BEGIN_FF_UPLOAD, event)) {
        perror("failed to upload uinput force-feedback event");
        return RET_ERROR;
    }

    event->retval = 0;

    if (ioctl(fd, UI_END_FF_UPLOAD, event)) {
        perror("failed to upload uinput force-feedback event");
        return RET_ERROR;
    }
    return RET_OKAY;
}

int virtual_gamepad_ff_erase(int fd, struct uinput_ff_erase* const event) {
    if (ioctl(fd, UI_BEGIN_FF_ERASE, event)) {
        perror("failed to erase uinput force-feedback event");
        return RET_ERROR;
    }

    event->retval = 0;

    if (ioctl(fd, UI_END_FF_ERASE, event)) {
        perror("failed to erase uinput force-feedback event");
        return RET_ERROR;
    }
    return RET_OKAY;
}
