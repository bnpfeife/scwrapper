#ifndef SCWRAPPER_GAMEPAD_H
#define SCWRAPPER_GAMEPAD_H

#include "sc_gamepad_state.h"
#include "sc_haptics.h"
#include "stopwatch.h"

#include <stddef.h>
#include <stdint.h>

#define GAMEPAD_STATE_PENDING  0
#define GAMEPAD_STATE_ACTIVE   1
#define GAMEPAD_STATE_RELEASED 2

#define GAMEPAD_TIMEOUT            1000 // milliseconds
#define GAMEPAD_CONFIGURE_INTERVAL 5000 // milliseconds
#define GAMEPAD_HAPTICS_INTERVAL   40   // milliseconds

struct GamepadActive {
    int ui_gamepad;
    int ui_mouse;
    struct ScGamepadState state;
    struct ScHaptics haptics;
    struct Stopwatch sw_timeout;
    struct Stopwatch sw_configure;
    struct Stopwatch sw_haptics;
};

struct Gamepad {
    int state;
    int hidraw;
    union {
        /* struct GamepadPending pending; */
        struct GamepadActive active;
    } u;
};

int Gamepad_init(struct Gamepad* const gamepad, int hidraw);
int Gamepad_free(struct Gamepad* const gamepad);
int Gamepad_update(struct Gamepad* const gamepad);
int Gamepad_update_haptics(struct Gamepad* const gamepad);
int Gamepad_get_hidraw(struct Gamepad const* const gamepad);
int Gamepad_get_uinput(struct Gamepad const* const gamepad);
int Gamepad_hidraw_event(struct Gamepad* const gamepad);
int Gamepad_uinput_event(struct Gamepad* const gamepad);
int Gamepad_into_active(struct Gamepad* const gamepad);
int Gamepad_into_pending(struct Gamepad* const gamepad);

int GamepadActive_init(struct GamepadActive* const gamepad);
int GamepadActive_free(struct GamepadActive* const gamepad);
int GamepadActive_update(struct GamepadActive* const gamepad, int hidraw);
int GamepadActive_update_haptics(struct GamepadActive* const gamepad, int hidraw);
int GamepadActive_hidraw_event(struct GamepadActive* const gamepad, int hidraw);
int GamepadActive_uinput_event(struct GamepadActive* const gamepad);

#endif // SCWRAPPER_GAMEPAD_H
