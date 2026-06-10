#include "constants.h"
#include "triton.h"

#include <linux/hidraw.h>
#include <math.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

struct __attribute__((packed)) TritonHapticsRumble {
    uint8_t  type;        // 1 byte
    uint16_t intensity;   // 2 bytes
    uint16_t left_speed;  // 2 bytes
    int8_t   left_gain;   // 1 byte
    uint16_t right_speed; // 2 bytes
    int8_t   right_gain;  // 1 byte
};

#define TRITON_HAPTICS_RUMBLE_SIZE 10 // 9 bytes + 1 byte (report_id)

struct __attribute__((packed)) TritonHapticsLfoTone {
    uint8_t  channel;       // 1 byte
    int8_t   gain;          // 1 byte
    uint16_t frequency;     // 2 bytes
    uint16_t duration_ms;   // 2 bytes
    uint16_t lfo_frequency; // 2 bytes
    uint8_t  lfo_depth;     // 1 byte
};

#define TRITON_HAPTICS_LFO_TONE_SIZE 10 // 9 bytes + 1 byte (report_id)

struct __attribute__((packed)) TritonMsg {
    uint8_t report_id; // 1 byte
    union {
        struct TritonHapticsRumble  rumble; // 9 bytes
        struct TritonHapticsLfoTone lfo;    // 9 bytes
    } u;
};

static uint8_t const TRITON_DISABLE_LIZARD_MODE[64] = {
    0x01,                          // Report ID
    TRITON_ID_SET_SETTINGS_VALUES, //
    0x03,                          // length
    TRITON_SETTING_LIZARD_MODE,    //
    0x00,                          // disable (lo)
    0x00,                          // disable (hi)
};

static uint8_t const TRITON_CLEAR_DIGITAL_MAPPINGS[64] = {
   0x01,                             // Report ID
   TRITON_ID_CLEAR_DIGITAL_MAPPINGS, //
   0x00,                             // length
};

int triton_disable_lizard_mode(int fd) {
    if (ioctl(fd, HIDIOCSFEATURE(sizeof(TRITON_DISABLE_LIZARD_MODE)), TRITON_DISABLE_LIZARD_MODE) < 0) {
        perror("could not disable lizard mode");
        return RET_ERROR;
    }
    return RET_OKAY;
}

int triton_clear_digital_mappings(int fd) {
    if (ioctl(fd, HIDIOCSFEATURE(sizeof(TRITON_CLEAR_DIGITAL_MAPPINGS)), TRITON_CLEAR_DIGITAL_MAPPINGS) < 0) {
        perror("could not disable digital mappings");
        return RET_ERROR;
    }
    return RET_OKAY;
}

#define RUMBLE_GAIN 6 // unknown units
#define RUMBLE_SPEED UINT16_MAX // unknown units
#define RUMBLE_INTENSITY_FLOOR 49152.0 // (48 * 1024)
#define RUMBLE_INTENSITY_RANGE 16383.0 // (16 * 1024) - 1

int triton_haptics_rumble(int fd, struct ScHapticsRumble rumble) {
    // NOTICE(bnpfeife): 2026-05-29
    //
    // Unfortunately, "intensity", "gain", and "speed" were derived through
    // subjective testing. Adjusting each parameter affects the perceived
    // rumble strength in a non-obvious way. While the algorithm used by
    // this function does not reflect the motor's physical behavior, it
    // seems to produce reasonable results across the games tested.
    //
    // SDL maps "left_speed" and "right_speed" to "rumble.strong" and
    // "rumble.weak" respectively. This produces incorrect behavior because
    // "speed" isn't strongly correlated with rumble intensity. The problem
    // is particularly noticeable in games that depend on layered rumble
    // effects. For example, in Grand Turismo 1, road vibration and
    // collisions interfere rather than "adding" together.
    //
    // Some additional thoughts:
    //
    // It may be the case that "gain" is measured in decibels. If we assume
    // that "rumble.strong" and "rumble.weak" are linear and "0" represents
    // the weakest rumble and "65535" represents the strongest rumble, then
    // the relationship between the two should be roughly:
    //
    //   intensity = 2^(10 / gain) OR gain = 10 * log2(intensity)
    //
    // However, I couldn't make this *feel* correct. If anyone has more
    // information please feel free to submit a PR or contact me!
    double norm_l = (double)rumble.strong / UINT16_MAX;
    double norm_r = (double)rumble.weak   / UINT16_MAX;
    uint16_t l = RUMBLE_INTENSITY_FLOOR + (RUMBLE_INTENSITY_RANGE * sqrt(norm_l));
    uint16_t r = RUMBLE_INTENSITY_FLOOR + (RUMBLE_INTENSITY_RANGE * sqrt(norm_r));
    struct TritonMsg message = {
        .report_id = TRITON_ID_OUT_HAPTICS_RUMBLE,
        .u.rumble = {
            // "type" is unknown
            .type = 0,
            // "intensity" is inverted
            .intensity  = UINT16_MAX - (l > r ? l : r),
            .left_gain  = RUMBLE_GAIN,
            .right_gain = RUMBLE_GAIN,
            // If "left_speed" or "right_speed" is zero, the corresponding
            // LRA motor will remain inactive. As a result, rumble events
            // can still target a single motor when desired. The units of
            // these fields are currently unknown. The values likely
            // correspond to values within a frequency range.
            .left_speed  = rumble.strong ? RUMBLE_SPEED : 0,
            .right_speed = rumble.weak   ? RUMBLE_SPEED : 0,
        },
    };
    if (write(fd, &message, TRITON_HAPTICS_RUMBLE_SIZE) != TRITON_HAPTICS_RUMBLE_SIZE) {
        perror("could not write rumble event");
        return RET_ERROR;
    }
    return RET_OKAY;
}

int triton_haptics_lfo_tone(int fd, struct ScHapticsLfoTone lfo_tone) {
    struct TritonMsg message = {
        .report_id = TRITON_ID_OUT_HAPTICS_LFO_TONE,
        .u.lfo = {
            .channel       = lfo_tone.channel,
            .gain          = lfo_tone.gain,
            .frequency     = lfo_tone.frequency,
            .duration_ms   = lfo_tone.duration_ms,
            .lfo_frequency = lfo_tone.lfo_frequency,
            .lfo_depth     = lfo_tone.lfo_depth,
        },
    };
    if (write(fd, &message, TRITON_HAPTICS_LFO_TONE_SIZE) != TRITON_HAPTICS_LFO_TONE_SIZE) {
        perror("could not write rumble event");
        return RET_ERROR;
    }
    return RET_OKAY;
}
