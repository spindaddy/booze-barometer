#ifndef TOMBSTONE_SOUND_H
#define TOMBSTONE_SOUND_H

#include <stdint.h>

/*
 * Sound events the flyer describes. This firmware does not synthesize
 * audio and does not drive the 3 by 5 inch speakers. A board outside
 * the sketch can map these ids to whatever sound hardware is fitted.
 * Values fit in four logic-level bits (see the unconfirmed pin map).
 */

typedef enum SoundId {
    SOUND_NONE = 0,
    SOUND_COIN = 1,
    SOUND_START = 2,
    SOUND_EXPLOSION = 3,    /* sheriff Colt */
    SOUND_THUNDER = 4,      /* fast hit: thunder and lightning */
    SOUND_RETURN_FIRE = 5,  /* slow shot, black hat shoots back */
    SOUND_WHITE_HAT = 6,
    SOUND_FREE_GAME = 7,
    SOUND_GAME_OVER = 8
} SoundId;

#define SOUND_QUEUE 6

#endif
