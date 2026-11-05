/*
 * Las acciones, las agregaciones, los log y las politicas con "where", sin
 * depender del azar, escritos a mano contra el runtime. La salida esperada
 * (acciones.out) esta calculada a mano.
 *
 *      1  game "Acciones" {
 *      2      players 2 to 2;
 *      3      board cells 10;
 *      4      die Raro faces {2, 4, 4};
 *      5      cardtype Carta { integer valor; string palo; }
 *      6      deck Mazo of Carta { valor: 1 to 3; palo: {"oro", "copa"}; }
 *      7      deck Pozo of Carta;
 *      8      piece ficha;
 *      9      decision elegir: Carta;
 *     10      decision numero: integer;
 *     11      strategy Exigente {
 *     12          elegir: prefer max option.valor where option.palo == "copa";
 *     13          numero: prefer min option where option > 2;
 *     14      }
 *     15      strategy Primera { elegir: prefer first where option.valor == 9; numero: prefer first; }
 *     16      metric puntaje = current.score;
 *     17      metric minimo = min(p in players by p.score).index;
 *     18      prepare {
 *     19          draw 2 from Mazo to Pozo;
 *     20          draw 10 from Mazo to Pozo;
 *     21          draw 0 from Pozo to Mazo;
 *     22          draw -1 from Pozo to Mazo;
 *     23          draw 1 from Pozo to Pozo;
 *     24          play Pozo[0] from Mazo to Pozo;
 *     25          play Pozo[2] from Pozo to Mazo;
 *     26          log "Pozo {Pozo[0]} {Pozo[1]} {Pozo[4]} Mazo {count(c in Mazo)} {Mazo[0].palo}";
 *     27          Mazo[0].valor = 9;
 *     28          log "suma {sum(c in Pozo by c.valor)} max {max(c in Pozo by c.valor).palo} pares {count(c in Pozo where c.valor % 2 == 0)} mazo {Mazo[0].valor}";
 *     29          shuffle Pozo;
 *     30          log "barajado {count(c in Pozo)} {sum(c in Pozo by c.valor)}";
 *     31          place ficha on board.cell(3);
 *     32          move ficha forward 100;
 *     33          log "{ficha}";
 *     34          move ficha forward -100;
 *     35          log "{ficha} {board.size}";
 *     36          integer total = 0;
 *     37          for (i in 1 to 4) { total = total + i; }
 *     38          repeat 3 { total = total * 2; }
 *     39          while (total > 50) { total = total - 7; }
 *     40          log "total {total} caras {sum(x in Raro.faces by x)} {count(x in Raro.faces)}";
 *     41          player[] lista = select(p in players where p.index == 1);
 *     42          log "{lista[0]} {count(q in lista)}";
 *     43          lista = players;
 *     44          lista[0] = none;
 *     45          log "{players[0]} {lista[0]}";
 *     46          players[0] uses Primera;
 *     47          players[1] uses Exigente;
 *     48      }
 *     49      turn {
 *     50          Carta c = ask current for elegir(Pozo);
 *     51          log "{current} elige {c} con {current.strategy}";
 *     52          integer n = ask current for numero(Raro.faces);
 *     53          log "{current.name} numero {n} {roll Raro > 1}";
 *     54          give 3 to current;
 *     55          take 5 from current;
 *     56          log "score {current.score}";
 *     57      }
 *     58      win when turns >= 4;
 *     59  }
 *     60  simulate 2 games of "Acciones" with 2 players seed 3 verbose;
 *     61  report { winrate; winrate by player; winrate by strategy; min turns; max turns; avg puntaje; min minimo; }
 *
 * Los arreglos se copian al asignarse: "lista[0] = none" no toca players.
 */

#include "MeepleRuntime.h"

static const int g0_Raro_faces[] = { 2, 4, 4 };
static const MpDie g0_Raro = { g0_Raro_faces, 3, 2, 4 };

typedef struct {
	int valor;
	const char * palo;
} g0_Carta;

static g0_Carta * g0_Carta_new(int valor, const char * palo) {
	g0_Carta * card = mp_alloc(sizeof(g0_Carta));
	card->valor = valor;
	card->palo = palo;
	return card;
}

static void g0_Carta_describe(FILE * out, MpValue value) {
	const g0_Carta * card = value.p;
	if (card == NULL) {
		fputs("none", out);
		return;
	}
	fprintf(out, "{valor: %d, palo: %s}", card->valor, card->palo);
}

static MpList * g0_Mazo;
static MpList * g0_Pozo;
static MpPiece * g0_ficha;

/* Estrategias: 1 = Exigente, 2 = Primera. */
static const char * const g0_strategies[] = { "Exigente", "Primera" };
static const char * const g0_metrics[] = { "puntaje", "minimo" };

static bool g0_Exigente_elegir_where(MpValue option, MpPlayer * current) {
	(void) current;
	return strcmp(MP_CHECK((g0_Carta *) option.p, 12)->palo, "copa") == 0;
}
static int g0_Exigente_elegir_0(MpValue option, MpPlayer * current) {
	(void) current;
	return MP_CHECK((g0_Carta *) option.p, 12)->valor;
}
static const MpRanking g0_Exigente_elegir[] = { { g0_Exigente_elegir_0, true } };

static bool g0_Exigente_numero_where(MpValue option, MpPlayer * current) {
	(void) current;
	return option.i > 2;
}
static int g0_Exigente_numero_0(MpValue option, MpPlayer * current) {
	(void) current;
	return option.i;
}
static const MpRanking g0_Exigente_numero[] = { { g0_Exigente_numero_0, false } };

static bool g0_Primera_elegir_where(MpValue option, MpPlayer * current) {
	(void) current;
	return MP_CHECK((g0_Carta *) option.p, 15)->valor == 9;
}

static const MpPolicy g0_elegir_policies[] = {
	{ NULL, NULL, 0, MP_FIRST },
	{ g0_Exigente_elegir_where, g0_Exigente_elegir, 1, MP_FIRST },
	{ g0_Primera_elegir_where, NULL, 0, MP_FIRST }
};
static const MpDecision g0_elegir = { "elegir", true, g0_Carta_describe, g0_elegir_policies };

static const MpPolicy g0_numero_policies[] = {
	{ NULL, NULL, 0, MP_FIRST },
	{ g0_Exigente_numero_where, g0_Exigente_numero, 1, MP_FIRST },
	{ NULL, NULL, 0, MP_FIRST }
};
static const MpDecision g0_numero = { "numero", false, mp_describe_integer, g0_numero_policies };

static void g0_reset(void) {
	/* Producto cartesiano: el primer generador es el bucle de afuera. */
	g0_Mazo = mp_list_new();
	static const char * const palos[] = { "oro", "copa" };
	for (int valor = 1; valor <= 3; ++valor) {
		for (int k = 0; k < 2; ++k) {
			mp_list_push(g0_Mazo, MP_REFERENCE(g0_Carta_new(valor, palos[k])));
		}
	}
	g0_Pozo = mp_list_new();
	g0_ficha = mp_piece_new("ficha");
}

static void g0_prepare(void) {
	mp_draw(2, g0_Mazo, g0_Pozo);
	mp_draw(10, g0_Mazo, g0_Pozo);
	mp_draw(0, g0_Pozo, g0_Mazo);
	mp_draw(-1, g0_Pozo, g0_Mazo);
	mp_draw(1, g0_Pozo, g0_Pozo);
	mp_play(mp_list_get(g0_Pozo, 0, 24).p, g0_Mazo, g0_Pozo);
	mp_play(mp_list_get(g0_Pozo, 2, 25).p, g0_Pozo, g0_Mazo);

	mp_log_text("Pozo ");
	mp_log_value(g0_Carta_describe, mp_list_get(g0_Pozo, 0, 26));
	mp_log_text(" ");
	mp_log_value(g0_Carta_describe, mp_list_get(g0_Pozo, 1, 26));
	mp_log_text(" ");
	mp_log_value(g0_Carta_describe, mp_list_get(g0_Pozo, 4, 26));
	mp_log_text(" Mazo ");
	mp_log_value(mp_describe_integer, MP_INTEGER(({
		int _t0 = 0;
		MpList * _t1 = g0_Mazo;
		for (int _t2 = 0; _t2 < _t1->size; ++_t2) {
			g0_Carta * v_c = _t1->items[_t2].p;
			(void) v_c;
			++_t0;
		}
		_t0;
	})));
	mp_log_text(" ");
	mp_log_value(mp_describe_string, MP_STRING(MP_CHECK((g0_Carta *) mp_list_get(g0_Mazo, 0, 26).p, 26)->palo));
	mp_log_end();

	MP_CHECK((g0_Carta *) mp_list_get(g0_Mazo, 0, 27).p, 27)->valor = 9;

	mp_log_text("suma ");
	mp_log_value(mp_describe_integer, MP_INTEGER(({
		int _t3 = 0;
		MpList * _t4 = g0_Pozo;
		for (int _t5 = 0; _t5 < _t4->size; ++_t5) {
			g0_Carta * v_c = _t4->items[_t5].p;
			_t3 += MP_CHECK(v_c, 28)->valor;
		}
		_t3;
	})));
	mp_log_text(" max ");
	/* max(c in C by e): el primero con el mayor e; none si C esta vacia. */
	mp_log_value(mp_describe_string, MP_STRING(MP_CHECK(({
		g0_Carta * _t6 = NULL;
		int _t7 = 0;
		bool _t8 = false;
		MpList * _t9 = g0_Pozo;
		for (int _t10 = 0; _t10 < _t9->size; ++_t10) {
			g0_Carta * v_c = _t9->items[_t10].p;
			const int _t11 = MP_CHECK(v_c, 28)->valor;
			if (!_t8 || _t11 > _t7) {
				_t6 = v_c;
				_t7 = _t11;
				_t8 = true;
			}
		}
		_t6;
	}), 28)->palo));
	mp_log_text(" pares ");
	mp_log_value(mp_describe_integer, MP_INTEGER(({
		int _t12 = 0;
		MpList * _t13 = g0_Pozo;
		for (int _t14 = 0; _t14 < _t13->size; ++_t14) {
			g0_Carta * v_c = _t13->items[_t14].p;
			if (mp_mod(MP_CHECK(v_c, 28)->valor, 2, 28) == 0) {
				++_t12;
			}
		}
		_t12;
	})));
	mp_log_text(" mazo ");
	mp_log_value(mp_describe_integer, MP_INTEGER(MP_CHECK((g0_Carta *) mp_list_get(g0_Mazo, 0, 28).p, 28)->valor));
	mp_log_end();

	mp_shuffle(g0_Pozo);
	mp_log_text("barajado ");
	mp_log_value(mp_describe_integer, MP_INTEGER(({
		int _t15 = 0;
		MpList * _t16 = g0_Pozo;
		for (int _t17 = 0; _t17 < _t16->size; ++_t17) {
			g0_Carta * v_c = _t16->items[_t17].p;
			(void) v_c;
			++_t15;
		}
		_t15;
	})));
	mp_log_text(" ");
	mp_log_value(mp_describe_integer, MP_INTEGER(({
		int _t18 = 0;
		MpList * _t19 = g0_Pozo;
		for (int _t20 = 0; _t20 < _t19->size; ++_t20) {
			g0_Carta * v_c = _t19->items[_t20].p;
			_t18 += MP_CHECK(v_c, 30)->valor;
		}
		_t18;
	})));
	mp_log_end();

	mp_place(g0_ficha, mp_cell(3, 31), 31);
	mp_move(g0_ficha, 100, 32);
	mp_log_value(mp_describe_piece, MP_REFERENCE(g0_ficha));
	mp_log_end();
	mp_move(g0_ficha, -100, 34);
	mp_log_value(mp_describe_piece, MP_REFERENCE(g0_ficha));
	mp_log_text(" ");
	mp_log_value(mp_describe_integer, MP_INTEGER(mp_board_size));
	mp_log_end();

	int v_total = 0;
	(void) v_total;
	/* for (i in a to b): los extremos se evaluan una vez. */
	for (int v_i = 1, _t21 = 4; v_i <= _t21; ++v_i) {
		v_total = v_total + v_i;
	}
	for (int _t22 = 0, _t23 = 3; _t22 < _t23; ++_t22) {
		v_total = v_total * 2;
	}
	while (v_total > 50) {
		v_total = v_total - 7;
	}
	mp_log_text("total ");
	mp_log_value(mp_describe_integer, MP_INTEGER(v_total));
	mp_log_text(" caras ");
	mp_log_value(mp_describe_integer, MP_INTEGER(({
		int _t24 = 0;
		MpList * _t25 = mp_die_faces(&g0_Raro);
		for (int _t26 = 0; _t26 < _t25->size; ++_t26) {
			int v_x = _t25->items[_t26].i;
			_t24 += v_x;
		}
		_t24;
	})));
	mp_log_text(" ");
	mp_log_value(mp_describe_integer, MP_INTEGER(({
		int _t27 = 0;
		MpList * _t28 = mp_die_faces(&g0_Raro);
		for (int _t29 = 0; _t29 < _t28->size; ++_t29) {
			int v_x = _t28->items[_t29].i;
			(void) v_x;
			++_t27;
		}
		_t27;
	})));
	mp_log_end();

	MpList * v_lista = ({
		MpList * _t30 = mp_list_new();
		MpList * _t31 = mp_player_list;
		for (int _t32 = 0; _t32 < _t31->size; ++_t32) {
			MpPlayer * v_p = _t31->items[_t32].p;
			if (MP_CHECK(v_p, 41)->index == 1) {
				mp_list_push(_t30, MP_REFERENCE(v_p));
			}
		}
		_t30;
	});
	(void) v_lista;
	mp_log_value(mp_describe_player, mp_list_get(v_lista, 0, 42));
	mp_log_text(" ");
	mp_log_value(mp_describe_integer, MP_INTEGER(({
		int _t33 = 0;
		MpList * _t34 = v_lista;
		for (int _t35 = 0; _t35 < _t34->size; ++_t35) {
			MpPlayer * v_q = _t34->items[_t35].p;
			(void) v_q;
			++_t33;
		}
		_t33;
	})));
	mp_log_end();

	/* Asignar un arreglo lo copia. */
	v_lista = mp_list_copy(mp_player_list);
	mp_list_set(v_lista, 0, MP_REFERENCE(NULL), 44);
	mp_log_value(mp_describe_player, mp_list_get(mp_player_list, 0, 45));
	mp_log_text(" ");
	mp_log_value(mp_describe_player, mp_list_get(v_lista, 0, 45));
	mp_log_end();

	mp_uses(mp_list_get(mp_player_list, 0, 46).p, 2, 46);
	mp_uses(mp_list_get(mp_player_list, 1, 47).p, 1, 47);
}

static void g0_turn(MpPlayer * current) {
	g0_Carta * v_c = mp_ask(&g0_elegir, current, g0_Pozo, 50).p;
	(void) v_c;
	mp_log_value(mp_describe_player, MP_REFERENCE(current));
	mp_log_text(" elige ");
	mp_log_value(g0_Carta_describe, MP_REFERENCE(v_c));
	mp_log_text(" con ");
	mp_log_value(mp_describe_strategy, MP_INTEGER(MP_CHECK(current, 51)->strategy));
	mp_log_end();
	int v_n = mp_ask(&g0_numero, current, mp_die_faces(&g0_Raro), 52).i;
	(void) v_n;
	mp_log_value(mp_describe_string, MP_STRING(MP_CHECK(current, 53)->name));
	mp_log_text(" numero ");
	mp_log_value(mp_describe_integer, MP_INTEGER(v_n));
	mp_log_text(" ");
	mp_log_value(mp_describe_boolean, MP_BOOLEAN(mp_roll(&g0_Raro) > 1));
	mp_log_end();
	mp_give(current, 3, 54);
	mp_take(current, 5, 55);
	mp_log_text("score ");
	mp_log_value(mp_describe_integer, MP_INTEGER(MP_CHECK(current, 56)->score));
	mp_log_end();
}

static bool g0_win(MpPlayer * current) {
	(void) current;
	return mp_turns >= 4;
}

static void g0_metrics_values(MpPlayer * current, int * values) {
	values[0] = MP_CHECK(current, 16)->score;
	values[1] = MP_CHECK(({
		MpPlayer * _t36 = NULL;
		int _t37 = 0;
		bool _t38 = false;
		MpList * _t39 = mp_player_list;
		for (int _t40 = 0; _t40 < _t39->size; ++_t40) {
			MpPlayer * v_p = _t39->items[_t40].p;
			const int _t41 = MP_CHECK(v_p, 17)->score;
			if (!_t38 || _t41 < _t37) {
				_t36 = v_p;
				_t37 = _t41;
				_t38 = true;
			}
		}
		_t36;
	}), 17)->index;
}

static const MpGame g0 = {
	.name = "Acciones",
	.boardSize = 10,
	.endAfter = MP_DEFAULT_END_AFTER,
	.clockwise = true,
	.strategyCount = 2,
	.strategyNames = g0_strategies,
	.metricCount = 2,
	.metricNames = g0_metrics,
	.reset = g0_reset,
	.prepare = g0_prepare,
	.turn = g0_turn,
	.win = g0_win,
	.metrics = g0_metrics_values
};

int main(void) {
	MpStats * _s = mp_simulate(&g0, 2, 2, 3, true);
	mp_report_winrate(_s);
	mp_report_winrate_by_player(_s);
	mp_report_winrate_by_strategy(_s);
	mp_report_metric(_s, MP_TURNS, MP_MIN);
	mp_report_metric(_s, MP_TURNS, MP_MAX);
	mp_report_metric(_s, 0, MP_AVG);
	mp_report_metric(_s, 1, MP_MIN);
	mp_stats_destroy(_s);
	return 0;
}
