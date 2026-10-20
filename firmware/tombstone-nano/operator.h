#ifndef TOMBSTONE_OPERATOR_H
#define TOMBSTONE_OPERATOR_H

/*
 * Combined-score free game. The flyer says the point setting is
 * operator-adjustable. Transmitters photographed with the game are
 * marked 50, so that is the default here, not a value proven for
 * every machine. The original adjustment control is not public.
 * Edit this constant, or call game_set_free_game_score(). 0 disables
 * the award.
 */
#ifndef TOMBSTONE_FREE_GAME_SCORE
#define TOMBSTONE_FREE_GAME_SCORE 50
#endif

#endif
