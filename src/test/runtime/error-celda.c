/*
 * Error de ejecucion, escrito a mano contra el runtime: el programa termina
 * con codigo 1 y sin report.
 *
 *      1  game "Celda" {
 *      2      players 2 to 2;
 *      3      board cells 5;
 *      4      piece f;
 *      5      turn { place f on board.cell(turns + 4); }
 *      6  }
 *      7  simulate 1 game of "Celda" with 2 players;
 *      8  report { winrate; }
 */

#include "MeepleRuntime.h"

static MpPiece * g0_f;

static void g0_reset(void) {
	g0_f = mp_piece_new("f");
}

static void g0_turn(MpPlayer * current) {
	(void) current;
	mp_place(g0_f, mp_cell(mp_turns + 4, 5), 5);
}

static const MpGame g0 = {
	.name = "Celda",
	.boardSize = 5,
	.reset = g0_reset,
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
