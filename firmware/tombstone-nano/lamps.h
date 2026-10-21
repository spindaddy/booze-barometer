#ifndef TOMBSTONE_LAMPS_H
#define TOMBSTONE_LAMPS_H

#include <stdint.h>

/*
 * Logical lamp channels for the flyer playfield.
 *
 * No ID comes from a manual or a schematic. None of these numbers is a
 * socket, a matrix row, or a Sunbird connector pin. The flyer names the
 * roles (time lamps, black hats, white hats, tombstones, sheriff flashes,
 * color-coded scores, "late shot" windows, the free-game lamp, attract
 * windows, heat lightning) and says each target has two lamps. Counts,
 * storefront-by-storefront placement, and the A/B split inside a pair are
 * inferred from that art. lamp_id_inferred() is 0 for a role the flyer
 * names directly and 1 for a placement this firmware inferred.
 *
 * Even a "named" role has an unknown physical bulb. Driver boards stay
 * outside this firmware.
 */

typedef enum LampId {
    /* Yellow time lamps along the left. Role named; count inferred. */
    LAMP_TIME_0 = 0,
    LAMP_TIME_1,
    LAMP_TIME_2,
    LAMP_TIME_3,
    LAMP_TIME_4,
    LAMP_TIME_5,
    LAMP_TIME_6,
    LAMP_TIME_7,

    /* Black hats that pop out. One pair per storefront, order inferred. */
    LAMP_POP_DRY_GOODS_A,
    LAMP_POP_DRY_GOODS_B,
    LAMP_POP_TONSORIAL_A,
    LAMP_POP_TONSORIAL_B,
    LAMP_POP_EATERY_A,
    LAMP_POP_EATERY_B,
    LAMP_POP_SALOON_A, /* Last Chance Saloon */
    LAMP_POP_SALOON_B,
    LAMP_POP_ASSAY_A,
    LAMP_POP_ASSAY_B,

    /* Black hats that move across the rooftops. Pairs inferred. */
    LAMP_ROOF_DRY_GOODS_A,
    LAMP_ROOF_DRY_GOODS_B,
    LAMP_ROOF_TONSORIAL_A,
    LAMP_ROOF_TONSORIAL_B,
    LAMP_ROOF_EATERY_A,
    LAMP_ROOF_EATERY_B,
    LAMP_ROOF_SALOON_A,
    LAMP_ROOF_SALOON_B,
    LAMP_ROOF_ASSAY_A,
    LAMP_ROOF_ASSAY_B,

    /* White hats. Role named; count and spots inferred. */
    LAMP_WHITE_0_A,
    LAMP_WHITE_0_B,
    LAMP_WHITE_1_A,
    LAMP_WHITE_1_B,
    LAMP_WHITE_2_A,
    LAMP_WHITE_2_B,
    LAMP_WHITE_3_A,
    LAMP_WHITE_3_B,

    /* Tombstones (R.I.P.). Role named; count inferred. Fast-hit flash. */
    LAMP_TOMBSTONE_0_A,
    LAMP_TOMBSTONE_0_B,
    LAMP_TOMBSTONE_1_A,
    LAMP_TOMBSTONE_1_B,

    /*
     * Named roles, contiguous so lamp_id_inferred() can range-check them.
     * Sheriff Colt flashes, shirt-colored score windows, "late shot"
     * windows, and the free-game lamp. A/B on a sheriff is the flyer's
     * two lamps per target, not a measured pair.
     */
    LAMP_SHERIFF_RED_A,
    LAMP_SHERIFF_RED_B,
    LAMP_SHERIFF_BLUE_A,
    LAMP_SHERIFF_BLUE_B,
    LAMP_SCORE_RED,
    LAMP_SCORE_BLUE,
    LAMP_LATE_RED,
    LAMP_LATE_BLUE,
    LAMP_FREE_GAME,

    /* Attract windows. Role named; one lamp per storefront is inferred. */
    LAMP_WINDOW_DRY_GOODS,
    LAMP_WINDOW_TONSORIAL,
    LAMP_WINDOW_EATERY,
    LAMP_WINDOW_SALOON,
    LAMP_WINDOW_ASSAY,
    LAMP_WINDOW_STAGECOACH, /* stagecoach is on the art; window use inferred */

    /* "Heat lightning" is a named attract effect. A private bulb is inferred. */
    LAMP_HEAT_LIGHTNING,

    LAMP_COUNT
} LampId;

#define LAMP_BYTES ((LAMP_COUNT + 7) / 8)

/* 0: flyer names this role. 1: count or position inferred from the art. */
static inline int lamp_id_inferred(LampId id)
{
    if (id >= LAMP_SHERIFF_RED_A && id <= LAMP_FREE_GAME) {
        return 0;
    }
    return 1;
}

static inline void lamp_bits_clear(uint8_t *bits)
{
    uint8_t i;
    for (i = 0; i < LAMP_BYTES; i++) {
        bits[i] = 0;
    }
}

static inline void lamp_bits_set(uint8_t *bits, LampId id)
{
    if ((unsigned)id >= LAMP_COUNT) {
        return;
    }
    bits[(unsigned)id >> 3] |= (uint8_t)(1u << ((unsigned)id & 7u));
}

static inline int lamp_bits_get(const uint8_t *bits, LampId id)
{
    if ((unsigned)id >= LAMP_COUNT) {
        return 0;
    }
    return (bits[(unsigned)id >> 3] >> ((unsigned)id & 7u)) & 1;
}

#endif
