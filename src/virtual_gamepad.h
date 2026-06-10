#ifndef SCWRAPPER_VIRTUAL_GAMEPAD_H
#define SCWRAPPER_VIRTUAL_GAMEPAD_H

#include <linux/uinput.h>

int virtual_gamepad_setup(int* const fd);
int virtual_gamepad_destroy(int fd);
int virtual_gamepad_ff_upload(int fd, struct uinput_ff_upload* const event);
int virtual_gamepad_ff_erase(int fd, struct uinput_ff_erase* const event);

#endif // SCWRAPPER_VIRTUAL_GAMEPAD_H
