#ifndef SCWRAPPER_SC_GAMEPAD_STATE_H
#define SCWRAPPER_SC_GAMEPAD_STATE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Steam Controller
struct ScGamepadState {
    // face buttons
    bool btn_a;
    bool btn_b;
    bool btn_x;
    bool btn_y;
    // dpad buttons
    bool btn_dpad_up;
    bool btn_dpad_down;
    bool btn_dpad_left;
    bool btn_dpad_right;
    // left trigger and paddle buttons
    bool btn_l1;
    bool btn_l2;
    bool btn_l3;
    bool btn_l4;
    bool btn_l5;
    // right trigger and paddle buttons
    bool btn_r1;
    bool btn_r2;
    bool btn_r3;
    bool btn_r4;
    bool btn_r5;
    // menu buttons
    bool btn_steam;
    bool btn_menu;
    bool btn_view;
    bool btn_quick;

    // trigger axis
    int16_t abs_l2;
    int16_t abs_r2;
    // left thumbstick axis
    int16_t abs_thumbstickl_x;
    int16_t abs_thumbstickl_y;
    // right thumbstick axis
    int16_t abs_thumbstickr_x;
    int16_t abs_thumbstickr_y;


    bool      tpl_click; // left touchpad click
    bool      tpl_sense; // left touchpad sense
    int16_t   tpl_abs_x; // left touchpad x-axis
    int16_t   tpl_abs_y; // left touchpad y-axis

    bool      tpr_click; // right touchpad click
    bool      tpr_sense; // right touchpad sense
    int16_t   tpr_abs_x; // right touchpad x-axis
    int16_t   tpr_abs_y; // right touchpad y-axis
};

int ScGamepadState_update(struct ScGamepadState* const gamepad, uint8_t const* const buffer, size_t length);
void ScGamepadState_print(struct ScGamepadState const* const gamepad);

int ScGamepadState_send(
    int hidraw,
    int ui_gamepad,
    int ui_mouse,
    struct ScGamepadState const* const prev,
    struct ScGamepadState const* const curr
);

#endif // SCWRAPPER_SC_GAMEPAD_STATE_H
