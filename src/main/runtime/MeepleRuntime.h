#ifndef MEEPLE_RUNTIME_HEADER
#define MEEPLE_RUNTIME_HEADER

/**
 * Runtime de Meeple. El programa en C que emite el compilador incluye este
 * header y se compila junto con MeepleRuntime.c (C99 con extensiones GNU). No
 * se compila con el compilador: es un asset que se copia junto al programa
 * generado, asi que se puede cambiar sin reconstruir el compilador.
 *
 * Solo usa la biblioteca estandar. Todo lo del runtime lleva el prefijo
 * "mp_", "Mp" o "MP_", para no chocar con los nombres que genera el
 * compilador.
 *
 * Memoria: todo lo que se crea durante una partida (jugadores, fichas, cartas,
 * mazos, arreglos, copias) sale de una arena que se vacia al empezar la
 * partida siguiente. El codigo generado nunca libera nada. Solo las
 * estadisticas de un simulate (MpStats) viven fuera de la arena.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/** Cota de turnos de un game sin "end after". */
#define MP_DEFAULT_END_AFTER 1000

/** La estrategia de un jugador sin "uses": resuelve todo con "prefer first". */
#define MP_NO_STRATEGY 0

/* VALORES Y LISTAS */

/**
 * Un valor de Meeple: integer (i), boolean (b), string (s), o una referencia
 * (p) a un jugador, una ficha o una carta. "none" es la referencia NULL. El
 * analisis semantico ya garantizo los tipos, asi que el valor no los guarda.
 */
typedef union {
	int i;
	bool b;
	const char * s;
	void * p;
} MpValue;

#define MP_INTEGER(value) ((MpValue) { .i = (value) })
#define MP_BOOLEAN(value) ((MpValue) { .b = (value) })
#define MP_STRING(value) ((MpValue) { .s = (value) })
#define MP_REFERENCE(value) ((MpValue) { .p = (value) })

/**
 * Mazos y arreglos son la misma lista. El tope de un mazo es la posicion 0.
 */
typedef struct {
	MpValue * items;
	int size;
	int capacity;
} MpList;

MpList * mp_list_new(void);
MpList * mp_list_copy(const MpList * list);
void mp_list_push(MpList * list, MpValue value);

/** "a[i]": error de ejecucion si i esta fuera de rango. */
MpValue mp_list_get(const MpList * list, int index, int line);
void mp_list_set(MpList * list, int index, MpValue value, int line);

/* COMPONENTES */

typedef struct MpPlayer {
	const char * name;
	int index;
	int score;
	int strategy;
	/* Las fichas "per player" del jugador, en el orden en que se declaran. */
	MpList * pieces;
} MpPlayer;

typedef struct MpPiece {
	const char * name;
	/* NULL si la ficha no es "per player". */
	MpPlayer * owner;
	int cell;
} MpPiece;

typedef struct MpDie {
	const int * faces;
	int count;
	int min;
	int max;
} MpDie;

/** "piece f;": una ficha sin dueno, en la celda 0. */
MpPiece * mp_piece_new(const char * name);

/**
 * "piece f per player;": una ficha por jugador, en la celda 0, indexadas por
 * el indice del jugador. Cada una se agrega a las "pieces" de su dueno.
 */
MpPiece * mp_piece_per_player(const char * name);

/** "deck M of T per player;": un mazo vacio por jugador. */
MpList ** mp_deck_per_player(void);

/** "die.faces". */
MpList * mp_die_faces(const MpDie * die);

/* AZAR */

/** Un entero en [0, bound), con el generador de la simulacion (bound > 0). */
int mp_random(int bound);

/** "roll D". */
int mp_roll(const MpDie * die);

/* ACCIONES */

/** "board.cell(i)": error de ejecucion si i esta fuera del tablero. */
int mp_cell(int cell, int line);

/** "place f on board.cell(i)". La celda ya paso por mp_cell. */
void mp_place(MpPiece * piece, int cell, int line);

/** "move f forward n": se detiene en la primera o en la ultima celda. */
void mp_move(MpPiece * piece, int steps, int line);

/** "shuffle M": Fisher-Yates con el generador de la simulacion. */
void mp_shuffle(MpList * deck);

/** "draw n from A to B": pasa las primeras min(n, A.size) cartas al final de B. */
void mp_draw(int count, MpList * from, MpList * to);

/** "play c from A to B": si c esta en A, la pasa al final de B; si no, nada. */
void mp_play(void * card, MpList * from, MpList * to);

/** "give n to p" y "take n from p". */
void mp_give(MpPlayer * player, int amount, int line);
void mp_take(MpPlayer * player, int amount, int line);

/** "p uses S". */
void mp_uses(MpPlayer * player, int strategy, int line);

/* LOG Y DESCRIPCIONES */

/**
 * Escribe un valor como lo muestran un "log" y un "prefer input". Cada
 * cardtype tiene la suya, que emite el compilador.
 */
typedef void (*MpDescribe)(FILE * out, MpValue value);

void mp_describe_integer(FILE * out, MpValue value);
void mp_describe_boolean(FILE * out, MpValue value);
void mp_describe_string(FILE * out, MpValue value);
void mp_describe_player(FILE * out, MpValue value);
void mp_describe_piece(FILE * out, MpValue value);
void mp_describe_strategy(FILE * out, MpValue value);

/**
 * Un "log" es una secuencia de llamadas: texto, valores interpolados y fin de
 * linea. Los argumentos se evaluan siempre, haya "verbose" o no, pero solo se
 * imprimen con "verbose". Asi una simulacion da lo mismo en los dos modos,
 * aunque una interpolacion tire un dado.
 */
void mp_log_text(const char * text);
void mp_log_value(MpDescribe describe, MpValue value);
void mp_log_end(void);

/* DECISIONES */

/** El "where" de una politica. */
typedef bool (*MpFilter)(MpValue option, MpPlayer * current);

/** La expresion de un criterio "max e" o "min e". */
typedef int (*MpKey)(MpValue option, MpPlayer * current);

typedef struct {
	MpKey key;
	bool maximize;
} MpRanking;

/** El criterio final de una politica. Sin criterio final, MP_FIRST. */
typedef enum {
	MP_FIRST,
	MP_RANDOM,
	MP_INPUT
} MpFinal;

typedef struct {
	/* NULL: sin "where". */
	MpFilter where;
	const MpRanking * rankings;
	int rankingCount;
	MpFinal final;
} MpPolicy;

/**
 * Una "decision": su nombre, si sus opciones son referencias (entonces "ask"
 * puede devolver none), como se muestran, y una politica por estrategia,
 * indexada por la estrategia del jugador. La posicion MP_NO_STRATEGY es
 * "prefer first".
 */
typedef struct {
	const char * name;
	bool reference;
	MpDescribe describe;
	const MpPolicy * policies;
} MpDecision;

/** "ask P for d(C)". */
MpValue mp_ask(const MpDecision * decision, MpPlayer * player, const MpList * options, int line);

/* SIMULACION Y REPORTE */

/**
 * Lo que emite el compilador por cada game. Las funciones que el game no
 * declara van en NULL.
 */
typedef struct {
	const char * name;
	/* 0 si el game no declara board. */
	int boardSize;
	int endAfter;
	bool clockwise;
	/* Las estrategias van de 1 a strategyCount: strategyNames[s - 1]. */
	int strategyCount;
	const char * const * strategyNames;
	int metricCount;
	const char * const * metricNames;
	/* Arma los componentes y las variables del juego al empezar cada partida. */
	void (*reset)(void);
	void (*prepare)(void);
	void (*turn)(MpPlayer * current);
	bool (*win)(MpPlayer * current);
	/* Evalua las "metric", en orden, con el ultimo jugador que jugo. */
	void (*metrics)(MpPlayer * current, int * values);
} MpGame;

/** Contadores de un simulate: victorias, turnos y, por metrica, suma, minimo y maximo. */
typedef struct MpStats MpStats;

/** El "turns" de "avg turns" en mp_report_metric; las metricas van de 0 en adelante. */
#define MP_TURNS (-1)

typedef enum {
	MP_AVG,
	MP_MIN,
	MP_MAX
} MpAggregator;

/** "simulate N games of G with K players seed S [verbose];". Sin seed, 0. */
MpStats * mp_simulate(const MpGame * game, int games, int players, int seed, bool verbose);

void mp_report_winrate(const MpStats * stats);
void mp_report_winrate_by_player(const MpStats * stats);
void mp_report_winrate_by_strategy(const MpStats * stats);
void mp_report_metric(const MpStats * stats, int metric, MpAggregator aggregator);
void mp_stats_destroy(MpStats * stats);

/* ESTADO DE LA PARTIDA EN CURSO */

extern MpPlayer * mp_players;
/* Los mismos jugadores, como lista: "players". */
extern MpList * mp_player_list;
extern int mp_player_count;
extern int mp_turns;
extern int mp_board_size;
extern bool mp_verbose;

/* MEMORIA */

/** Memoria de la partida en curso, en cero. Se libera sola al terminar. */
void * mp_alloc(size_t bytes);

/* ERRORES DE EJECUCION */

/**
 * Imprime "Error de ejecucion en la linea N: <mensaje>." por stderr y termina
 * el programa con codigo 1, sin imprimir el report.
 */
void mp_error(int line, const char * format, ...) __attribute__((noreturn, format(printf, 2, 3)));

/** Acceso a un miembro: error de ejecucion si la referencia es none. */
void * mp_check(void * reference, int line);
#define MP_CHECK(reference, line) ((__typeof__(reference)) mp_check((reference), (line)))

/** "/" y "%": error de ejecucion si el divisor es 0. */
int mp_div(int dividend, int divisor, int line);
int mp_mod(int dividend, int divisor, int line);

#endif
