/*
 * Error de ejecucion, escrito a mano contra el runtime: el programa termina
 * con codigo 1 y sin report.
 *
 *      1  game "Modulo" {
 *      2      players 2 to 2;
 *      3      turn {
 *      4          integer cero = turns - 1;
 *      5          give 10 % cero to current;
 *      6      }
 *      7  }
 *      8  simulate 1 game of "Modulo" with 2 players;
 *      9  report { winrate; }
 */

#include "MeepleRuntime.h"

static void g0_turn(MpPlayer * current) {
	int v_cero = mp_turns - 1;
	(void) v_cero;
	mp_give(current, mp_mod(10, v_cero, 5), 5);
}

static const MpGame g0 = {
	.name = "Modulo",
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
