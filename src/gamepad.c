#include "constants.h"
#include "gamepad.h"
#include "sc_epoll.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

int Gamepad_init(struct Gamepad* const gamepad, int hidraw) {
    // The Steam Controller puck exposes four "hidraw" devices regardless
    // of whether a controller is currently attached. To account for this
    // behavior, initialize the gamepad in "GAMEPAD_STATE_PENDING" and
    // defer creation of the virtual gamepad and mouse. This prevents
    // applications that list gamepads from displaying gamepads
    // that are not actually connected.
    *gamepad = (struct Gamepad) {
        .state  = GAMEPAD_STATE_PENDING,
        .hidraw = hidraw,
    };
    if (sc_epoll_ctl_add(hidraw, EPOLLIN | EPOLLRDHUP)) {
        (void)Gamepad_free(gamepad);
        return RET_ERROR;
    }
    return RET_OKAY;
}

int Gamepad_free(struct Gamepad* const gamepad) {
    int result = RET_OKAY;
    if (sc_epoll_ctl_remove(gamepad->hidraw)) {
        result = RET_ERROR;
    }
    if (close(gamepad->hidraw)) {
        perror("failed to close hidraw device");
        result = RET_ERROR;
    }
    if (gamepad->state == GAMEPAD_STATE_ACTIVE) {
        if (GamepadActive_free(&gamepad->u.active)) {
            result = RET_ERROR;
        }
    }
    gamepad->state = GAMEPAD_STATE_RELEASED;
    return result;
}

int Gamepad_into_active(struct Gamepad* const gamepad) {
    if (gamepad->state != GAMEPAD_STATE_PENDING) {
        return RET_ERROR;
    }
    if (GamepadActive_init(&gamepad->u.active)) {
        return RET_ERROR;
    }
    gamepad->state = GAMEPAD_STATE_ACTIVE;
    return RET_OKAY;
}

int Gamepad_into_pending(struct Gamepad* const gamepad) {
    if (gamepad->state != GAMEPAD_STATE_ACTIVE) {
        return RET_ERROR;
    }
    if (GamepadActive_free(&gamepad->u.active)) {
        return RET_ERROR;
    }
    gamepad->state = GAMEPAD_STATE_PENDING;
    return RET_OKAY;
}

int Gamepad_update(struct Gamepad* gamepad) {
    if (gamepad->state == GAMEPAD_STATE_ACTIVE) {
        int result = GamepadActive_update(&gamepad->u.active, gamepad->hidraw);
        if (result == RET_TIMEOUT) {
            return Gamepad_into_pending(gamepad);
        }
        if (result) {
            return RET_ERROR;
        }
    }
    return RET_OKAY;
}

int Gamepad_update_haptics(struct Gamepad* const gamepad) {
    if (gamepad->state == GAMEPAD_STATE_ACTIVE) {
        return GamepadActive_update_haptics(&gamepad->u.active, gamepad->hidraw);
    }
    return RET_OKAY;
}

int Gamepad_get_hidraw(struct Gamepad const* const gamepad) {
    return gamepad->hidraw;
}

int Gamepad_get_uinput(struct Gamepad const* const gamepad) {
    if (gamepad->state == GAMEPAD_STATE_ACTIVE) {
        return gamepad->u.active.ui_gamepad;
    }
    return -1;
}

int Gamepad_hidraw_event(struct Gamepad* const gamepad) {
    if (gamepad->state == GAMEPAD_STATE_PENDING) {
        if (Gamepad_into_active(gamepad)) {
            return RET_ERROR;
        }
    }
    if (gamepad->state == GAMEPAD_STATE_ACTIVE) {
        if (GamepadActive_hidraw_event(&gamepad->u.active, gamepad->hidraw)) {
            return RET_ERROR;
        }
    }
    return RET_OKAY;
}

int Gamepad_uinput_event(struct Gamepad* const gamepad) {
    if (gamepad->state == GAMEPAD_STATE_ACTIVE) {
        if (GamepadActive_uinput_event(&gamepad->u.active)) {
            return RET_ERROR;
        }
    }
    return RET_OKAY;
}
