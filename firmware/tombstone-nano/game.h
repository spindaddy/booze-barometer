#ifndef TOMBSTONE_GAME_H
#define TOMBSTONE_GAME_H

#include <stdint.h>

#include "hw.h"
#include "lamps.h"
#include "operator.h"
#include "sound.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Flyer rules, with timing and point values chosen so the game can run.
 * Those numbers are not from a manual. See the README.
 *
 * One armed figure at a time. The transmitters are fire buttons, so a
 * shot resolves whatever is armed: a black hat in its fast window scores,
 * the same hat in the following window is return fire, a white hat costs
 * points. Two players shoot the same armed figure; the first sample wins.
 * A tie inside one sample is awarded to red because red is sampled first.
 */

enum GamePhase {
    PHASE_ATTRACT = 0,
    PHASE_JOIN,
    PHASE_PLAY,
    PHASE_GAME_OVER,
    PHASE_TEST
};

enum PlayerId {
    PLAYER_RED = 0,
    PLAYER_BLUE = 1
};

enum TargetId {
    TARGET_POP_DRY_GOODS = 0,
    TARGET_POP_TONSORIAL,
    TARGET_POP_EATERY,
    TARGET_POP_SALOON,
    TARGET_POP_ASSAY,
    TARGET_ROOF,
    TARGET_WHITE_0,
    TARGET_WHITE_1,
    TARGET_WHITE_2,
    TARGET_WHITE_3,
    TARGET_COUNT,
    TARGET_NONE = 255
};

/* Firmware choices. Not printed on the flyer except the free-game default. */
enum {
    JOIN_WINDOW_MS = 2000,
    FAST_WINDOW_MS = 600,
    SLOW_WINDOW_MS = 500,
    GAP_MS = 300,
    TIME_LAMP_MS = 4000,
    TIME_LAMP_COUNT = 8,
    PLAY_TIME_MS = TIME_LAMP_COUNT * TIME_LAMP_MS,
    GAME_OVER_MS = 4000,
    TOMBSTONE_MS = 800,
    SHERIFF_FLASH_MS = 180,
    LATE_HOLD_MS = 700,
    ATTRACT_WINDOW_MS = 400,
    LIGHTNING_PERIOD_MS = 2400,
    LIGHTNING_ON_MS = 80,
    TEST_STEP_MS = 250,
    BLINK_HALF_MS = 200,
    SCORE_FAST_HIT = 10,
    SCORE_PENALTY = 5,
    CREDIT_MAX = 9,
    SCORE_CAP = 999,
    ROOF_STEPS = 5,
    WINDOW_COUNT = 6
};

typedef struct Game {
    uint32_t rng;
    uint32_t now;
    uint32_t beat_started;
    uint32_t play_started;
    uint32_t phase_started;
    uint32_t tombstone_until;
    uint32_t sheriff_until[2];
    uint32_t late_until[2];
    const uint8_t *script;
    uint16_t score[2];
    uint16_t free_threshold;
    uint8_t script_len;
    uint8_t script_pos;
    uint8_t phase;
    uint8_t credits;
    uint8_t joined;
    uint8_t beat;
    uint8_t target;
    uint8_t roof_step;
    uint8_t resolved;
    uint8_t free_awarded;
    uint8_t prev_red;
    uint8_t prev_blue;
    uint8_t prev_coin;
    uint8_t prev_test;
    uint8_t suppress_shots;
    uint8_t sound_n;
    uint8_t sound_q[SOUND_QUEUE];
    uint8_t lamps[LAMP_BYTES];
} Game;

void game_init(Game *g, uint32_t seed);
void game_set_free_game_score(Game *g, uint16_t score);
uint16_t game_free_game_score(const Game *g);
void game_set_target_script(Game *g, const uint8_t *targets, uint8_t count);

void game_tick(Game *g, const GameInputs *in, uint32_t now_ms);

uint8_t game_phase(const Game *g);
uint16_t game_score(const Game *g, uint8_t player);
uint8_t game_credits(const Game *g);
uint8_t game_players(const Game *g);
uint8_t game_free_game_awarded(const Game *g);
uint8_t game_current_target(const Game *g);
int game_lamp(const Game *g, LampId id);
uint8_t game_sound_count(const Game *g);
SoundId game_sound_at(const Game *g, uint8_t index);
void game_read_outputs(const Game *g, HardwareOutputs *out);

#ifdef __cplusplus
}
#endif

#endif
