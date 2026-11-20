/*
 * Nano GPIO adapter. Logic levels only. Not Sunbird wiring.
 * Lamp current and mains switching stay on an external driver board.
 */

#include "game.h"
#include "pins.h"

#include <Arduino.h>

static uint32_t g_strobe_until;
static uint8_t g_latched_sound;

typedef struct Debounce {
    uint8_t stable;
    uint8_t last_raw;
    uint32_t changed_ms;
} Debounce;

static Debounce g_red;
static Debounce g_blue;
static Debounce g_coin;
static Debounce g_test;

static const PinAssignment *role_pin(uint8_t role)
{
    uint8_t i;
    for (i = 0; i < ROLE_COUNT; i++) {
        if (kPinMap[i].role == role) {
            return &kPinMap[i];
        }
    }
    return &kPinMap[0];
}

static uint8_t debounce_bit(Debounce *d, uint8_t raw, uint32_t now)
{
    if (raw != d->last_raw) {
        d->last_raw = raw;
        d->changed_ms = now;
    } else if ((uint32_t)(now - d->changed_ms) >= 10u) {
        d->stable = raw;
    }
    return d->stable;
}

static uint8_t read_asserted(uint8_t role, uint32_t now, Debounce *d)
{
    const PinAssignment *pin = role_pin(role);
    uint8_t level = (uint8_t)digitalRead(pin->nano_pin);
    uint8_t raw = pin->active_low ? (level == LOW) : (level == HIGH);
    return debounce_bit(d, raw, now);
}

static void write_level(uint8_t role, uint8_t high)
{
    const PinAssignment *pin = role_pin(role);
    digitalWrite(pin->nano_pin, high ? HIGH : LOW);
}

static uint8_t sound_rank(uint8_t id)
{
    switch (id) {
    case SOUND_FREE_GAME:
        return 8;
    case SOUND_THUNDER:
        return 7;
    case SOUND_RETURN_FIRE:
        return 6;
    case SOUND_WHITE_HAT:
        return 5;
    case SOUND_EXPLOSION:
        return 4;
    case SOUND_GAME_OVER:
        return 3;
    case SOUND_START:
        return 2;
    case SOUND_COIN:
        return 1;
    default:
        return 0;
    }
}

void hw_setup(void)
{
    uint8_t i;

    /*
     * kPinMapStatus is the unconfirmed mark. PIN_MAP_UNCONFIRMED is the
     * same mark as a constant. Setup refuses to run only if the string
     * was replaced with an empty claim of confirmation.
     */
    (void)PIN_MAP_UNCONFIRMED;
    if (kPinMapStatus[0] == '\0') {
        return;
    }

    for (i = 0; i < ROLE_COUNT; i++) {
        if (kPinMap[i].output) {
            pinMode(kPinMap[i].nano_pin, OUTPUT);
            digitalWrite(kPinMap[i].nano_pin, LOW);
        } else {
            pinMode(kPinMap[i].nano_pin, INPUT_PULLUP);
        }
    }
}

void hw_read_inputs(GameInputs *in, uint32_t now_ms)
{
    if (in == 0) {
        return;
    }
    in->red_gun = read_asserted(ROLE_GUN_RED, now_ms, &g_red);
    in->blue_gun = read_asserted(ROLE_GUN_BLUE, now_ms, &g_blue);
    in->coin = read_asserted(ROLE_COIN, now_ms, &g_coin);
    in->test_mode = read_asserted(ROLE_TEST_PLAY, now_ms, &g_test);
}

void hw_present(const HardwareOutputs *out, uint32_t now_ms)
{
    uint8_t i;
    uint8_t best = SOUND_NONE;
    uint8_t rank = 0;
    LampId id;

    if (out == 0) {
        return;
    }

    for (i = 0; i < out->sound_count && i < SOUND_QUEUE; i++) {
        uint8_t r = sound_rank(out->sounds[i]);
        if (r > rank) {
            rank = r;
            best = out->sounds[i];
        }
    }

    if (out->sound_count > 0 && best != SOUND_NONE) {
        g_latched_sound = best;
        g_strobe_until = now_ms + 30u;
    }
    if ((int32_t)(g_strobe_until - now_ms) <= 0) {
        g_latched_sound = SOUND_NONE;
    }
    write_level(ROLE_SOUND_BIT0, g_latched_sound & 0x1u);
    write_level(ROLE_SOUND_BIT1, (g_latched_sound >> 1) & 0x1u);
    write_level(ROLE_SOUND_BIT2, (g_latched_sound >> 2) & 0x1u);
    write_level(ROLE_SOUND_BIT3, (g_latched_sound >> 3) & 0x1u);
    write_level(ROLE_SOUND_STROBE, g_latched_sound != SOUND_NONE);

    /*
     * Logic-level shift. LampId 0 is clocked first. The receiving board
     * is not part of this firmware and must not be a mains connection.
     */
    for (id = (LampId)0; id < LAMP_COUNT; id = (LampId)(id + 1)) {
        write_level(ROLE_LAMP_DATA, lamp_bits_get(out->lamps, id));
        write_level(ROLE_LAMP_CLOCK, 1);
        write_level(ROLE_LAMP_CLOCK, 0);
    }
    write_level(ROLE_LAMP_LATCH, 1);
    write_level(ROLE_LAMP_LATCH, 0);
}
