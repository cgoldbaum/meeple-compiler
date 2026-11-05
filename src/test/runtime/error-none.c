/*
 * Error de ejecucion, escrito a mano contra el runtime: el programa termina
 * con codigo 1 y sin report.
 *
 *      1  game "Nadie" {
 *      2      players 2 to 2;
 *      3      turn {
 *      4          player nadie = none;
 *      5          give nadie.score to current;
 *      6      }
 *      7  }
 *      8  simulate 1 game of "Nadie" with 2 players;
 *      9  report { winrate; }
 */

#include "MeepleRuntime.h"

static void g0_turn(MpPlayer * current) {
	MpPlayer * v_nadie = NULL;
	(void) v_nadie;
	mp_give(current, MP_CHECK(v_nadie, 5)->score, 5);
}

static const MpGame g0 = {
	.name = "Nadie",
	.endAfter = MP_DEFAULT_END_AFTER,
	.clockwise = true,
	.turn = g0_turn
};

int main(void) {
	MpStats * _s = mp_simulate(&g0, 1, 2, 0, false);
	mp_report_winrate(_s);
	mp_stats_destroy(_s);
	return 0;
}
