#include "pins.h"

const char kPinMapStatus[] = "UNCONFIRMED";

/*
 * Suggested Nano pins only. D0 and D1 stay free for serial.
 * D13 stays free so the onboard LED is not part of the lamp bus.
 * Pin 14 is Arduino A0.
 */
const PinAssignment kPinMap[ROLE_COUNT] = {
    {ROLE_GUN_RED, 2, 0, 1},
    {ROLE_GUN_BLUE, 3, 0, 1},
    {ROLE_COIN, 4, 0, 1},
    {ROLE_TEST_PLAY, 5, 0, 1},
    {ROLE_SOUND_BIT0, 6, 1, 0},
    {ROLE_SOUND_BIT1, 7, 1, 0},
    {ROLE_SOUND_BIT2, 8, 1, 0},
    {ROLE_SOUND_BIT3, 9, 1, 0},
    {ROLE_SOUND_STROBE, 10, 1, 0},
    {ROLE_LAMP_DATA, 11, 1, 0},
    {ROLE_LAMP_CLOCK, 12, 1, 0},
    {ROLE_LAMP_LATCH, 14, 1, 0},
};
