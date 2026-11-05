/*
 * "prefer input", escrito a mano contra el runtime: la persona elige por
 * stdin (prefer-input.stdin), con entradas invalidas que se vuelven a pedir.
 * Con prefer-input.sin-entrada.out, la entrada se termina en la primera
 * pregunta: es un error de ejecucion.
 *
 *      1  game "Humano" {
 *      2      players 2 to 2;
 *      3      board cells 5;
 *      4      die D faces {3, 1, 3, 2};
 *      5      piece a per player;
 *      6      piece b per player;
 *      7      decision numero: integer;
 *      8      decision rival: player;
 *      9      decision ficha: piece;
 *     10      strategy Persona {
 *     11          numero: prefer input where option > 1;
 *     12          rival: prefer input;
 *     13          ficha: prefer max option.cell, input;
 *     14      }
 *     15      prepare { players[0] uses Persona; players[1] uses Persona; }
 *     16      turn {
 *     17          integer n = ask current for numero(D.faces);
 *     18          player r = ask current for rival(players);
 *     19          piece f = ask current for ficha(current.pieces);
 *     20          move f forward n;
 *     21          give n to r;
 *     22          log "{current} mueve {f} y le da {n} a {r}";
 *     23      }
 *     24      win when current.score >= 3;
 *     25      end after 2 turns;
 *     26  }
 *     27  simulate 1 game of "Humano" with 2 players verbose;
 *     28  report { winrate by player; }
 */

#include "MeepleRuntime.h"

static const int g0_D_faces[] = { 3, 1, 3, 2 };
static const MpDie g0_D = { g0_D_faces, 4, 1, 3 };

static MpPiece * g0_a;
static MpPiece * g0_b;

static const char * const g0_strategies[] = { "Persona" };

static bool g0_Persona_numero_where(MpValue option, MpPlayer * current) {
	(void) current;
	return option.i > 1;
}

static int g0_Persona_ficha_0(MpValue option, MpPlayer * current) {
	(void) current;
	return MP_CHECK((MpPiece *) option.p, 13)->cell;
}
static const MpRanking g0_Persona_ficha[] = { { g0_Persona_ficha_0, true } };

static const MpPolicy g0_numero_policies[] = {
	{ NULL, NULL, 0, MP_FIRST },
	{ g0_Persona_numero_where, NULL, 0, MP_INPUT }
};
static const MpDecision g0_numero = { "numero", false, mp_describe_integer, g0_numero_policies };

static const MpPolicy g0_rival_policies[] = {
	{ NULL, NULL, 0, MP_FIRST },
	{ NULL, NULL, 0, MP_INPUT }
};
static const MpDecision g0_rival = { "rival", true, mp_describe_player, g0_rival_policies };

static const MpPolicy g0_ficha_policies[] = {
	{ NULL, NULL, 0, MP_FIRST },
	{ NULL, g0_Persona_ficha, 1, MP_INPUT }
};
static const MpDecision g0_ficha = { "ficha", true, mp_describe_piece, g0_ficha_policies };

static void g0_reset(void) {
	g0_a = mp_piece_per_player("a");
	g0_b = mp_piece_per_player("b");
}

static void g0_prepare(void) {
	mp_uses(mp_list_get(mp_player_list, 0, 15).p, 1, 15);
	mp_uses(mp_list_get(mp_player_list, 1, 15).p, 1, 15);
}

static void g0_turn(MpPlayer * current) {
	int v_n = mp_ask(&g0_numero, current, mp_die_faces(&g0_D), 17).i;
	(void) v_n;
	MpPlayer * v_r = mp_ask(&g0_rival, current, mp_player_list, 18).p;
	(void) v_r;
	MpPiece * v_f = mp_ask(&g0_ficha, current, MP_CHECK(current, 19)->pieces, 19).p;
	(void) v_f;
	mp_move(v_f, v_n, 20);
	mp_give(v_r, v_n, 21);
	mp_log_value(mp_describe_player, MP_REFERENCE(current));
	mp_log_text(" mueve ");
	mp_log_value(mp_describe_piece, MP_REFERENCE(v_f));
	mp_log_text(" y le da ");
	mp_log_value(mp_describe_integer, MP_INTEGER(v_n));
	mp_log_text(" a ");
	mp_log_value(mp_describe_player, MP_REFERENCE(v_r));
	mp_log_end();
}

static bool g0_win(MpPlayer * current) {
	return MP_CHECK(current, 24)->score >= 3;
}

static const MpGame g0 = {
	.name = "Humano",
	.boardSize = 5,
	.endAfter = 2,
	.clockwise = true,
	.strategyCount = 1,
	.strategyNames = g0_strategies,
	.reset = g0_reset,
	.prepare = g0_prepare,
	.turn = g0_turn,
	.win = g0_win
};

int main(void) {
	MpStats * _s = mp_simulate(&g0, 1, 2, 0, true);
	mp_report_winrate_by_player(_s);
	mp_stats_destroy(_s);
	return 0;
}
