/*
 * Una decision sobre fichas (src/test/c/accept/41-decision-sobre-fichas),
 * escrita a mano contra el runtime. Prueba "pieces", criterios encadenados con
 * "random" al final, la cota de "move" y "winrate by strategy".
 *
 *     game "Dos fichas" {
 *         players 2 to 2;
 *         board cells 20;
 *         die D6 faces 1 to 6;
 *         piece a per player;
 *         piece b per player;
 *         decision mover: piece;
 *         strategy Rezagada { mover: prefer min option.cell; }
 *         strategy Puntera { mover: prefer max option.cell, random; }
 *         prepare { players[0] uses Rezagada; players[1] uses Puntera; }
 *         turn {
 *             piece f = ask current for mover(current.pieces);
 *             move f forward roll D6;
 *         }
 *         win when current.a.cell == 19 and current.b.cell == 19;
 *         end after 200 turns;
 *     }
 *     simulate 1000 games of "Dos fichas" with 2 players seed 7;
 *     report { winrate by strategy; avg turns; }
 *
 * Puntera nunca gana: en cuanto una ficha llega a la celda 19 la sigue
 * eligiendo, y move no la saca del tablero.
 */

#include "MeepleRuntime.h"

static const int g0_D6_faces[] = { 1, 2, 3, 4, 5, 6 };
static const MpDie g0_D6 = { g0_D6_faces, 6, 1, 6 };

static MpPiece * g0_a;
static MpPiece * g0_b;

static const char * const g0_strategies[] = { "Rezagada", "Puntera" };

static int g0_Rezagada_mover_0(MpValue option, MpPlayer * current) {
	(void) current;
	return MP_CHECK((MpPiece *) option.p, 9)->cell;
}
static const MpRanking g0_Rezagada_mover[] = { { g0_Rezagada_mover_0, false } };

static int g0_Puntera_mover_0(MpValue option, MpPlayer * current) {
	(void) current;
	return MP_CHECK((MpPiece *) option.p, 10)->cell;
}
static const MpRanking g0_Puntera_mover[] = { { g0_Puntera_mover_0, true } };

static const MpPolicy g0_mover_policies[] = {
	{ NULL, NULL, 0, MP_FIRST },
	{ NULL, g0_Rezagada_mover, 1, MP_FIRST },
	{ NULL, g0_Puntera_mover, 1, MP_RANDOM }
};
static const MpDecision g0_mover = { "mover", true, mp_describe_piece, g0_mover_policies };

static void g0_reset(void) {
	g0_a = mp_piece_per_player("a");
	g0_b = mp_piece_per_player("b");
}

static void g0_prepare(void) {
	mp_uses(mp_list_get(mp_player_list, 0, 11).p, 1, 11);
	mp_uses(mp_list_get(mp_player_list, 1, 11).p, 2, 11);
}

static void g0_turn(MpPlayer * current) {
	MpPiece * v_f = mp_ask(&g0_mover, current, MP_CHECK(current, 13)->pieces, 13).p;
	(void) v_f;
	mp_move(v_f, mp_roll(&g0_D6), 14);
}

static bool g0_win(MpPlayer * current) {
	return MP_CHECK(&g0_a[MP_CHECK(current, 16)->index], 16)->cell == 19
		&& MP_CHECK(&g0_b[MP_CHECK(current, 16)->index], 16)->cell == 19;
}

static const MpGame g0 = {
	.name = "Dos fichas",
	.boardSize = 20,
	.endAfter = 200,
	.clockwise = true,
	.strategyCount = 2,
	.strategyNames = g0_strategies,
	.reset = g0_reset,
	.prepare = g0_prepare,
	.turn = g0_turn,
	.win = g0_win
};

int main(void) {
	MpStats * _s = mp_simulate(&g0, 1000, 2, 7, false);
	mp_report_winrate_by_strategy(_s);
	mp_report_metric(_s, MP_TURNS, MP_AVG);
	mp_stats_destroy(_s);
	return 0;
}
