#ifndef TOMBSTONE_HW_H
#define TOMBSTONE_HW_H

#include <stdint.h>

#include "lamps.h"
#include "sound.h"

/*
 * Hardware boundary. Game rules never read pins. The Nano adapter fills
 * GameInputs and presents HardwareOutputs. Lamp bytes are logic levels
 * for an external driver. This firmware does not switch mains lamps.
 *
 * LampId 0 is bit 0 of lamps[0]. A shift-register board should clock
 * LampId 0 first and LAMP_HEAT_LIGHTNING last. That order is a suggestion
 * for a new driver, not Sunbird wiring.
 */

typedef struct GameInputs {
    uint8_t red_gun;   /* 1 = trigger pressed */
    uint8_t blue_gun;
    uint8_t coin;      /* 1 = coin switch asserted */
    uint8_t test_mode; /* 1 = test, 0 = play */
} GameInputs;

typedef struct HardwareOutputs {
    uint16_t score_red;  /* integer score; digit wiring is unknown */
    uint16_t score_blue;
    uint8_t credits;
    uint8_t phase;
    uint8_t players;
    uint8_t sound_count;
    uint8_t sounds[SOUND_QUEUE];
    uint8_t lamps[LAMP_BYTES];
} HardwareOutputs;

#endif
