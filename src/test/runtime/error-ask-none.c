/*
 * Error de ejecucion, escrito a mano contra el runtime: el programa termina
 * con codigo 1 y sin report.
 *
 *      1  game "Ask" {
 *      2      players 2 to 2;
 *      3      decision d: player;
 *      4      turn {
 *      5          player nadie = none;
 *      6          player x = ask nadie for d(players);
 *      7      }
 *      8  }
 *      9  simulate 1 game of "Ask" with 2 players;
 *     10  report { winrate; }
 */

#include "MeepleRuntime.h"

static const MpPolicy g0_d_policies[] = {
	{ NULL, NULL, 0, MP_FIRST }
};
static const MpDecision g0_d = { "d", true, mp_describe_player, g0_d_policies };

static void g0_turn(MpPlayer * current) {
	(void) current;
	MpPlayer * v_nadie = NULL;
	(void) v_nadie;
	MpPlayer * v_x = mp_ask(&g0_d, v_nadie, mp_player_list, 6).p;
	(void) v_x;
}

static const MpGame g0 = {
	.name = "Ask",
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
