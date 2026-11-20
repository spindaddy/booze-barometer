/*
 * Tombstone wall-game rules on a classic Arduino Nano (ATmega328P).
 *
 * The pin map is unconfirmed. No Sunbird schematic was available.
 * Outputs are logic levels for an external lamp and sound driver.
 * Do not wire these pins to mains lamps.
 */

#include <Arduino.h>

#include "game.h"
#include "nano_board.h"

static Game g_game;

void setup()
{
    hw_setup();
    game_init(&g_game, 0x544F4D42ul);
}

void loop()
{
    GameInputs in;
    HardwareOutputs out;
    uint32_t now = millis();

    hw_read_inputs(&in, now);
    game_tick(&g_game, &in, now);
    game_read_outputs(&g_game, &out);
    hw_present(&out, now);
}
