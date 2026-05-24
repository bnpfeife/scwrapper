#include "virtual_mouse.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/uinput.h>

int virtual_mouse_setup(int* const uinput_fd) {
    if ((*uinput_fd = open("/dev/uinput", O_WRONLY)) < 0) {
        perror("failed to open uinput device");
        return 1;
    }

    if (ioctl(*uinput_fd, UI_SET_EVBIT, EV_KEY) ||
        ioctl(*uinput_fd, UI_SET_EVBIT, EV_REL)) {
        perror("failed to set uinput device capabilities");
	    return 1;
    }

    if (ioctl(*uinput_fd, UI_SET_KEYBIT, BTN_LEFT) ||
        ioctl(*uinput_fd, UI_SET_KEYBIT, BTN_RIGHT) ||
        ioctl(*uinput_fd, UI_SET_RELBIT, REL_X) ||
        ioctl(*uinput_fd, UI_SET_RELBIT, REL_Y)) {
        perror("failed to set uinput device buttons");
	    return 1;
    }

    struct uinput_setup usetup = {
        .name = "Virtual Mouse",
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

int virtual_mouse_destroy(int uinput_fd) {
    int result = 1;

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
