#define _POSIX_C_SOURCE 200809L

/*
 * Host playfield for the Tombstone rules in game.c.
 * Same scoring, timer, and lamp IDs as the Nano sketch.
 * The picture is the flyer layout with inferred positions.
 * It is not a measured panel and not Sunbird wiring.
 */

#include "game.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

static const LampId kPopA[5] = {
    LAMP_POP_DRY_GOODS_A, LAMP_POP_TONSORIAL_A, LAMP_POP_EATERY_A,
    LAMP_POP_SALOON_A, LAMP_POP_ASSAY_A};
static const LampId kPopB[5] = {
    LAMP_POP_DRY_GOODS_B, LAMP_POP_TONSORIAL_B, LAMP_POP_EATERY_B,
    LAMP_POP_SALOON_B, LAMP_POP_ASSAY_B};
static const LampId kRoofA[5] = {
    LAMP_ROOF_DRY_GOODS_A, LAMP_ROOF_TONSORIAL_A, LAMP_ROOF_EATERY_A,
    LAMP_ROOF_SALOON_A, LAMP_ROOF_ASSAY_A};
static const LampId kRoofB[5] = {
    LAMP_ROOF_DRY_GOODS_B, LAMP_ROOF_TONSORIAL_B, LAMP_ROOF_EATERY_B,
    LAMP_ROOF_SALOON_B, LAMP_ROOF_ASSAY_B};
static const LampId kWhiteA[4] = {
    LAMP_WHITE_0_A, LAMP_WHITE_1_A, LAMP_WHITE_2_A, LAMP_WHITE_3_A};
static const LampId kWhiteB[4] = {
    LAMP_WHITE_0_B, LAMP_WHITE_1_B, LAMP_WHITE_2_B, LAMP_WHITE_3_B};
static const LampId kWin[5] = {
    LAMP_WINDOW_DRY_GOODS, LAMP_WINDOW_TONSORIAL, LAMP_WINDOW_EATERY,
    LAMP_WINDOW_SALOON, LAMP_WINDOW_ASSAY};
static const char *kStore[5] = {
    "Dry Goods", "Tonsorial", "Eatery", "Saloon", "Assay"};

static struct termios g_saved_term;
static int g_term_raw;

static void restore_term(void)
{
    if (g_term_raw) {
        tcsetattr(STDIN_FILENO, TCSANOW, &g_saved_term);
        fputs("\033[?25h\033[0m\n", stdout);
        fflush(stdout);
        g_term_raw = 0;
    }
}

static uint64_t mono_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ull + (uint64_t)ts.tv_nsec / 1000000ull;
}

static const char *phase_name(uint8_t phase)
{
    switch (phase) {
    case PHASE_ATTRACT:
        return "ATTRACT";
    case PHASE_JOIN:
        return "JOIN";
    case PHASE_PLAY:
        return "PLAY";
    case PHASE_GAME_OVER:
        return "GAME OVER";
    case PHASE_TEST:
        return "LAMP TEST";
    default:
        return "?";
    }
}

static const char *sound_name(SoundId id)
{
    switch (id) {
    case SOUND_COIN:
        return "coin";
    case SOUND_START:
        return "start";
    case SOUND_EXPLOSION:
        return "colt";
    case SOUND_THUNDER:
        return "thunder";
    case SOUND_RETURN_FIRE:
        return "return fire";
    case SOUND_WHITE_HAT:
        return "white hat";
    case SOUND_FREE_GAME:
        return "free game";
    case SOUND_GAME_OVER:
        return "game over";
    default:
        return "";
    }
}

static void format_sounds(const Game *g, char *dst, size_t n)
{
    uint8_t i;
    size_t used = 0;
    dst[0] = '\0';
    for (i = 0; i < game_sound_count(g); i++) {
        const char *name = sound_name(game_sound_at(g, i));
        int wrote;
        if (name[0] == '\0') {
            continue;
        }
        wrote = snprintf(dst + used, n - used, "%s%s", used ? ", " : "", name);
        if (wrote < 0 || (size_t)wrote >= n - used) {
            break;
        }
        used += (size_t)wrote;
    }
}

static void pair_cell(const Game *g, LampId a, LampId b, char out[3])
{
    out[0] = game_lamp(g, a) ? '#' : '.';
    out[1] = game_lamp(g, b) ? '#' : '.';
    out[2] = '\0';
}

static void draw(const Game *g, const char *sound, unsigned scale, int color)
{
    int i;
    char timebar[TIME_LAMP_COUNT + 1];
    const char *red = color ? "\033[31m" : "";
    const char *blue = color ? "\033[34m" : "";
    const char *yellow = color ? "\033[33m" : "";
    const char *dim = color ? "\033[2m" : "";
    const char *off = color ? "\033[0m" : "";

    for (i = 0; i < TIME_LAMP_COUNT; i++) {
        /* Lamp 7 is the first to blink off, so it sits at the left. */
        timebar[i] = game_lamp(g, (LampId)(LAMP_TIME_7 - i)) ? '#' : '.';
    }
    timebar[TIME_LAMP_COUNT] = '\0';

    fputs("\033[H", stdout);
    printf("%sTOMBSTONE%s  host demo of the Nano rules\n", yellow, off);
    printf("%sInferred flyer layout. Not a measured panel or Sunbird wiring.%s\n",
           dim, off);
    printf("phase %-10s  credits %u  players %u  free-game %u%s  clock 1/%u\n",
           phase_name(game_phase(g)),
           (unsigned)game_credits(g),
           (unsigned)game_players(g),
           (unsigned)game_free_game_score(g),
           game_free_game_awarded(g) ? "  LAMP ON" : "",
           scale);
    printf("%stime%s %s%s%s\n", yellow, off, yellow, timebar, off);
    printf("sky  heat %c    tomb %c%c %c%c    stagecoach %c    free %c\n",
           game_lamp(g, LAMP_HEAT_LIGHTNING) ? '*' : '.',
           game_lamp(g, LAMP_TOMBSTONE_0_A) ? '#' : '.',
           game_lamp(g, LAMP_TOMBSTONE_0_B) ? '#' : '.',
           game_lamp(g, LAMP_TOMBSTONE_1_A) ? '#' : '.',
           game_lamp(g, LAMP_TOMBSTONE_1_B) ? '#' : '.',
           game_lamp(g, LAMP_WINDOW_STAGECOACH) ? '#' : '.',
           game_lamp(g, LAMP_FREE_GAME) ? '#' : '.');
    fputs("\n", stdout);
    fputs("         ", stdout);
    for (i = 0; i < 5; i++) {
        printf(" %-10s", kStore[i]);
    }
    fputs("\nwindow   ", stdout);
    for (i = 0; i < 5; i++) {
        printf("     %c      ", game_lamp(g, kWin[i]) ? '#' : '.');
    }
    fputs("\nroof     ", stdout);
    for (i = 0; i < 5; i++) {
        char cell[3];
        pair_cell(g, kRoofA[i], kRoofB[i], cell);
        printf("    %s      ", cell);
    }
    fputs("\npop      ", stdout);
    for (i = 0; i < 5; i++) {
        char cell[3];
        pair_cell(g, kPopA[i], kPopB[i], cell);
        printf("    %s      ", cell);
    }
    fputs("\nwhite    ", stdout);
    for (i = 0; i < 4; i++) {
        char cell[3];
        pair_cell(g, kWhiteA[i], kWhiteB[i], cell);
        printf("    %s      ", cell);
    }
    fputs("\n\n", stdout);
    printf("%sRED%s   sheriff %c%c   score %3u   late %c\n",
           red, off,
           game_lamp(g, LAMP_SHERIFF_RED_A) ? '#' : '.',
           game_lamp(g, LAMP_SHERIFF_RED_B) ? '#' : '.',
           (unsigned)game_score(g, PLAYER_RED),
           game_lamp(g, LAMP_LATE_RED) ? '#' : '.');
    printf("%sBLUE%s  sheriff %c%c   score %3u   late %c\n",
           blue, off,
           game_lamp(g, LAMP_SHERIFF_BLUE_A) ? '#' : '.',
           game_lamp(g, LAMP_SHERIFF_BLUE_B) ? '#' : '.',
           (unsigned)game_score(g, PLAYER_BLUE),
           game_lamp(g, LAMP_LATE_BLUE) ? '#' : '.');
    printf("sound  %s\n", sound[0] ? sound : "(quiet)");
    printf("\nC coin    R red shot    B blue shot    T test/play\n");
    printf("[ ] free-game threshold    Q quit\n");
    printf("%sShoot a lit black hat while it is steady. A blinking hat is return fire.%s\n",
           dim, off);
    fflush(stdout);
}

static void step_game(Game *g, GameInputs *in, uint32_t now, char *sound, size_t nsound,
                      uint32_t *sound_until)
{
    char fresh[64];
    game_tick(g, in, now);
    format_sounds(g, fresh, sizeof fresh);
    if (fresh[0] != '\0') {
        snprintf(sound, nsound, "%s", fresh);
        *sound_until = now + 700u;
    } else if (*sound_until == 0 || (int32_t)(*sound_until - now) <= 0) {
        sound[0] = '\0';
    }
}

static int run_check(void)
{
    Game g;
    GameInputs in;
    uint32_t now;
    char sound[64];
    uint32_t sound_until;
    const uint8_t saloon[] = {TARGET_POP_SALOON};
    const uint8_t white[] = {TARGET_WHITE_1};
    const uint8_t pops[] = {TARGET_POP_DRY_GOODS};
    int i;
    int guard;

    memset(&in, 0, sizeof in);
    sound[0] = '\0';
    sound_until = 0;
    game_init(&g, 1);
    now = 0;
    step_game(&g, &in, now, sound, sizeof sound, &sound_until);
    if (game_phase(&g) != PHASE_ATTRACT ||
        !game_lamp(&g, LAMP_WINDOW_DRY_GOODS) ||
        !game_lamp(&g, LAMP_HEAT_LIGHTNING)) {
        fprintf(stderr, "demo check: attract panel is wrong\n");
        return 1;
    }

    in.red_gun = 1;
    now += 1;
    step_game(&g, &in, now, sound, sizeof sound, &sound_until);
    in.red_gun = 0;
    now += 1;
    step_game(&g, &in, now, sound, sizeof sound, &sound_until);
    if (game_phase(&g) != PHASE_ATTRACT || game_credits(&g) != 0) {
        fprintf(stderr, "demo check: a shot started a game with no coin\n");
        return 1;
    }

    game_set_target_script(&g, saloon, 1);
    in.coin = 1;
    now += 1;
    step_game(&g, &in, now, sound, sizeof sound, &sound_until);
    in.coin = 0;
    now += 1;
    step_game(&g, &in, now, sound, sizeof sound, &sound_until);
    in.red_gun = 1;
    now += 1;
    step_game(&g, &in, now, sound, sizeof sound, &sound_until);
    in.red_gun = 0;
    now += 1;
    step_game(&g, &in, now, sound, sizeof sound, &sound_until);
    if (game_phase(&g) != PHASE_PLAY || game_players(&g) != 1) {
        fprintf(stderr, "demo check: one-player start failed\n");
        return 1;
    }

    guard = 0;
    while (game_current_target(&g) == TARGET_NONE && guard < 2000) {
        now += 1;
        step_game(&g, &in, now, sound, sizeof sound, &sound_until);
        guard++;
    }
    if (!game_lamp(&g, LAMP_POP_SALOON_A) || !game_lamp(&g, LAMP_POP_SALOON_B)) {
        fprintf(stderr, "demo check: saloon black hat did not light\n");
        return 1;
    }
    in.red_gun = 1;
    now += 1;
    step_game(&g, &in, now, sound, sizeof sound, &sound_until);
    if (game_score(&g, PLAYER_RED) != SCORE_FAST_HIT ||
        !game_lamp(&g, LAMP_TOMBSTONE_0_A) ||
        !game_lamp(&g, LAMP_SHERIFF_RED_A) ||
        strstr(sound, "thunder") == NULL) {
        fprintf(stderr, "demo check: fast hit did not score\n");
        return 1;
    }
    in.red_gun = 0;

    /* Late shot on the next saloon hat. */
    guard = 0;
    while (game_current_target(&g) == TARGET_NONE && guard < 2000) {
        now += 1;
        step_game(&g, &in, now, sound, sizeof sound, &sound_until);
        guard++;
    }
    now += FAST_WINDOW_MS;
    step_game(&g, &in, now, sound, sizeof sound, &sound_until);
    in.red_gun = 1;
    now += 1;
    step_game(&g, &in, now, sound, sizeof sound, &sound_until);
    if (game_score(&g, PLAYER_RED) != SCORE_FAST_HIT - SCORE_PENALTY ||
        !game_lamp(&g, LAMP_LATE_RED) ||
        strstr(sound, "return fire") == NULL) {
        fprintf(stderr, "demo check: slow shot did not penalize (score %u)\n",
                (unsigned)game_score(&g, PLAYER_RED));
        return 1;
    }
    in.red_gun = 0;
    now += 1;
    step_game(&g, &in, now, sound, sizeof sound, &sound_until);

    game_set_target_script(&g, white, 1);
    guard = 0;
    while (game_current_target(&g) != TARGET_WHITE_1 && guard < 4000) {
        now += 20;
        step_game(&g, &in, now, sound, sizeof sound, &sound_until);
        guard++;
    }
    in.red_gun = 1;
    now += 1;
    step_game(&g, &in, now, sound, sizeof sound, &sound_until);
    if (game_score(&g, PLAYER_RED) != SCORE_FAST_HIT - (2 * SCORE_PENALTY) ||
        strstr(sound, "white hat") == NULL) {
        fprintf(stderr, "demo check: white hat did not penalize (score %u sound %s)\n",
                (unsigned)game_score(&g, PLAYER_RED), sound);
        return 1;
    }

    /* Two players. Blue is faster. */
    memset(&in, 0, sizeof in);
    game_init(&g, 1);
    now = 0;
    game_set_target_script(&g, saloon, 1);
    in.coin = 1;
    now += 1;
    step_game(&g, &in, now, sound, sizeof sound, &sound_until);
    in.coin = 0;
    now += 1;
    step_game(&g, &in, now, sound, sizeof sound, &sound_until);
    in.coin = 1;
    now += 1;
    step_game(&g, &in, now, sound, sizeof sound, &sound_until);
    in.coin = 0;
    now += 1;
    step_game(&g, &in, now, sound, sizeof sound, &sound_until);
    in.red_gun = 1;
    now += 1;
    step_game(&g, &in, now, sound, sizeof sound, &sound_until);
    in.red_gun = 0;
    now += 1;
    step_game(&g, &in, now, sound, sizeof sound, &sound_until);
    in.blue_gun = 1;
    now += 1;
    step_game(&g, &in, now, sound, sizeof sound, &sound_until);
    in.blue_gun = 0;
    now += 1;
    step_game(&g, &in, now, sound, sizeof sound, &sound_until);
    if (game_players(&g) != 2 || game_phase(&g) != PHASE_PLAY) {
        fprintf(stderr, "demo check: two-player join failed\n");
        return 1;
    }
    guard = 0;
    while (game_current_target(&g) == TARGET_NONE && guard < 2000) {
        now += 1;
        step_game(&g, &in, now, sound, sizeof sound, &sound_until);
        guard++;
    }
    in.blue_gun = 1;
    now += 1;
    step_game(&g, &in, now, sound, sizeof sound, &sound_until);
    if (game_score(&g, PLAYER_BLUE) != SCORE_FAST_HIT ||
        game_score(&g, PLAYER_RED) != 0) {
        fprintf(stderr, "demo check: faster blue shot did not win\n");
        return 1;
    }

    /* Five fast hits reach the photographed free-game score of 50. */
    memset(&in, 0, sizeof in);
    game_init(&g, 1);
    now = 0;
    game_set_target_script(&g, pops, 1);
    in.coin = 1;
    now += 1;
    step_game(&g, &in, now, sound, sizeof sound, &sound_until);
    in.coin = 0;
    now += 1;
    step_game(&g, &in, now, sound, sizeof sound, &sound_until);
    in.red_gun = 1;
    now += 1;
    step_game(&g, &in, now, sound, sizeof sound, &sound_until);
    in.red_gun = 0;
    now += 1;
    step_game(&g, &in, now, sound, sizeof sound, &sound_until);
    for (i = 0; i < 5; i++) {
        guard = 0;
        while (game_current_target(&g) == TARGET_NONE && guard < 2000) {
            now += 1;
            step_game(&g, &in, now, sound, sizeof sound, &sound_until);
            guard++;
        }
        in.red_gun = 1;
        now += 1;
        step_game(&g, &in, now, sound, sizeof sound, &sound_until);
        in.red_gun = 0;
        now += 1;
        step_game(&g, &in, now, sound, sizeof sound, &sound_until);
    }
    if (game_score(&g, PLAYER_RED) != 50 || !game_free_game_awarded(&g) ||
        !game_lamp(&g, LAMP_FREE_GAME) || game_credits(&g) != 1) {
        fprintf(stderr, "demo check: free game at 50 failed (score %u credits %u)\n",
                (unsigned)game_score(&g, PLAYER_RED),
                (unsigned)game_credits(&g));
        return 1;
    }

    in.test_mode = 1;
    in.red_gun = 1;
    now += 1;
    step_game(&g, &in, now, sound, sizeof sound, &sound_until);
    if (game_phase(&g) != PHASE_TEST || !game_lamp(&g, LAMP_TIME_0) ||
        game_score(&g, PLAYER_RED) != 50) {
        fprintf(stderr, "demo check: lamp test did not take over\n");
        return 1;
    }

    printf("demo check ok\n");
    return 0;
}

static int run_play(unsigned scale)
{
    Game g;
    GameInputs in;
    char sound[64];
    uint32_t sound_until = 0;
    uint64_t origin;
    uint32_t game_now = 0;
    int color;

    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        fprintf(stderr,
                "The demo plays in a terminal.\n"
                "  ./build/demo\n"
                "  ./build/demo --scale 1\n"
                "Headless check: ./build/demo --check\n");
        return 1;
    }

    if (tcgetattr(STDIN_FILENO, &g_saved_term) != 0) {
        perror("tcgetattr");
        return 1;
    }
    {
        struct termios raw = g_saved_term;
        raw.c_lflag &= (tcflag_t) ~(ICANON | ECHO);
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) {
            perror("tcsetattr");
            return 1;
        }
    }
    g_term_raw = 1;
    atexit(restore_term);
    fputs("\033[?25l\033[2J", stdout);

    memset(&in, 0, sizeof in);
    sound[0] = '\0';
    game_init(&g, (uint32_t)mono_ms() | 1u);
    color = 1;
    origin = mono_ms();

    for (;;) {
        char keys[32];
        ssize_t n;
        ssize_t k;
        uint64_t elapsed;
        uint32_t want;
        struct timespec pause;

        n = read(STDIN_FILENO, keys, sizeof keys);
        if (n < 0 && errno != EAGAIN) {
            break;
        }
        for (k = 0; k < n; k++) {
            char c = keys[k];
            if (c >= 'A' && c <= 'Z') {
                c = (char)(c - 'A' + 'a');
            }
            if (c == 'q' || c == 27) {
                return 0;
            }
            if (c == 'c') {
                in.coin = 1;
            } else if (c == 'r') {
                in.red_gun = 1;
            } else if (c == 'b') {
                in.blue_gun = 1;
            } else if (c == 't') {
                in.test_mode = (uint8_t)!in.test_mode;
            } else if (c == '[' || c == ']') {
                uint16_t threshold = game_free_game_score(&g);
                if (c == '[' && threshold >= 10) {
                    threshold = (uint16_t)(threshold - 10);
                } else if (c == ']') {
                    threshold = (uint16_t)(threshold + 10);
                }
                game_set_free_game_score(&g, threshold);
            }
        }

        elapsed = mono_ms() - origin;
        want = (uint32_t)(elapsed / scale);
        if (want < game_now) {
            want = game_now;
        }
        /*
         * Catch up without the new trigger, then sample guns and coin on
         * the latest step. Otherwise a slow frame would fire the shot in
         * the past, at a target the player can no longer see.
         */
        while (game_now + 20u < want) {
            GameInputs quiet = in;
            quiet.red_gun = 0;
            quiet.blue_gun = 0;
            quiet.coin = 0;
            game_now += 20u;
            step_game(&g, &quiet, game_now, sound, sizeof sound, &sound_until);
        }
        if (game_now < want) {
            game_now = want;
            step_game(&g, &in, game_now, sound, sizeof sound, &sound_until);
            in.red_gun = 0;
            in.blue_gun = 0;
            in.coin = 0;
        }
        draw(&g, sound, scale, color);

        pause.tv_sec = 0;
        pause.tv_nsec = 20000000L;
        nanosleep(&pause, NULL);
    }
    return 0;
}

static unsigned parse_scale(const char *text)
{
    char *end = NULL;
    long value = strtol(text, &end, 10);
    if (end == text || value < 1 || value > 20) {
        return 0;
    }
    return (unsigned)value;
}

int main(int argc, char **argv)
{
    unsigned scale = 4;
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--check") == 0) {
            return run_check();
        }
        if (strcmp(argv[i], "--scale") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--scale needs a number from 1 to 20\n");
                return 2;
            }
            scale = parse_scale(argv[++i]);
            if (scale == 0) {
                fprintf(stderr, "--scale needs a number from 1 to 20\n");
                return 2;
            }
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: ./build/demo [--scale N]\n"
                   "       ./build/demo --check\n"
                   "Default scale is 4, so the 600 ms fast window lasts about 2.4 s.\n");
            return 0;
        } else {
            fprintf(stderr, "Unknown argument %s\n", argv[i]);
            return 2;
        }
    }
    return run_play(scale);
}
