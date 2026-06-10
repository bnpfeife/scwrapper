#include "config.h"
#include "constants.h"
#include "virtual_mouse.h"

#include <fcntl.h>
#include <linux/uinput.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

int virtual_mouse_setup(int* const uinput_fd) {
    if ((*uinput_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK)) < 0) {
        perror("failed to open uinput device");
        return RET_ERROR;
    }
    if (ioctl(*uinput_fd, UI_SET_EVBIT, EV_KEY) ||
        ioctl(*uinput_fd, UI_SET_EVBIT, EV_REL)) {
        perror("failed to set uinput device capabilities");
            return RET_ERROR;
    }
    if (ioctl(*uinput_fd, UI_SET_KEYBIT, BTN_LEFT)   ||
        ioctl(*uinput_fd, UI_SET_KEYBIT, BTN_RIGHT)  ||
        ioctl(*uinput_fd, UI_SET_KEYBIT, BTN_MIDDLE) ||
        ioctl(*uinput_fd, UI_SET_RELBIT, REL_X)      ||
        ioctl(*uinput_fd, UI_SET_RELBIT, REL_Y)) {
        perror("failed to set uinput device buttons");
            return RET_ERROR;
    }

    struct uinput_setup usetup = {
        .name = VIRTUAL_MOUSE_NAME,
        .id = {
            .bustype = BUS_VIRTUAL,
            .vendor  = config.virtual.mouse.vid,
            .product = config.virtual.mouse.pid,
            .version = config.virtual.mouse.version,
        }
    };

    if (ioctl(*uinput_fd, UI_DEV_SETUP, &usetup)) {
        perror("failed to setup uinput device");
        return RET_ERROR;
    }
    if (ioctl(*uinput_fd, UI_DEV_CREATE)) {
        perror("failed to create uinput device");
        return RET_ERROR;
    }
    return RET_OKAY;
}

int virtual_mouse_destroy(int uinput_fd) {
    int result = RET_OKAY;
    if (ioctl(uinput_fd, UI_DEV_DESTROY)) {
        perror("failed to destroy uinput device");
        result = RET_ERROR;
    }
    if (close(uinput_fd)) {
        perror("failed to close uinput device");
        result = RET_ERROR;
    }
    return result;
}
