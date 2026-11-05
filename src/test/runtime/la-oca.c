/*
 * El ejemplo 6.1 de la especificacion (src/test/c/accept/21-ejemplo-la-oca),
 * escrito a mano contra el runtime: es lo que tiene que emitir el generador.
 * Los numeros de linea de MP_CHECK y compania son los del programa Meeple.
 *
 *     game "La Oca" {
 *         players 2 to 4;
 *         board cells 63;
 *         die D6 faces 1 to 6;
 *         piece token per player;
 *         prepare {
 *             for (p in players) {
 *                 place p.token on board.cell(0);
 *             }
 *         }
 *         turn {
 *             integer pasos = roll D6;
 *             move current.token forward pasos;
 *             log "{current.name} avanzo {pasos} y quedo en la celda {current.token.cell}";
 *         }
 *         win when current.token.cell >= 62;
 *         end after 500 turns;
 *     }
 *     simulate 1000 games of "La Oca" with 4 players seed 42;
 *     report { winrate by player; avg turns; }
 */

#include "MeepleRuntime.h"

/* die D6 faces 1 to 6; */
static const int g0_D6_faces[] = { 1, 2, 3, 4, 5, 6 };
static const MpDie g0_D6 = { g0_D6_faces, 6, 1, 6 };

/* piece token per player; */
static MpPiece * g0_token;

static void g0_reset(void) {
	g0_token = mp_piece_per_player("token");
}

static void g0_prepare(void) {
	/* for (p in players): recorre una copia, asi el cuerpo puede cambiar la coleccion. */
	MpList * _t0 = mp_list_copy(mp_player_list);
	for (int _t1 = 0; _t1 < _t0->size; ++_t1) {
		MpPlayer * v_p = _t0->items[_t1].p;
		mp_place(&g0_token[MP_CHECK(v_p, 8)->index], mp_cell(0, 8), 8);
	}
}

static void g0_turn(MpPlayer * current) {
	int v_pasos = mp_roll(&g0_D6);
	(void) v_pasos;
	mp_move(&g0_token[MP_CHECK(current, 13)->index], v_pasos, 13);
	mp_log_value(mp_describe_string, MP_STRING(MP_CHECK(current, 14)->name));
	mp_log_text(" avanzo ");
	mp_log_value(mp_describe_integer, MP_INTEGER(v_pasos));
	mp_log_text(" y quedo en la celda ");
	mp_log_value(mp_describe_integer, MP_INTEGER(MP_CHECK(&g0_token[MP_CHECK(current, 14)->index], 14)->cell));
	mp_log_end();
}

static bool g0_win(MpPlayer * current) {
	return MP_CHECK(&g0_token[MP_CHECK(current, 16)->index], 16)->cell >= 62;
}

static const MpGame g0 = {
	.name = "La Oca",
	.boardSize = 63,
	.endAfter = 500,
	.clockwise = true,
	.reset = g0_reset,
	.prepare = g0_prepare,
	.turn = g0_turn,
	.win = g0_win
};

int main(void) {
	MpStats * _s = mp_simulate(&g0, 1000, 4, 42, false);
	mp_report_winrate_by_player(_s);
	mp_report_metric(_s, MP_TURNS, MP_AVG);
	mp_stats_destroy(_s);
	return 0;
}
