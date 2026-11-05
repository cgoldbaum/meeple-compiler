/*
 * Error de ejecucion, escrito a mano contra el runtime: el programa termina
 * con codigo 1 y sin report.
 *
 *      1  game "Opciones" {
 *      2      players 2 to 2;
 *      3      die D faces {1, 2};
 *      4      decision d: integer;
 *      5      strategy S { d: prefer first where option > 5; }
 *      6      prepare { players[0] uses S; }
 *      7      turn { integer x = ask current for d(D.faces); }
 *      8  }
 *      9  simulate 1 game of "Opciones" with 2 players;
 *     10  report { winrate; }
 */

#include "MeepleRuntime.h"

static const int g0_D_faces[] = { 1, 2 };
static const MpDie g0_D = { g0_D_faces, 2, 1, 2 };

static const char * const g0_strategies[] = { "S" };

static bool g0_S_d_where(MpValue option, MpPlayer * current) {
	(void) current;
	return option.i > 5;
}

static const MpPolicy g0_d_policies[] = {
	{ NULL, NULL, 0, MP_FIRST },
	{ g0_S_d_where, NULL, 0, MP_FIRST }
};
static const MpDecision g0_d = { "d", false, mp_describe_integer, g0_d_policies };

static void g0_prepare(void) {
	mp_uses(mp_list_get(mp_player_list, 0, 6).p, 1, 6);
}

static void g0_turn(MpPlayer * current) {
	int v_x = mp_ask(&g0_d, current, mp_die_faces(&g0_D), 7).i;
	(void) v_x;
}

static const MpGame g0 = {
	.name = "Opciones",
	.strategyCount = 1,
	.strategyNames = g0_strategies,
	.prepare = g0_prepare,
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
