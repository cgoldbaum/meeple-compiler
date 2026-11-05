/*
 * Error de ejecucion, escrito a mano contra el runtime: el programa termina
 * con codigo 1 y sin report.
 *
 *      1  game "Indice" {
 *      2      players 2 to 2;
 *      3      turn { give players[2].score to current; }
 *      4  }
 *      5  simulate 1 game of "Indice" with 2 players;
 *      6  report { winrate; }
 */

#include "MeepleRuntime.h"

static void g0_turn(MpPlayer * current) {
	mp_give(current, MP_CHECK((MpPlayer *) mp_list_get(mp_player_list, 2, 3).p, 3)->score, 3);
}

static const MpGame g0 = {
	.name = "Indice",
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
