/*
 * Juegos sin azar cuyo resultado se sabe de antemano, escritos a mano contra
 * el runtime. La salida esperada (oraculos.out) esta calculada a mano.
 *
 *     game "Contador" {
 *         players 2 to 3;
 *         decision d: integer;
 *         strategy Nadie { d: prefer first; }
 *         metric puntos = current.score;
 *         turn { give 1 to current; }
 *         win when current.score >= 3;
 *     }
 *     game "Ronda" {
 *         players 3 to 3;
 *         turn order counterclockwise;
 *         win when current.index == 1;
 *     }
 *     game "Infinito" {
 *         players 1 to 1;
 *     }
 *     game "Vacio" {
 *         players 2 to 2;
 *         metric indice = current.index + 7;
 *         end after 0 turns;
 *     }
 *     simulate 10 games of "Contador" with 2 players;
 *     report {
 *         winrate; winrate by player; winrate by strategy;
 *         avg turns; min turns; max turns;
 *         avg puntos; min puntos; max puntos;
 *     }
 *     simulate 4 games of "Ronda" with 3 players seed 5;
 *     report { winrate by player; avg turns; }
 *     simulate 3 games of "Infinito" with 1 players;
 *     report { winrate; avg turns; }
 *     simulate 2 games of "Vacio" with 2 players;
 *     report { winrate; avg turns; avg indice; }
 *
 * - Contador: con 2 jugadores, Jugador 1 llega a 3 en el turno 5. Nadie usa
 *   Nadie; los dos jugadores terminan sin estrategia: 10 victorias sobre 20.
 * - Ronda: en sentido antihorario el orden es 0, 2, 1, asi que Jugador 2
 *   gana en el turno 3 (en sentido horario seria el 2).
 * - Infinito: sin turn ni win when, termina por la cota de 1000 turnos.
 * - Vacio: sin turnos, las metricas se evaluan con players[0].
 */

#include "MeepleRuntime.h"

/* game "Contador" */

static const char * const g0_strategies[] = { "Nadie" };
static const char * const g0_metrics[] = { "puntos" };

static void g0_turn(MpPlayer * current) {
	mp_give(current, 1, 6);
}

static bool g0_win(MpPlayer * current) {
	return MP_CHECK(current, 7)->score >= 3;
}

static void g0_metrics_values(MpPlayer * current, int * values) {
	values[0] = MP_CHECK(current, 5)->score;
}

static const MpGame g0 = {
	.name = "Contador",
	.endAfter = MP_DEFAULT_END_AFTER,
	.clockwise = true,
	.strategyCount = 1,
	.strategyNames = g0_strategies,
	.metricCount = 1,
	.metricNames = g0_metrics,
	.turn = g0_turn,
	.win = g0_win,
	.metrics = g0_metrics_values
};

/* game "Ronda" */

static bool g1_win(MpPlayer * current) {
	return MP_CHECK(current, 12)->index == 1;
}

static const MpGame g1 = {
	.name = "Ronda",
	.endAfter = MP_DEFAULT_END_AFTER,
	.clockwise = false,
	.win = g1_win
};

/* game "Infinito" */

static const MpGame g2 = {
	.name = "Infinito",
	.endAfter = MP_DEFAULT_END_AFTER,
	.clockwise = true
};

/* game "Vacio" */

static const char * const g3_metrics[] = { "indice" };

static void g3_metrics_values(MpPlayer * current, int * values) {
	values[0] = MP_CHECK(current, 19)->index + 7;
}

static const MpGame g3 = {
	.name = "Vacio",
	.endAfter = 0,
	.clockwise = true,
	.metricCount = 1,
	.metricNames = g3_metrics,
	.metrics = g3_metrics_values
};

int main(void) {
	MpStats * _s = mp_simulate(&g0, 10, 2, 0, false);
	mp_report_winrate(_s);
	mp_report_winrate_by_player(_s);
	mp_report_winrate_by_strategy(_s);
	mp_report_metric(_s, MP_TURNS, MP_AVG);
	mp_report_metric(_s, MP_TURNS, MP_MIN);
	mp_report_metric(_s, MP_TURNS, MP_MAX);
	mp_report_metric(_s, 0, MP_AVG);
	mp_report_metric(_s, 0, MP_MIN);
	mp_report_metric(_s, 0, MP_MAX);
	mp_stats_destroy(_s);
	_s = mp_simulate(&g1, 4, 3, 5, false);
	mp_report_winrate_by_player(_s);
	mp_report_metric(_s, MP_TURNS, MP_AVG);
	mp_stats_destroy(_s);
	_s = mp_simulate(&g2, 3, 1, 0, false);
	mp_report_winrate(_s);
	mp_report_metric(_s, MP_TURNS, MP_AVG);
	mp_stats_destroy(_s);
	_s = mp_simulate(&g3, 2, 2, 0, false);
	mp_report_winrate(_s);
	mp_report_metric(_s, MP_TURNS, MP_AVG);
	mp_report_metric(_s, 0, MP_AVG);
	mp_stats_destroy(_s);
	return 0;
}
