/*
 * Determinismo, escrito a mano contra el runtime: el mismo juego, con la misma
 * semilla, da el mismo report con y sin verbose, aunque el log tire un dado.
 * Los dos report de determinismo.out son iguales.
 *
 *      1  game "Azar" {
 *      2      players 3 to 3;
 *      3      die D6 faces 1 to 6;
 *      4      turn {
 *      5          log "{current.name} tira {roll D6}";
 *      6          give roll D6 to current;
 *      7      }
 *      8      win when current.score >= 12;
 *      9  }
 *     10  simulate 4 games of "Azar" with 3 players seed 11;
 *     11  report { winrate by player; avg turns; }
 *     12  simulate 4 games of "Azar" with 3 players seed 11 verbose;
 *     13  report { winrate by player; avg turns; }
 */

#include "MeepleRuntime.h"

static const int g0_D6_faces[] = { 1, 2, 3, 4, 5, 6 };
static const MpDie g0_D6 = { g0_D6_faces, 6, 1, 6 };

static void g0_turn(MpPlayer * current) {
	mp_log_value(mp_describe_string, MP_STRING(MP_CHECK(current, 5)->name));
	mp_log_text(" tira ");
	mp_log_value(mp_describe_integer, MP_INTEGER(mp_roll(&g0_D6)));
	mp_log_end();
	mp_give(current, mp_roll(&g0_D6), 6);
}

static bool g0_win(MpPlayer * current) {
	return MP_CHECK(current, 8)->score >= 12;
}

static const MpGame g0 = {
	.name = "Azar",
	.endAfter = MP_DEFAULT_END_AFTER,
	.clockwise = true,
	.turn = g0_turn,
	.win = g0_win
};

int main(void) {
	MpStats * _s = mp_simulate(&g0, 4, 3, 11, false);
	mp_report_winrate_by_player(_s);
	mp_report_metric(_s, MP_TURNS, MP_AVG);
	mp_stats_destroy(_s);
	_s = mp_simulate(&g0, 4, 3, 11, true);
	mp_report_winrate_by_player(_s);
	mp_report_metric(_s, MP_TURNS, MP_AVG);
	mp_stats_destroy(_s);
	return 0;
}
