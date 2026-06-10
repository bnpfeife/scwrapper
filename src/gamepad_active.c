#include "config.h"
#include "constants.h"
#include "gamepad.h"
#include "sc_epoll.h"
#include "triton.h"
#include "virtual_gamepad.h"
#include "virtual_mouse.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int GamepadActive_init(struct GamepadActive* const gamepad) {
    gamepad->ui_gamepad = -1;
    gamepad->ui_mouse   = -1;

    int result = RET_OKAY;
    if (Stopwatch_init(&gamepad->sw_timeout, GAMEPAD_TIMEOUT)) {
        result = RET_ERROR;
    }
    if (Stopwatch_init(&gamepad->sw_configure, GAMEPAD_CONFIGURE_INTERVAL)) {
        result = RET_ERROR;
    }
    if (Stopwatch_init(&gamepad->sw_haptics, GAMEPAD_HAPTICS_INTERVAL)) {
        result = RET_ERROR;
    }
    if (virtual_mouse_setup(&gamepad->ui_mouse)) {
        result = RET_ERROR;
    }
    if (virtual_gamepad_setup(&gamepad->ui_gamepad)) {
        result = RET_ERROR;
    }
    if (sc_epoll_ctl_add(gamepad->ui_gamepad, EPOLLIN | EPOLLRDHUP)) {
        result = RET_ERROR;
    }

    // This is required for `GamepadActive_update` to configure the controller
    // immediately. Otherwise, `GAMEPAD_CONFIGURE_INTERVAL` must elapse before
    // the controller no longer behaves like a mouse and keyboard.
    gamepad->sw_configure.triggered = true;

    if (result) {
        (void)GamepadActive_free(gamepad);
        return RET_ERROR;
    }
    return RET_OKAY;
}

int GamepadActive_free(struct GamepadActive* const gamepad) {
    int result = RET_OKAY;
    if (gamepad->ui_mouse != -1) {
        if (virtual_mouse_destroy(gamepad->ui_mouse)) {
            result = RET_ERROR;
        }
    }
    if (gamepad->ui_gamepad != -1) {
        if (sc_epoll_ctl_remove(gamepad->ui_gamepad)) {
            result = RET_ERROR;
        }
        if (virtual_gamepad_destroy(gamepad->ui_gamepad)) {
            result = RET_ERROR;
        }
    }
    return result;
}

int GamepadActive_update(struct GamepadActive* const gamepad, int hidraw) {
    if (Stopwatch_update(&gamepad->sw_timeout)) {
        return RET_ERROR;
    }
    if (gamepad->sw_timeout.triggered) {
        return RET_TIMEOUT;
    }

    if (Stopwatch_update(&gamepad->sw_configure)) {
        return RET_ERROR;
    }
    if (gamepad->sw_configure.triggered) {
        if (Stopwatch_reset(&gamepad->sw_configure)) {
            return RET_ERROR;
        }
        if (triton_clear_digital_mappings(hidraw)) {
            return RET_ERROR;
        }
        if (triton_disable_lizard_mode(hidraw)) {
            return RET_ERROR;
        }
    }
    return RET_OKAY;
}

int GamepadActive_update_haptics(struct GamepadActive* const gamepad, int hidraw) {
    if (gamepad->haptics.id == SC_HAPTICS_ID_RUMBLE) {
        if (Stopwatch_update(&gamepad->sw_haptics)) {
            return RET_ERROR;
        }
        if (gamepad->sw_haptics.triggered) {
            if (Stopwatch_reset(&gamepad->sw_haptics)) {
                return RET_ERROR;
            }
            if (triton_haptics_rumble(hidraw, gamepad->haptics.u.rumble)) {
                return RET_ERROR;
            }
        }
    }
    return RET_OKAY;
}

#define MAX_HIDRAW_EVENTS 16
#define MAX_UINPUT_EVENTS 16

int GamepadActive_hidraw_event(struct GamepadActive* const gamepad, int hidraw) {
    uint8_t buffer[64] = { 0 };

    // Continue reading until `MAX_HIDRAW_EVENTS` is exhausted, as multiple
    // events may already be queued in the socket. Processing all available
    // events now, rather than waiting for the next `epoll_wait`, reduces
    // CPU overhead and improves responsiveness.
    for (size_t i = 0; i < MAX_HIDRAW_EVENTS; i++) {
        ssize_t length = read(hidraw, buffer, sizeof(buffer));
        if (length == -1) {
            // With "O_NONBLOCK" enabled, "read" returns immediately rather than
            // waiting for data. An "EWOULDBLOCK" error is expected when no data
            // is available and does not indicate a failure. Break from the read
            // loop to continue the program control flow.
            if (errno == EWOULDBLOCK) {
                break;
            }
            // An "EIO" error indicates that the gamepad has been disconnected.
            // While this function still returns an error, printing "errno" is
            // unnecessary because disconnects are part of normal operation.
            if (errno != EIO) {
                perror("failed to read from gamepad");
            }
            return RET_ERROR;
        }

        // The Steam Controller puck exposes four "hidraw" devices regardless of
        // whether a controller is currently attached. If no message is received
        // within "GAMEPAD_TIMEOUT", the gamepad transitions back to
        // "GAMEPAD_STATE_PENDING". Since a non-disconnect, non-error message was
        // just received, restart the timer to preserve the current state.
        if (Stopwatch_reset(&gamepad->sw_timeout)) {
            return RET_ERROR;
        }

        struct ScGamepadState state;
        memcpy(&state, &gamepad->state, sizeof(state));
        ScGamepadState_update(&state, buffer, length);
        ScGamepadState_send(
            hidraw,
            gamepad->ui_gamepad,
            gamepad->ui_mouse,
            &gamepad->state,
            &state);
        memcpy(&gamepad->state, &state, sizeof(state));
    }

    return RET_OKAY;
}

int GamepadActive_uinput_event(struct GamepadActive* const gamepad) {
    struct input_event event;
    // Continue reading until `MAX_UINPUT_EVENTS` is exhausted, as multiple
    // events may already be queued in the socket. Processing all available
    // events now, rather than waiting for the next `epoll_wait`, reduces
    // CPU overhead and improves responsiveness.
    for (size_t i = 0; i < MAX_UINPUT_EVENTS; i++) {
        ssize_t length = read(gamepad->ui_gamepad, &event, sizeof(event));
        if (length < 0) {
            // With "O_NONBLOCK" enabled, "read" returns immediately rather than
            // waiting for data. An "EWOULDBLOCK" error is expected when no data
            // is available and does not indicate a failure. Break from the read
            // loop to continue the program control flow.
            if (errno == EWOULDBLOCK) {
                break;
            }
            perror("failed to read from virtual gamepad");
            return RET_ERROR;
        }

        if (event.type == EV_UINPUT) {
            if (event.code == UI_FF_UPLOAD) {
                struct uinput_ff_upload ff;
                memset(&ff, 0, sizeof(ff));
                ff.request_id = event.value;

                if (virtual_gamepad_ff_upload(gamepad->ui_gamepad, &ff)) {
                    return RET_ERROR;
                }

                if (ff.effect.type == FF_RUMBLE) {
                    gamepad->haptics.id              = SC_HAPTICS_ID_RUMBLE;
                    gamepad->haptics.u.rumble.strong = ff.effect.u.rumble.strong_magnitude;
                    gamepad->haptics.u.rumble.weak   = ff.effect.u.rumble.weak_magnitude;

                    gamepad->sw_haptics.triggered = true;
                }
            }

            if (event.code == UI_FF_ERASE) {
                gamepad->haptics.id = SC_HAPTICS_ID_NONE;

                struct uinput_ff_erase ff;
                memset(&ff, 0, sizeof(ff));
                ff.request_id = event.value;

                if (virtual_gamepad_ff_erase(gamepad->ui_gamepad, &ff)) {
                    return RET_ERROR;
                }
            }
        }
    }
    return RET_OKAY;
}
