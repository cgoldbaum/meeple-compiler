/*
 * El ejemplo 6.2 de la especificacion (src/test/c/accept/22-ejemplo-duelo-de-cartas),
 * escrito a mano contra el runtime.
 *
 *     game "Duelo de Cartas" {
 *         players 2 to 2;
 *         cardtype Carta { integer ataque; integer costo; }
 *         deck Mazo of Carta {
 *             card { ataque: 1; costo: 1; }   card { ataque: 2; costo: 1; }
 *             card { ataque: 3; costo: 2; }   card { ataque: 4; costo: 3; }
 *         }
 *         deck Mano of Carta per player;
 *         deck Mesa of Carta;
 *         decision jugar: Carta;
 *         strategy Agresivo  { jugar: prefer max option.ataque; }
 *         strategy Economico { jugar: prefer min option.costo;  }
 *         prepare {
 *             shuffle Mazo;
 *             for (p in players) {
 *                 draw 3 from Mazo to p.Mano;
 *                 give 5 to p;
 *             }
 *             players[0] uses Agresivo;
 *             players[1] uses Economico;
 *         }
 *         turn {
 *             if (count(c in Mazo) > 0) { draw 1 from Mazo to current.Mano; }
 *             Carta elegida = ask current for jugar(current.Mano);
 *             if (elegida != none and elegida.costo <= current.score) {
 *                 take elegida.costo from current;
 *                 play elegida from current.Mano to Mesa;
 *                 give elegida.ataque to current;
 *             }
 *         }
 *         win when current.score >= 15;
 *         end after 20 turns;
 *     }
 *     simulate 5000 games of "Duelo de Cartas" with 2 players seed 1;
 *     report { winrate by strategy; avg turns; }
 *
 * Nadie gana nunca: el mazo tiene 4 cartas, cada una suma a lo sumo 1 punto
 * neto (ataque - costo) y se empieza con 5, asi que el score no pasa de 8.
 */

#include "MeepleRuntime.h"

/* cardtype Carta { integer ataque; integer costo; } */
typedef struct {
	int ataque;
	int costo;
} g0_Carta;

static g0_Carta * g0_Carta_new(int ataque, int costo) {
	g0_Carta * card = mp_alloc(sizeof(g0_Carta));
	card->ataque = ataque;
	card->costo = costo;
	return card;
}

static void g0_Carta_describe(FILE * out, MpValue value) {
	const g0_Carta * card = value.p;
	if (card == NULL) {
		fputs("none", out);
		return;
	}
	fprintf(out, "{ataque: %d, costo: %d}", card->ataque, card->costo);
}

static MpList * g0_Mazo;
static MpList ** g0_Mano;
static MpList * g0_Mesa;

/* Estrategias: 1 = Agresivo, 2 = Economico. */
static const char * const g0_strategies[] = { "Agresivo", "Economico" };

/* strategy Agresivo { jugar: prefer max option.ataque; } */
static int g0_Agresivo_jugar_0(MpValue option, MpPlayer * current) {
	(void) current;
	return MP_CHECK((g0_Carta *) option.p, 11)->ataque;
}
static const MpRanking g0_Agresivo_jugar[] = { { g0_Agresivo_jugar_0, true } };

/* strategy Economico { jugar: prefer min option.costo; } */
static int g0_Economico_jugar_0(MpValue option, MpPlayer * current) {
	(void) current;
	return MP_CHECK((g0_Carta *) option.p, 12)->costo;
}
static const MpRanking g0_Economico_jugar[] = { { g0_Economico_jugar_0, false } };

/* decision jugar: Carta; con una politica por estrategia (0: sin uses, prefer first). */
static const MpPolicy g0_jugar_policies[] = {
	{ NULL, NULL, 0, MP_FIRST },
	{ NULL, g0_Agresivo_jugar, 1, MP_FIRST },
	{ NULL, g0_Economico_jugar, 1, MP_FIRST }
};
static const MpDecision g0_jugar = { "jugar", true, g0_Carta_describe, g0_jugar_policies };

static void g0_reset(void) {
	g0_Mazo = mp_list_new();
	mp_list_push(g0_Mazo, MP_REFERENCE(g0_Carta_new(1, 1)));
	mp_list_push(g0_Mazo, MP_REFERENCE(g0_Carta_new(2, 1)));
	mp_list_push(g0_Mazo, MP_REFERENCE(g0_Carta_new(3, 2)));
	mp_list_push(g0_Mazo, MP_REFERENCE(g0_Carta_new(4, 3)));
	g0_Mano = mp_deck_per_player();
	g0_Mesa = mp_list_new();
}

static void g0_prepare(void) {
	mp_shuffle(g0_Mazo);
	MpList * _t0 = mp_list_copy(mp_player_list);
	for (int _t1 = 0; _t1 < _t0->size; ++_t1) {
		MpPlayer * v_p = _t0->items[_t1].p;
		mp_draw(3, g0_Mazo, g0_Mano[MP_CHECK(v_p, 16)->index]);
		mp_give(v_p, 5, 17);
	}
	mp_uses(mp_list_get(mp_player_list, 0, 19).p, 1, 19);
	mp_uses(mp_list_get(mp_player_list, 1, 20).p, 2, 20);
}

static void g0_turn(MpPlayer * current) {
	/* count(c in Mazo): las agregaciones son statement expressions, con temporales frescos. */
	if (({
		int _t2 = 0;
		MpList * _t3 = g0_Mazo;
		for (int _t4 = 0; _t4 < _t3->size; ++_t4) {
			g0_Carta * v_c = _t3->items[_t4].p;
			(void) v_c;
			++_t2;
		}
		_t2;
	}) > 0) {
		mp_draw(1, g0_Mazo, g0_Mano[MP_CHECK(current, 23)->index]);
	}
	g0_Carta * v_elegida = mp_ask(&g0_jugar, current, g0_Mano[MP_CHECK(current, 24)->index], 24).p;
	(void) v_elegida;
	/* "and" corta como en C: si elegida es none, no se evalua elegida.costo. */
	if (v_elegida != NULL && MP_CHECK(v_elegida, 25)->costo <= MP_CHECK(current, 25)->score) {
		mp_take(current, MP_CHECK(v_elegida, 26)->costo, 26);
		mp_play(v_elegida, g0_Mano[MP_CHECK(current, 27)->index], g0_Mesa);
		mp_give(current, MP_CHECK(v_elegida, 28)->ataque, 28);
	}
}

static bool g0_win(MpPlayer * current) {
	return MP_CHECK(current, 31)->score >= 15;
}

static const MpGame g0 = {
	.name = "Duelo de Cartas",
	.endAfter = 20,
	.clockwise = true,
	.strategyCount = 2,
	.strategyNames = g0_strategies,
	.reset = g0_reset,
	.prepare = g0_prepare,
	.turn = g0_turn,
	.win = g0_win
};

int main(void) {
	MpStats * _s = mp_simulate(&g0, 5000, 2, 1, false);
	mp_report_winrate_by_strategy(_s);
	mp_report_metric(_s, MP_TURNS, MP_AVG);
	mp_stats_destroy(_s);
	return 0;
}
