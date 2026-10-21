#include "game.h"
#include "pins.h"

#include <stdio.h>
#include <string.h>

static int g_failed;
static int g_passed;

static void check(int cond, const char *expr, int line)
{
    if (cond) {
        g_passed++;
        return;
    }
    fprintf(stderr, "FAIL line %d: %s\n", line, expr);
    g_failed++;
}

#define CHECK(cond) check((cond), #cond, __LINE__)

typedef struct Harness {
    Game game;
    GameInputs in;
    uint32_t t;
} Harness;

static void advance(Harness *h, uint32_t dt)
{
    h->t += dt;
    game_tick(&h->game, &h->in, h->t);
}

static void boot(Harness *h, uint32_t seed)
{
    memset(h, 0, sizeof *h);
    game_init(&h->game, seed);
    game_tick(&h->game, &h->in, h->t);
}

static void coin(Harness *h)
{
    h->in.coin = 1;
    advance(h, 1);
    h->in.coin = 0;
    advance(h, 1);
}

static void press(Harness *h, uint8_t *level)
{
    *level = 1;
    advance(h, 1);
}

static void release(Harness *h, uint8_t *level)
{
    *level = 0;
    advance(h, 1);
}

static int walk_live(Harness *h)
{
    int guard = 0;
    while (game_current_target(&h->game) == TARGET_NONE && guard < 2000) {
        advance(h, 1);
        guard++;
    }
    return game_current_target(&h->game) != TARGET_NONE;
}

static void goto_time(Harness *h, uint32_t when)
{
    if (h->t < when) {
        advance(h, when - h->t);
    }
}

static int heard(const Harness *h, SoundId id)
{
    uint8_t i;
    for (i = 0; i < game_sound_count(&h->game); i++) {
        if (game_sound_at(&h->game, i) == id) {
            return 1;
        }
    }
    return 0;
}

static int time_lamps_on(const Harness *h)
{
    int n = 0;
    LampId id;
    for (id = LAMP_TIME_0; id <= LAMP_TIME_7; id = (LampId)(id + 1)) {
        n += game_lamp(&h->game, id);
    }
    return n;
}

static void set_script(Harness *h, const uint8_t *ids, uint8_t n)
{
    game_set_target_script(&h->game, ids, n);
}

static void start_red(Harness *h)
{
    coin(h);
    press(h, &h->in.red_gun);
    release(h, &h->in.red_gun);
}

static void start_two(Harness *h)
{
    coin(h);
    coin(h);
    press(h, &h->in.red_gun);
    release(h, &h->in.red_gun);
    press(h, &h->in.blue_gun);
    release(h, &h->in.blue_gun);
}

static void test_default_free_game_and_state_size(void)
{
    Harness h;
    boot(&h, 1);
    CHECK(game_free_game_score(&h.game) == 50);
    CHECK(game_free_game_score(&h.game) == TOMBSTONE_FREE_GAME_SCORE);
    CHECK(game_phase(&h.game) == PHASE_ATTRACT);
    CHECK(game_credits(&h.game) == 0);
    CHECK(game_players(&h.game) == 0);
    CHECK(sizeof(Game) < 160u);
}

static void test_attract(void)
{
    Harness h;
    boot(&h, 1);
    CHECK(game_lamp(&h.game, LAMP_WINDOW_DRY_GOODS));
    CHECK(!game_lamp(&h.game, LAMP_WINDOW_TONSORIAL));
    CHECK(game_lamp(&h.game, LAMP_HEAT_LIGHTNING));
    CHECK(!game_lamp(&h.game, LAMP_POP_SALOON_A));
    CHECK(!game_lamp(&h.game, LAMP_SCORE_RED));
    CHECK(!game_lamp(&h.game, LAMP_FREE_GAME));

    goto_time(&h, LIGHTNING_ON_MS);
    CHECK(!game_lamp(&h.game, LAMP_HEAT_LIGHTNING));

    goto_time(&h, ATTRACT_WINDOW_MS);
    CHECK(!game_lamp(&h.game, LAMP_WINDOW_DRY_GOODS));
    CHECK(game_lamp(&h.game, LAMP_WINDOW_TONSORIAL));
    CHECK(!game_lamp(&h.game, LAMP_POP_DRY_GOODS_A));
    CHECK(!game_lamp(&h.game, LAMP_ROOF_DRY_GOODS_A));
    CHECK(!game_lamp(&h.game, LAMP_WHITE_0_A));
}

static void test_coin_and_one_player(void)
{
    Harness h;
    uint32_t t_play;
    boot(&h, 1);

    press(&h, &h.in.red_gun);
    CHECK(game_phase(&h.game) == PHASE_ATTRACT);
    CHECK(game_credits(&h.game) == 0);
    release(&h, &h.in.red_gun);

    /* Rising edge credits once. Holding the switch does not repeat. */
    h.in.coin = 1;
    advance(&h, 1);
    CHECK(game_credits(&h.game) == 1);
    CHECK(heard(&h, SOUND_COIN));
    advance(&h, 50);
    CHECK(game_credits(&h.game) == 1);
    CHECK(!heard(&h, SOUND_COIN));
    h.in.coin = 0;
    advance(&h, 1);
    CHECK(game_credits(&h.game) == 1);

    press(&h, &h.in.red_gun);
    t_play = h.t;
    CHECK(game_phase(&h.game) == PHASE_PLAY);
    CHECK(game_players(&h.game) == 1);
    CHECK(game_credits(&h.game) == 0);
    CHECK(game_score(&h.game, PLAYER_RED) == 0);
    CHECK(heard(&h, SOUND_START));
    CHECK(!heard(&h, SOUND_EXPLOSION));
    CHECK(game_lamp(&h.game, LAMP_SCORE_RED));
    CHECK(!game_lamp(&h.game, LAMP_SCORE_BLUE));
    CHECK(time_lamps_on(&h) == TIME_LAMP_COUNT);
    (void)t_play;
    release(&h, &h.in.red_gun);

    /* The other transmitter is not in this game. */
    CHECK(walk_live(&h));
    press(&h, &h.in.blue_gun);
    CHECK(game_score(&h.game, PLAYER_BLUE) == 0);
    CHECK(game_score(&h.game, PLAYER_RED) == 0);
    CHECK(game_players(&h.game) == 1);
    CHECK(!game_lamp(&h.game, LAMP_SHERIFF_BLUE_A));
    release(&h, &h.in.blue_gun);
}

static void test_blue_only_player(void)
{
    Harness h;
    const uint8_t script[] = {TARGET_POP_EATERY};
    boot(&h, 1);
    set_script(&h, script, 1);
    coin(&h);
    press(&h, &h.in.blue_gun);
    CHECK(game_phase(&h.game) == PHASE_PLAY);
    CHECK(game_players(&h.game) == 1);
    CHECK(game_lamp(&h.game, LAMP_SCORE_BLUE));
    CHECK(!game_lamp(&h.game, LAMP_SCORE_RED));
    release(&h, &h.in.blue_gun);
    CHECK(walk_live(&h));
    press(&h, &h.in.blue_gun);
    CHECK(game_score(&h.game, PLAYER_BLUE) == SCORE_FAST_HIT);
    CHECK(game_lamp(&h.game, LAMP_SHERIFF_BLUE_A));
    CHECK(game_lamp(&h.game, LAMP_SHERIFF_BLUE_B));
    CHECK(!game_lamp(&h.game, LAMP_SHERIFF_RED_A));
    release(&h, &h.in.blue_gun);
}

static void test_two_player_flow(void)
{
    Harness h;
    uint32_t t_join;
    boot(&h, 1);

    coin(&h);
    coin(&h);
    press(&h, &h.in.red_gun);
    t_join = h.t;
    CHECK(game_phase(&h.game) == PHASE_JOIN);
    CHECK(game_players(&h.game) == 1);
    CHECK(game_credits(&h.game) == 1);
    goto_time(&h, t_join + JOIN_WINDOW_MS - 1u);
    CHECK(game_phase(&h.game) == PHASE_JOIN);
    goto_time(&h, t_join + JOIN_WINDOW_MS);
    CHECK(game_phase(&h.game) == PHASE_PLAY);
    CHECK(game_players(&h.game) == 1);
    CHECK(game_credits(&h.game) == 1);
    release(&h, &h.in.red_gun);

    boot(&h, 1);
    coin(&h);
    coin(&h);
    press(&h, &h.in.red_gun);
    release(&h, &h.in.red_gun);
    CHECK(game_phase(&h.game) == PHASE_JOIN);
    press(&h, &h.in.blue_gun);
    CHECK(game_phase(&h.game) == PHASE_PLAY);
    CHECK(game_players(&h.game) == 2);
    CHECK(game_credits(&h.game) == 0);
    CHECK(game_lamp(&h.game, LAMP_SCORE_RED));
    CHECK(game_lamp(&h.game, LAMP_SCORE_BLUE));
    release(&h, &h.in.blue_gun);
}

static void test_fast_hit_and_pair_lamps(void)
{
    Harness h;
    const uint8_t script[] = {TARGET_POP_SALOON};
    HardwareOutputs out;
    boot(&h, 1);
    set_script(&h, script, 1);
    start_red(&h);
    CHECK(walk_live(&h));
    CHECK(game_current_target(&h.game) == TARGET_POP_SALOON);
    CHECK(game_lamp(&h.game, LAMP_POP_SALOON_A));
    CHECK(game_lamp(&h.game, LAMP_POP_SALOON_B));
    CHECK(!game_lamp(&h.game, LAMP_POP_EATERY_A));
    CHECK(!game_lamp(&h.game, LAMP_TOMBSTONE_0_A));

    press(&h, &h.in.red_gun);
    CHECK(game_score(&h.game, PLAYER_RED) == SCORE_FAST_HIT);
    CHECK(heard(&h, SOUND_THUNDER));
    CHECK(heard(&h, SOUND_EXPLOSION));
    CHECK(!heard(&h, SOUND_RETURN_FIRE));
    CHECK(game_lamp(&h.game, LAMP_TOMBSTONE_0_A));
    CHECK(game_lamp(&h.game, LAMP_TOMBSTONE_0_B));
    CHECK(game_lamp(&h.game, LAMP_TOMBSTONE_1_A));
    CHECK(game_lamp(&h.game, LAMP_TOMBSTONE_1_B));
    CHECK(game_lamp(&h.game, LAMP_HEAT_LIGHTNING));
    CHECK(game_lamp(&h.game, LAMP_SHERIFF_RED_A));
    CHECK(game_lamp(&h.game, LAMP_SHERIFF_RED_B));
    CHECK(!game_lamp(&h.game, LAMP_SHERIFF_BLUE_A));
    game_read_outputs(&h.game, &out);
    CHECK(out.score_red == SCORE_FAST_HIT);
    CHECK(out.players == 1);
    CHECK(lamp_bits_get(out.lamps, LAMP_TOMBSTONE_0_A));
    release(&h, &h.in.red_gun);

    goto_time(&h, h.t + TOMBSTONE_MS);
    CHECK(!game_lamp(&h.game, LAMP_TOMBSTONE_0_A));
    CHECK(!game_lamp(&h.game, LAMP_HEAT_LIGHTNING));
}

static void test_faster_shooter(void)
{
    Harness h;
    const uint8_t script[] = {TARGET_POP_ASSAY};
    boot(&h, 1);
    set_script(&h, script, 1);
    start_two(&h);
    CHECK(walk_live(&h));

    press(&h, &h.in.blue_gun);
    CHECK(game_score(&h.game, PLAYER_BLUE) == SCORE_FAST_HIT);
    CHECK(game_score(&h.game, PLAYER_RED) == 0);
    release(&h, &h.in.blue_gun);
    press(&h, &h.in.red_gun);
    CHECK(game_score(&h.game, PLAYER_RED) == 0);
    CHECK(game_score(&h.game, PLAYER_BLUE) == SCORE_FAST_HIT);
    release(&h, &h.in.red_gun);

    boot(&h, 1);
    set_script(&h, script, 1);
    start_two(&h);
    CHECK(walk_live(&h));
    h.in.red_gun = 1;
    h.in.blue_gun = 1;
    advance(&h, 1);
    CHECK(game_score(&h.game, PLAYER_RED) == SCORE_FAST_HIT);
    CHECK(game_score(&h.game, PLAYER_BLUE) == 0);
    CHECK(game_lamp(&h.game, LAMP_SHERIFF_RED_A));
    CHECK(game_lamp(&h.game, LAMP_SHERIFF_BLUE_A));
}

static void test_slow_shot_and_white_hat(void)
{
    Harness h;
    const uint8_t slow_script[] = {TARGET_POP_DRY_GOODS};
    const uint8_t white_script[] = {TARGET_WHITE_2};
    const uint8_t mixed[] = {TARGET_POP_TONSORIAL, TARGET_WHITE_0};
    uint32_t t_live;

    boot(&h, 1);
    set_script(&h, slow_script, 1);
    start_red(&h);
    CHECK(walk_live(&h));
    t_live = h.t;
    goto_time(&h, t_live + FAST_WINDOW_MS);
    CHECK(game_current_target(&h.game) == TARGET_POP_DRY_GOODS);
    press(&h, &h.in.red_gun);
    CHECK(game_score(&h.game, PLAYER_RED) == 0);
    CHECK(heard(&h, SOUND_RETURN_FIRE));
    CHECK(heard(&h, SOUND_EXPLOSION));
    CHECK(!heard(&h, SOUND_THUNDER));
    CHECK(game_lamp(&h.game, LAMP_LATE_RED));
    CHECK(!game_lamp(&h.game, LAMP_LATE_BLUE));
    CHECK(!game_lamp(&h.game, LAMP_TOMBSTONE_0_A));
    release(&h, &h.in.red_gun);

    /* Waiting out the return-fire window is not itself a penalty. */
    boot(&h, 1);
    set_script(&h, slow_script, 1);
    start_red(&h);
    CHECK(walk_live(&h));
    t_live = h.t;
    goto_time(&h, t_live + FAST_WINDOW_MS + SLOW_WINDOW_MS + 1u);
    CHECK(game_score(&h.game, PLAYER_RED) == 0);
    CHECK(game_current_target(&h.game) != TARGET_POP_DRY_GOODS ||
          game_phase(&h.game) == PHASE_PLAY);

    boot(&h, 1);
    set_script(&h, white_script, 1);
    start_red(&h);
    CHECK(walk_live(&h));
    CHECK(game_lamp(&h.game, LAMP_WHITE_2_A));
    CHECK(game_lamp(&h.game, LAMP_WHITE_2_B));
    press(&h, &h.in.red_gun);
    CHECK(game_score(&h.game, PLAYER_RED) == 0);
    CHECK(heard(&h, SOUND_WHITE_HAT));
    CHECK(!heard(&h, SOUND_THUNDER));
    CHECK(!game_lamp(&h.game, LAMP_TOMBSTONE_0_A));
    release(&h, &h.in.red_gun);

    /* Ignoring a white hat does not cost points. */
    boot(&h, 1);
    set_script(&h, white_script, 1);
    start_red(&h);
    CHECK(walk_live(&h));
    t_live = h.t;
    goto_time(&h, t_live + FAST_WINDOW_MS + 1u);
    CHECK(game_score(&h.game, PLAYER_RED) == 0);

    boot(&h, 1);
    set_script(&h, mixed, 2);
    start_red(&h);
    CHECK(walk_live(&h));
    press(&h, &h.in.red_gun);
    CHECK(game_score(&h.game, PLAYER_RED) == SCORE_FAST_HIT);
    release(&h, &h.in.red_gun);
    CHECK(walk_live(&h));
    press(&h, &h.in.red_gun);
    CHECK(game_score(&h.game, PLAYER_RED) == SCORE_FAST_HIT - SCORE_PENALTY);
    CHECK(heard(&h, SOUND_WHITE_HAT));
    release(&h, &h.in.red_gun);
}

static void test_roof_run(void)
{
    Harness h;
    const uint8_t script[] = {TARGET_ROOF};
    uint32_t t_live;
    boot(&h, 1);
    set_script(&h, script, 1);
    start_red(&h);
    CHECK(walk_live(&h));
    t_live = h.t;
    CHECK(game_lamp(&h.game, LAMP_ROOF_DRY_GOODS_A));
    CHECK(game_lamp(&h.game, LAMP_ROOF_DRY_GOODS_B));
    CHECK(!game_lamp(&h.game, LAMP_ROOF_TONSORIAL_A));

    goto_time(&h, t_live + FAST_WINDOW_MS);
    CHECK(game_lamp(&h.game, LAMP_ROOF_TONSORIAL_A));
    CHECK(game_lamp(&h.game, LAMP_ROOF_TONSORIAL_B));
    CHECK(!game_lamp(&h.game, LAMP_ROOF_DRY_GOODS_A));
    press(&h, &h.in.red_gun);
    CHECK(game_score(&h.game, PLAYER_RED) == SCORE_FAST_HIT);
    CHECK(heard(&h, SOUND_THUNDER));
    CHECK(game_lamp(&h.game, LAMP_TOMBSTONE_1_B));
    release(&h, &h.in.red_gun);

    boot(&h, 1);
    set_script(&h, script, 1);
    start_red(&h);
    CHECK(walk_live(&h));
    t_live = h.t;
    goto_time(&h, t_live + (uint32_t)ROOF_STEPS * FAST_WINDOW_MS);
    press(&h, &h.in.red_gun);
    CHECK(game_score(&h.game, PLAYER_RED) == 0);
    CHECK(heard(&h, SOUND_RETURN_FIRE));
    CHECK(!heard(&h, SOUND_THUNDER));
    CHECK(game_lamp(&h.game, LAMP_LATE_RED));
}

static void test_timer(void)
{
    Harness h;
    uint32_t t_play;
    int i;
    boot(&h, 1);
    coin(&h);
    press(&h, &h.in.red_gun);
    t_play = h.t;
    CHECK(time_lamps_on(&h) == 8);
    CHECK(game_lamp(&h.game, LAMP_TIME_0));
    CHECK(game_lamp(&h.game, LAMP_TIME_7));

    goto_time(&h, t_play + BLINK_HALF_MS);
    CHECK(!game_lamp(&h.game, LAMP_TIME_7));
    CHECK(game_lamp(&h.game, LAMP_TIME_0));
    CHECK(game_lamp(&h.game, LAMP_TIME_6));
    CHECK(time_lamps_on(&h) == 7);

    goto_time(&h, t_play + TIME_LAMP_MS);
    CHECK(!game_lamp(&h.game, LAMP_TIME_7));
    CHECK(game_lamp(&h.game, LAMP_TIME_6));
    CHECK(time_lamps_on(&h) == 7);

    goto_time(&h, t_play + TIME_LAMP_MS + BLINK_HALF_MS);
    CHECK(!game_lamp(&h.game, LAMP_TIME_6));
    CHECK(game_lamp(&h.game, LAMP_TIME_5));
    CHECK(time_lamps_on(&h) == 6);

    for (i = 2; i < TIME_LAMP_COUNT; i++) {
        goto_time(&h, t_play + (uint32_t)i * TIME_LAMP_MS);
        CHECK(game_phase(&h.game) == PHASE_PLAY);
        /* Each exact lamp boundary is an even blink phase, so the edge lamp is lit. */
        CHECK(time_lamps_on(&h) == TIME_LAMP_COUNT - i);
    }

    goto_time(&h, t_play + PLAY_TIME_MS - 1u);
    CHECK(game_phase(&h.game) == PHASE_PLAY);
    goto_time(&h, t_play + PLAY_TIME_MS);
    CHECK(game_phase(&h.game) == PHASE_GAME_OVER);
    CHECK(heard(&h, SOUND_GAME_OVER));
    CHECK(time_lamps_on(&h) == 0);
    CHECK(game_score(&h.game, PLAYER_RED) == 0);
    CHECK(game_lamp(&h.game, LAMP_SCORE_RED));

    goto_time(&h, h.t + GAME_OVER_MS - 1u);
    CHECK(game_phase(&h.game) == PHASE_GAME_OVER);
    goto_time(&h, h.t + 1u);
    CHECK(game_phase(&h.game) == PHASE_ATTRACT);
    CHECK(!game_lamp(&h.game, LAMP_SCORE_RED));
    CHECK(game_lamp(&h.game, LAMP_WINDOW_DRY_GOODS) ||
          game_lamp(&h.game, LAMP_WINDOW_TONSORIAL) ||
          game_lamp(&h.game, LAMP_WINDOW_EATERY) ||
          game_lamp(&h.game, LAMP_WINDOW_SALOON) ||
          game_lamp(&h.game, LAMP_WINDOW_ASSAY) ||
          game_lamp(&h.game, LAMP_WINDOW_STAGECOACH));
}

static void test_free_game(void)
{
    Harness h;
    const uint8_t pops[] = {TARGET_POP_SALOON};
    const uint8_t alt[] = {TARGET_POP_DRY_GOODS};
    const uint8_t white[] = {TARGET_WHITE_1};
    int i;

    boot(&h, 1);
    CHECK(game_free_game_score(&h.game) == 50);
    set_script(&h, pops, 1);
    start_red(&h);
    for (i = 0; i < 4; i++) {
        CHECK(walk_live(&h));
        press(&h, &h.in.red_gun);
        release(&h, &h.in.red_gun);
    }
    CHECK(game_score(&h.game, PLAYER_RED) == 40);
    CHECK(!game_free_game_awarded(&h.game));
    CHECK(game_credits(&h.game) == 0);
    CHECK(!game_lamp(&h.game, LAMP_FREE_GAME));

    CHECK(walk_live(&h));
    press(&h, &h.in.red_gun);
    CHECK(game_score(&h.game, PLAYER_RED) == 50);
    CHECK(game_free_game_awarded(&h.game));
    CHECK(game_credits(&h.game) == 1);
    CHECK(game_lamp(&h.game, LAMP_FREE_GAME));
    CHECK(heard(&h, SOUND_FREE_GAME));
    CHECK(game_players(&h.game) == 1);
    release(&h, &h.in.red_gun);

    /* A later penalty does not take the award back. */
    game_set_target_script(&h.game, white, 1);
    CHECK(walk_live(&h));
    press(&h, &h.in.red_gun);
    CHECK(game_score(&h.game, PLAYER_RED) == 45);
    CHECK(game_free_game_awarded(&h.game));
    CHECK(game_credits(&h.game) == 1);
    release(&h, &h.in.red_gun);

    /* One award per play, even past the threshold. */
    game_set_target_script(&h.game, pops, 1);
    CHECK(walk_live(&h));
    press(&h, &h.in.red_gun);
    CHECK(game_score(&h.game, PLAYER_RED) == 55);
    CHECK(game_credits(&h.game) == 1);
    release(&h, &h.in.red_gun);

    goto_time(&h, h.game.play_started + PLAY_TIME_MS);
    CHECK(game_phase(&h.game) == PHASE_GAME_OVER);
    CHECK(game_lamp(&h.game, LAMP_FREE_GAME));
    CHECK(game_credits(&h.game) == 1);
    goto_time(&h, h.t + GAME_OVER_MS);
    CHECK(game_phase(&h.game) == PHASE_ATTRACT);
    CHECK(game_lamp(&h.game, LAMP_FREE_GAME));
    CHECK(game_score(&h.game, PLAYER_RED) == 55);

    /* The free credit starts the next game and clears the lamp. */
    press(&h, &h.in.red_gun);
    CHECK(game_phase(&h.game) == PHASE_PLAY);
    CHECK(game_credits(&h.game) == 0);
    CHECK(!game_free_game_awarded(&h.game));
    CHECK(!game_lamp(&h.game, LAMP_FREE_GAME));
    CHECK(game_score(&h.game, PLAYER_RED) == 0);
    release(&h, &h.in.red_gun);

    /* Operator threshold, including "off". */
    boot(&h, 1);
    game_set_free_game_score(&h.game, 0);
    set_script(&h, pops, 1);
    start_red(&h);
    for (i = 0; i < 5; i++) {
        CHECK(walk_live(&h));
        press(&h, &h.in.red_gun);
        release(&h, &h.in.red_gun);
    }
    CHECK(game_score(&h.game, PLAYER_RED) == 50);
    CHECK(!game_free_game_awarded(&h.game));

    boot(&h, 1);
    game_set_free_game_score(&h.game, 100);
    set_script(&h, pops, 1);
    start_red(&h);
    for (i = 0; i < 5; i++) {
        CHECK(walk_live(&h));
        press(&h, &h.in.red_gun);
        release(&h, &h.in.red_gun);
    }
    CHECK(game_score(&h.game, PLAYER_RED) == 50);
    CHECK(!game_free_game_awarded(&h.game));

    boot(&h, 1);
    game_set_free_game_score(&h.game, 30);
    set_script(&h, alt, 1);
    start_red(&h);
    for (i = 0; i < 3; i++) {
        CHECK(walk_live(&h));
        press(&h, &h.in.red_gun);
        release(&h, &h.in.red_gun);
    }
    CHECK(game_score(&h.game, PLAYER_RED) == 30);
    CHECK(game_free_game_awarded(&h.game));
    CHECK(game_free_game_score(&h.game) == 30);
}

static void test_combined_free_game(void)
{
    Harness h;
    const uint8_t script[] = {TARGET_POP_EATERY};
    int i;
    boot(&h, 1);
    set_script(&h, script, 1);
    start_two(&h);
    for (i = 0; i < 2; i++) {
        CHECK(walk_live(&h));
        press(&h, &h.in.red_gun);
        CHECK(game_score(&h.game, PLAYER_RED) == (uint16_t)((i + 1) * SCORE_FAST_HIT));
        release(&h, &h.in.red_gun);
    }
    CHECK(!game_free_game_awarded(&h.game));
    for (i = 0; i < 3; i++) {
        CHECK(walk_live(&h));
        press(&h, &h.in.blue_gun);
        release(&h, &h.in.blue_gun);
    }
    CHECK(game_score(&h.game, PLAYER_RED) == 20);
    CHECK(game_score(&h.game, PLAYER_BLUE) == 30);
    CHECK(game_free_game_awarded(&h.game));
    CHECK(game_credits(&h.game) == 1);
    CHECK(game_players(&h.game) == 2);
}

static void test_gap_miss_and_banked_coin(void)
{
    Harness h;
    const uint8_t script[] = {TARGET_POP_SALOON};
    boot(&h, 1);
    set_script(&h, script, 1);
    start_red(&h);
    /* Still in the opening gap: a shot flashes the Colt and does not score. */
    press(&h, &h.in.red_gun);
    CHECK(game_current_target(&h.game) == TARGET_NONE);
    CHECK(game_score(&h.game, PLAYER_RED) == 0);
    CHECK(heard(&h, SOUND_EXPLOSION));
    CHECK(!heard(&h, SOUND_THUNDER));
    CHECK(game_lamp(&h.game, LAMP_SHERIFF_RED_A));
    release(&h, &h.in.red_gun);

    CHECK(walk_live(&h));
    coin(&h);
    CHECK(game_credits(&h.game) == 1);
    CHECK(game_players(&h.game) == 1);
    press(&h, &h.in.red_gun);
    CHECK(game_score(&h.game, PLAYER_RED) == SCORE_FAST_HIT);
    release(&h, &h.in.red_gun);
}

static void test_credit_cap(void)
{
    Harness h;
    int i;
    boot(&h, 1);
    for (i = 0; i < 12; i++) {
        coin(&h);
    }
    CHECK(game_credits(&h.game) == CREDIT_MAX);
}

static void test_test_switch(void)
{
    Harness h;
    const uint8_t script[] = {TARGET_POP_SALOON};
    boot(&h, 1);
    set_script(&h, script, 1);
    start_red(&h);
    CHECK(walk_live(&h));
    CHECK(game_lamp(&h.game, LAMP_POP_SALOON_A));

    h.in.test_mode = 1;
    h.in.red_gun = 1;
    advance(&h, 1);
    CHECK(game_phase(&h.game) == PHASE_TEST);
    CHECK(game_score(&h.game, PLAYER_RED) == 0);
    CHECK(game_lamp(&h.game, LAMP_TIME_0));
    CHECK(!game_lamp(&h.game, LAMP_TIME_1));
    CHECK(!game_lamp(&h.game, LAMP_POP_SALOON_A));

    goto_time(&h, h.t + TEST_STEP_MS);
    CHECK(game_phase(&h.game) == PHASE_TEST);
    CHECK(!game_lamp(&h.game, LAMP_TIME_0));
    CHECK(game_lamp(&h.game, LAMP_TIME_1));
    CHECK(game_score(&h.game, PLAYER_RED) == 0);

    goto_time(&h, h.game.phase_started + (uint32_t)LAMP_COUNT * TEST_STEP_MS);
    CHECK(game_lamp(&h.game, LAMP_TIME_0));
    CHECK(!game_lamp(&h.game, LAMP_TIME_1));

    h.in.test_mode = 0;
    h.in.red_gun = 0;
    advance(&h, 1);
    CHECK(game_phase(&h.game) == PHASE_ATTRACT);
    CHECK(!game_lamp(&h.game, LAMP_POP_SALOON_A));
    CHECK(game_players(&h.game) == 0);
}

static void test_same_seed_same_target(void)
{
    Harness a;
    Harness b;
    LampId id;
    boot(&a, 99);
    boot(&b, 99);
    start_red(&a);
    start_red(&b);
    CHECK(walk_live(&a));
    CHECK(walk_live(&b));
    CHECK(game_current_target(&a.game) == game_current_target(&b.game));
    for (id = (LampId)0; id < LAMP_COUNT; id = (LampId)(id + 1)) {
        CHECK(game_lamp(&a.game, id) == game_lamp(&b.game, id));
    }
}

static void test_lamp_roles_and_pin_map(void)
{
    uint8_t seen_role[ROLE_COUNT];
    uint8_t seen_pin[20];
    uint8_t i;
    int outputs = 0;
    int inputs = 0;

    CHECK(LAMP_HEAT_LIGHTNING + 1 == LAMP_COUNT);
    CHECK(lamp_id_inferred(LAMP_TIME_0));
    CHECK(lamp_id_inferred(LAMP_TIME_7));
    CHECK(lamp_id_inferred(LAMP_POP_DRY_GOODS_A));
    CHECK(lamp_id_inferred(LAMP_POP_ASSAY_B));
    CHECK(lamp_id_inferred(LAMP_ROOF_SALOON_A));
    CHECK(lamp_id_inferred(LAMP_WHITE_0_A));
    CHECK(lamp_id_inferred(LAMP_WHITE_3_B));
    CHECK(lamp_id_inferred(LAMP_TOMBSTONE_0_A));
    CHECK(lamp_id_inferred(LAMP_WINDOW_DRY_GOODS));
    CHECK(lamp_id_inferred(LAMP_WINDOW_STAGECOACH));
    CHECK(lamp_id_inferred(LAMP_HEAT_LIGHTNING));
    CHECK(!lamp_id_inferred(LAMP_SHERIFF_RED_A));
    CHECK(!lamp_id_inferred(LAMP_SHERIFF_BLUE_B));
    CHECK(!lamp_id_inferred(LAMP_SCORE_RED));
    CHECK(!lamp_id_inferred(LAMP_SCORE_BLUE));
    CHECK(!lamp_id_inferred(LAMP_LATE_RED));
    CHECK(!lamp_id_inferred(LAMP_LATE_BLUE));
    CHECK(!lamp_id_inferred(LAMP_FREE_GAME));

    CHECK(PIN_MAP_UNCONFIRMED == 1);
    CHECK(strcmp(kPinMapStatus, "UNCONFIRMED") == 0);
    CHECK(ROLE_COUNT < LAMP_COUNT);

    memset(seen_role, 0, sizeof seen_role);
    memset(seen_pin, 0, sizeof seen_pin);
    CHECK(sizeof kPinMap / sizeof kPinMap[0] == ROLE_COUNT);
    for (i = 0; i < ROLE_COUNT; i++) {
        uint8_t pin = kPinMap[i].nano_pin;
        CHECK(kPinMap[i].role == i);
        CHECK(kPinMap[i].role < ROLE_COUNT);
        CHECK(!seen_role[kPinMap[i].role]);
        seen_role[kPinMap[i].role] = 1;
        CHECK(pin < 20u);
        CHECK(pin != 0u); /* serial */
        CHECK(pin != 1u);
        CHECK(pin != 13u); /* onboard LED left free */
        CHECK(!seen_pin[pin]);
        seen_pin[pin] = 1;
        if (kPinMap[i].output) {
            outputs++;
            CHECK(kPinMap[i].active_low == 0);
        } else {
            inputs++;
            CHECK(kPinMap[i].active_low == 1);
        }
    }
    CHECK(inputs == 4);
    CHECK(outputs == 8);
}

int main(void)
{
    test_default_free_game_and_state_size();
    test_attract();
    test_coin_and_one_player();
    test_blue_only_player();
    test_two_player_flow();
    test_fast_hit_and_pair_lamps();
    test_faster_shooter();
    test_slow_shot_and_white_hat();
    test_roof_run();
    test_timer();
    test_free_game();
    test_combined_free_game();
    test_gap_miss_and_banked_coin();
    test_credit_cap();
    test_test_switch();
    test_same_seed_same_target();
    test_lamp_roles_and_pin_map();

    if (g_failed) {
        fprintf(stderr, "%d failed, %d passed\n", g_failed, g_passed);
        return 1;
    }
    printf("ok %d checks\n", g_passed);
    return 0;
}
