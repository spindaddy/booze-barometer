#ifndef TOMBSTONE_PINS_H
#define TOMBSTONE_PINS_H

#include <stdint.h>

/*
 * UNCONFIRMED PIN MAP.
 *
 * Suggested GPIO for a classic Arduino Nano (ATmega328P). These are not
 * Sunbird board pin numbers. No schematic was available, and this map
 * must not be read as a match for the original wiring.
 *
 * Every output is a logic-level signal at the Nano's Vcc (usually 5 V).
 * Do not connect these pins to mains lamps. Shift registers, MOSFETs,
 * and triacs belong on a driver board outside this firmware. The lamp
 * channels in lamps.h are not assigned to Nano pins one-to-one; three
 * pins are a logic-level data/clock/latch bus for that external board.
 *
 * Input polarity below is an assumption (switch to ground, internal
 * pull-up). Measure the harness before trusting it.
 */

#define PIN_MAP_UNCONFIRMED 1

enum PinRole {
    ROLE_GUN_RED = 0,
    ROLE_GUN_BLUE,
    ROLE_COIN,
    ROLE_TEST_PLAY,
    ROLE_SOUND_BIT0,
    ROLE_SOUND_BIT1,
    ROLE_SOUND_BIT2,
    ROLE_SOUND_BIT3,
    ROLE_SOUND_STROBE,
    ROLE_LAMP_DATA,
    ROLE_LAMP_CLOCK,
    ROLE_LAMP_LATCH,
    ROLE_COUNT
};

typedef struct PinAssignment {
    uint8_t role;
    uint8_t nano_pin;   /* Arduino pin index, not a Sunbird pin */
    uint8_t output;     /* 1 = logic-level output, 0 = input */
    uint8_t active_low; /* inputs only; 1 = asserted when the pin reads low */
} PinAssignment;

/* Short mark kept as a symbol so the map cannot be mistaken for a schematic. */
extern const char kPinMapStatus[];
extern const PinAssignment kPinMap[ROLE_COUNT];

#endif
