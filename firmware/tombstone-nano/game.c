#include "game.h"

#include <string.h>

_Static_assert(TARGET_POP_DRY_GOODS == 0, "pop table is indexed by target id");
_Static_assert(TARGET_POP_ASSAY == 4, "pop table is indexed by target id");
_Static_assert(TARGET_WHITE_3 == TARGET_WHITE_0 + 3, "white table");
_Static_assert(LAMP_TIME_7 == LAMP_TIME_0 + TIME_LAMP_COUNT - 1, "time lamps");
_Static_assert(LAMP_FREE_GAME > LAMP_SHERIFF_RED_A, "named lamp range");

enum {
    BEAT_GAP = 0,
    BEAT_LIVE = 1,
    BEAT_LATE = 2
};

static const LampId kPop[5][2] = {
    {LAMP_POP_DRY_GOODS_A, LAMP_POP_DRY_GOODS_B},
    {LAMP_POP_TONSORIAL_A, LAMP_POP_TONSORIAL_B},
    {LAMP_POP_EATERY_A, LAMP_POP_EATERY_B},
    {LAMP_POP_SALOON_A, LAMP_POP_SALOON_B},
    {LAMP_POP_ASSAY_A, LAMP_POP_ASSAY_B},
};

static const LampId kRoof[ROOF_STEPS][2] = {
    {LAMP_ROOF_DRY_GOODS_A, LAMP_ROOF_DRY_GOODS_B},
    {LAMP_ROOF_TONSORIAL_A, LAMP_ROOF_TONSORIAL_B},
    {LAMP_ROOF_EATERY_A, LAMP_ROOF_EATERY_B},
    {LAMP_ROOF_SALOON_A, LAMP_ROOF_SALOON_B},
    {LAMP_ROOF_ASSAY_A, LAMP_ROOF_ASSAY_B},
};

static const LampId kWhite[4][2] = {
    {LAMP_WHITE_0_A, LAMP_WHITE_0_B},
    {LAMP_WHITE_1_A, LAMP_WHITE_1_B},
    {LAMP_WHITE_2_A, LAMP_WHITE_2_B},
    {LAMP_WHITE_3_A, LAMP_WHITE_3_B},
};

static const LampId kWindows[WINDOW_COUNT] = {
    LAMP_WINDOW_DRY_GOODS,
    LAMP_WINDOW_TONSORIAL,
    LAMP_WINDOW_EATERY,
    LAMP_WINDOW_SALOON,
    LAMP_WINDOW_ASSAY,
    LAMP_WINDOW_STAGECOACH,
};

static int in_future(uint32_t now, uint32_t deadline)
{
    return (int32_t)(deadline - now) > 0;
}

static int blink_on(uint32_t now)
{
    return ((now / BLINK_HALF_MS) % 2u) == 0u;
}

static void push_sound(Game *g, SoundId id)
{
    if (g->sound_n >= SOUND_QUEUE) {
        return;
    }
    g->sound_q[g->sound_n++] = (uint8_t)id;
}

static int is_white(uint8_t target)
{
    return target >= TARGET_WHITE_0 && target <= TARGET_WHITE_3;
}

static int is_roof(uint8_t target)
{
    return target == TARGET_ROOF;
}

static int is_black(uint8_t target)
{
    return target <= TARGET_POP_ASSAY || target == TARGET_ROOF;
}

static uint32_t lcg(Game *g)
{
    g->rng = g->rng * 1664525u + 1013904223u;
    return g->rng;
}

static uint8_t next_target(Game *g)
{
    uint8_t t;
    uint8_t r;

    if (g->script != 0 && g->script_len > 0) {
        t = g->script[g->script_pos];
        g->script_pos = (uint8_t)((g->script_pos + 1u) % g->script_len);
        if (t >= TARGET_COUNT) {
            t = TARGET_POP_DRY_GOODS;
        }
        return t;
    }

    r = (uint8_t)(lcg(g) % 10u);
    if (r < 5u) {
        return (uint8_t)(TARGET_POP_DRY_GOODS + r);
    }
    if (r == 5u) {
        return TARGET_ROOF;
    }
    return (uint8_t)(TARGET_WHITE_0 + (r - 6u));
}

static void check_free_game(Game *g)
{
    uint16_t combined;

    if (g->free_awarded || g->free_threshold == 0) {
        return;
    }
    combined = (uint16_t)(g->score[PLAYER_RED] + g->score[PLAYER_BLUE]);
    if (combined < g->free_threshold) {
        return;
    }
    g->free_awarded = 1;
    if (g->credits < CREDIT_MAX) {
        g->credits++;
    }
    push_sound(g, SOUND_FREE_GAME);
}

static void add_score(Game *g, uint8_t player, uint16_t points)
{
    uint16_t s = g->score[player];
    if (s > (uint16_t)(SCORE_CAP - points)) {
        s = SCORE_CAP;
    } else {
        s = (uint16_t)(s + points);
    }
    g->score[player] = s;
    check_free_game(g);
}

static void sub_score(Game *g, uint8_t player, uint16_t points)
{
    if (g->score[player] <= points) {
        g->score[player] = 0;
    } else {
        g->score[player] = (uint16_t)(g->score[player] - points);
    }
}

static void begin_play(Game *g)
{
    g->phase = PHASE_PLAY;
    g->play_started = g->now;
    g->score[PLAYER_RED] = 0;
    g->score[PLAYER_BLUE] = 0;
    g->free_awarded = 0;
    g->beat = BEAT_GAP;
    g->beat_started = g->now;
    g->resolved = 1;
    g->target = 0;
    g->roof_step = 0;
    g->tombstone_until = g->now;
    g->sheriff_until[0] = g->now;
    g->sheriff_until[1] = g->now;
    g->late_until[0] = g->now;
    g->late_until[1] = g->now;
    push_sound(g, SOUND_START);
}

static void try_join(Game *g, uint8_t player)
{
    uint8_t bit;

    if (player > PLAYER_BLUE) {
        return;
    }
    if (g->phase != PHASE_ATTRACT && g->phase != PHASE_JOIN) {
        return;
    }
    bit = (uint8_t)(1u << player);
    if (g->joined & bit) {
        return;
    }
    if (g->credits == 0) {
        return;
    }
    g->credits--;
    g->joined = (uint8_t)(g->joined | bit);
    g->suppress_shots = 1;
    if (g->phase == PHASE_ATTRACT) {
        g->phase = PHASE_JOIN;
        g->phase_started = g->now;
    }
    /* Both seats filled, or no credit left for a second player: start now. */
    if (g->joined == 0x3u || g->credits == 0) {
        begin_play(g);
    }
}

static void enter_game_over(Game *g)
{
    g->phase = PHASE_GAME_OVER;
    g->phase_started = g->now;
    g->beat = BEAT_GAP;
    g->resolved = 1;
    push_sound(g, SOUND_GAME_OVER);
}

static uint32_t beat_duration(const Game *g)
{
    if (g->beat == BEAT_GAP) {
        return GAP_MS;
    }
    if (g->beat == BEAT_LATE) {
        return SLOW_WINDOW_MS;
    }
    return FAST_WINDOW_MS;
}

static void step_beat(Game *g)
{
    if (g->beat == BEAT_GAP) {
        g->target = next_target(g);
        g->roof_step = 0;
        g->resolved = 0;
        g->beat = BEAT_LIVE;
        return;
    }
    if (g->beat == BEAT_LIVE) {
        if (!is_white(g->target) && !g->resolved && is_roof(g->target) &&
            (g->roof_step + 1u) < ROOF_STEPS) {
            g->roof_step++;
            return;
        }
        if (!is_white(g->target) && !g->resolved) {
            g->beat = BEAT_LATE;
            return;
        }
        g->beat = BEAT_GAP;
        return;
    }
    g->beat = BEAT_GAP;
}

static void advance_beats(Game *g)
{
    int n;

    for (n = 0; n < 64; n++) {
        uint32_t dur = beat_duration(g);
        if ((g->now - g->beat_started) < dur) {
            return;
        }
        g->beat_started += dur;
        step_beat(g);
    }
    g->beat = BEAT_GAP;
    g->beat_started = g->now;
    g->resolved = 1;
}

static void end_presentation(Game *g)
{
    g->beat = BEAT_GAP;
    g->beat_started = g->now;
    g->resolved = 1;
}

static void fire(Game *g, uint8_t player)
{
    uint8_t bit = (uint8_t)(1u << player);

    if ((g->joined & bit) == 0 || g->phase != PHASE_PLAY) {
        return;
    }

    g->sheriff_until[player] = g->now + SHERIFF_FLASH_MS;
    push_sound(g, SOUND_EXPLOSION);

    if (g->beat == BEAT_LIVE && !g->resolved && is_black(g->target)) {
        add_score(g, player, SCORE_FAST_HIT);
        g->tombstone_until = g->now + TOMBSTONE_MS;
        push_sound(g, SOUND_THUNDER);
        end_presentation(g);
        return;
    }
    if (g->beat == BEAT_LIVE && !g->resolved && is_white(g->target)) {
        sub_score(g, player, SCORE_PENALTY);
        push_sound(g, SOUND_WHITE_HAT);
        end_presentation(g);
        return;
    }
    if (g->beat == BEAT_LATE && !g->resolved) {
        sub_score(g, player, SCORE_PENALTY);
        g->late_until[player] = g->now + LATE_HOLD_MS;
        push_sound(g, SOUND_RETURN_FIRE);
        end_presentation(g);
    }
}

static void light_pair(Game *g, LampId a, LampId b)
{
    lamp_bits_set(g->lamps, a);
    lamp_bits_set(g->lamps, b);
}

static void light_current_target(Game *g)
{
    uint8_t step;
    uint8_t index;

    if (g->target <= TARGET_POP_ASSAY) {
        light_pair(g, kPop[g->target][0], kPop[g->target][1]);
        return;
    }
    if (g->target == TARGET_ROOF) {
        step = g->roof_step;
        if (step >= ROOF_STEPS) {
            step = ROOF_STEPS - 1;
        }
        light_pair(g, kRoof[step][0], kRoof[step][1]);
        return;
    }
    if (is_white(g->target)) {
        index = (uint8_t)(g->target - TARGET_WHITE_0);
        light_pair(g, kWhite[index][0], kWhite[index][1]);
    }
}

static void render_time(Game *g)
{
    uint32_t elapsed;
    uint32_t steps;
    uint8_t remaining;
    uint8_t i;
    int solid_blink;

    if (g->phase != PHASE_PLAY) {
        return;
    }
    elapsed = g->now - g->play_started;
    steps = elapsed / TIME_LAMP_MS;
    if (steps > TIME_LAMP_COUNT) {
        steps = TIME_LAMP_COUNT;
    }
    remaining = (uint8_t)(TIME_LAMP_COUNT - (uint8_t)steps);
    solid_blink = ((elapsed / BLINK_HALF_MS) % 2u) == 0u;
    for (i = 0; i < remaining; i++) {
        if ((uint8_t)(i + 1u) == remaining && !solid_blink) {
            continue;
        }
        lamp_bits_set(g->lamps, (LampId)(LAMP_TIME_0 + i));
    }
}

static void render(Game *g)
{
    uint8_t i;
    int show_target;

    lamp_bits_clear(g->lamps);

    if (g->phase == PHASE_TEST) {
        uint32_t step = (g->now - g->phase_started) / TEST_STEP_MS;
        lamp_bits_set(g->lamps, (LampId)(step % (uint32_t)LAMP_COUNT));
        return;
    }

    render_time(g);

    show_target = (g->phase == PHASE_PLAY) &&
                  (g->beat == BEAT_LIVE || g->beat == BEAT_LATE);
    if (show_target) {
        if (g->beat == BEAT_LIVE || blink_on(g->now)) {
            light_current_target(g);
        }
    }

    if (g->phase == PHASE_JOIN || g->phase == PHASE_PLAY ||
        g->phase == PHASE_GAME_OVER) {
        for (i = 0; i < 2u; i++) {
            int on = (g->joined & (uint8_t)(1u << i)) != 0;
            if (g->phase == PHASE_JOIN) {
                on = on && blink_on(g->now);
            }
            if (on) {
                lamp_bits_set(g->lamps,
                              i == PLAYER_RED ? LAMP_SCORE_RED : LAMP_SCORE_BLUE);
            }
        }
    }

    for (i = 0; i < 2u; i++) {
        int late_on = in_future(g->now, g->late_until[i]);
        if (g->phase == PHASE_PLAY && g->beat == BEAT_LATE &&
            (g->joined & (uint8_t)(1u << i)) && blink_on(g->now)) {
            late_on = 1;
        }
        if (late_on) {
            lamp_bits_set(g->lamps, i == PLAYER_RED ? LAMP_LATE_RED : LAMP_LATE_BLUE);
        }
        if (in_future(g->now, g->sheriff_until[i])) {
            if (i == PLAYER_RED) {
                light_pair(g, LAMP_SHERIFF_RED_A, LAMP_SHERIFF_RED_B);
            } else {
                light_pair(g, LAMP_SHERIFF_BLUE_A, LAMP_SHERIFF_BLUE_B);
            }
        }
    }

    if (in_future(g->now, g->tombstone_until)) {
        light_pair(g, LAMP_TOMBSTONE_0_A, LAMP_TOMBSTONE_0_B);
        light_pair(g, LAMP_TOMBSTONE_1_A, LAMP_TOMBSTONE_1_B);
        lamp_bits_set(g->lamps, LAMP_HEAT_LIGHTNING);
    }

    if (g->phase == PHASE_ATTRACT) {
        uint8_t w = (uint8_t)((g->now / ATTRACT_WINDOW_MS) % WINDOW_COUNT);
        lamp_bits_set(g->lamps, kWindows[w]);
        if ((g->now % LIGHTNING_PERIOD_MS) < LIGHTNING_ON_MS) {
            lamp_bits_set(g->lamps, LAMP_HEAT_LIGHTNING);
        }
    }

    if (g->free_awarded) {
        lamp_bits_set(g->lamps, LAMP_FREE_GAME);
    }
}

void game_init(Game *g, uint32_t seed)
{
    if (g == 0) {
        return;
    }
    memset(g, 0, sizeof *g);
    g->rng = seed == 0 ? 1u : seed;
    g->phase = PHASE_ATTRACT;
    g->free_threshold = TOMBSTONE_FREE_GAME_SCORE;
    g->beat = BEAT_GAP;
    g->now = 0;
    render(g);
}

void game_set_free_game_score(Game *g, uint16_t score)
{
    if (g == 0) {
        return;
    }
    g->free_threshold = score;
}

uint16_t game_free_game_score(const Game *g)
{
    if (g == 0) {
        return 0;
    }
    return g->free_threshold;
}

void game_set_target_script(Game *g, const uint8_t *targets, uint8_t count)
{
    if (g == 0) {
        return;
    }
    if (targets == 0 || count == 0) {
        g->script = 0;
        g->script_len = 0;
        g->script_pos = 0;
        return;
    }
    g->script = targets;
    g->script_len = count;
    g->script_pos = 0;
}

void game_tick(Game *g, const GameInputs *in, uint32_t now_ms)
{
    if (g == 0 || in == 0) {
        return;
    }

    g->now = now_ms;
    g->sound_n = 0;
    g->suppress_shots = 0;

    if (in->test_mode) {
        if (g->phase != PHASE_TEST) {
            g->phase = PHASE_TEST;
            g->phase_started = now_ms;
            g->joined = 0;
            g->beat = BEAT_GAP;
            g->resolved = 1;
        }
        g->prev_red = in->red_gun;
        g->prev_blue = in->blue_gun;
        g->prev_coin = in->coin;
        g->prev_test = 1;
        render(g);
        return;
    }

    if (g->phase == PHASE_TEST) {
        g->phase = PHASE_ATTRACT;
        g->phase_started = now_ms;
        g->joined = 0;
        g->beat = BEAT_GAP;
    }

    if (!g->prev_coin && in->coin) {
        if (g->credits < CREDIT_MAX) {
            g->credits++;
        }
        push_sound(g, SOUND_COIN);
    }

    if (!g->prev_red && in->red_gun) {
        try_join(g, PLAYER_RED);
    }
    if (!g->prev_blue && in->blue_gun) {
        try_join(g, PLAYER_BLUE);
    }

    if (g->phase == PHASE_JOIN &&
        (now_ms - g->phase_started) >= JOIN_WINDOW_MS) {
        begin_play(g);
    }

    if (g->phase == PHASE_PLAY) {
        if ((now_ms - g->play_started) >= PLAY_TIME_MS) {
            enter_game_over(g);
        } else {
            advance_beats(g);
            if (!g->suppress_shots) {
                if (!g->prev_red && in->red_gun) {
                    fire(g, PLAYER_RED);
                }
                if (!g->prev_blue && in->blue_gun) {
                    fire(g, PLAYER_BLUE);
                }
            }
        }
    } else if (g->phase == PHASE_GAME_OVER) {
        if ((now_ms - g->phase_started) >= GAME_OVER_MS) {
            g->phase = PHASE_ATTRACT;
            g->phase_started = now_ms;
            g->joined = 0;
        }
    }

    g->prev_red = in->red_gun;
    g->prev_blue = in->blue_gun;
    g->prev_coin = in->coin;
    g->prev_test = 0;
    render(g);
}

uint8_t game_phase(const Game *g)
{
    return g == 0 ? 0 : g->phase;
}

uint16_t game_score(const Game *g, uint8_t player)
{
    if (g == 0 || player > PLAYER_BLUE) {
        return 0;
    }
    return g->score[player];
}

uint8_t game_credits(const Game *g)
{
    return g == 0 ? 0 : g->credits;
}

uint8_t game_players(const Game *g)
{
    uint8_t n = 0;
    if (g == 0) {
        return 0;
    }
    if (g->joined & 0x1u) {
        n++;
    }
    if (g->joined & 0x2u) {
        n++;
    }
    return n;
}

uint8_t game_free_game_awarded(const Game *g)
{
    return g == 0 ? 0 : g->free_awarded;
}

uint8_t game_current_target(const Game *g)
{
    if (g == 0 || g->phase != PHASE_PLAY) {
        return TARGET_NONE;
    }
    if (g->beat != BEAT_LIVE && g->beat != BEAT_LATE) {
        return TARGET_NONE;
    }
    return g->target;
}

int game_lamp(const Game *g, LampId id)
{
    if (g == 0) {
        return 0;
    }
    return lamp_bits_get(g->lamps, id);
}

uint8_t game_sound_count(const Game *g)
{
    return g == 0 ? 0 : g->sound_n;
}

SoundId game_sound_at(const Game *g, uint8_t index)
{
    if (g == 0 || index >= g->sound_n) {
        return SOUND_NONE;
    }
    return (SoundId)g->sound_q[index];
}

void game_read_outputs(const Game *g, HardwareOutputs *out)
{
    uint8_t i;

    if (out == 0) {
        return;
    }
    memset(out, 0, sizeof *out);
    if (g == 0) {
        return;
    }
    out->score_red = g->score[PLAYER_RED];
    out->score_blue = g->score[PLAYER_BLUE];
    out->credits = g->credits;
    out->phase = g->phase;
    out->players = game_players(g);
    out->sound_count = g->sound_n;
    for (i = 0; i < g->sound_n && i < SOUND_QUEUE; i++) {
        out->sounds[i] = g->sound_q[i];
    }
    for (i = 0; i < LAMP_BYTES; i++) {
        out->lamps[i] = g->lamps[i];
    }
}
