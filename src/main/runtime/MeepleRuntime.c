#include "MeepleRuntime.h"
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>

/* ESTADO */

MpPlayer * mp_players = NULL;
MpList * mp_player_list = NULL;
int mp_player_count = 0;
int mp_turns = 0;
int mp_board_size = 0;
bool mp_verbose = false;

/* El game que se esta simulando y el numero de la partida en curso (desde 1). */
static const MpGame * _game = NULL;
static int _gameNumber = 0;

/* Cuantos simulate se imprimieron: a partir del segundo, van separados. */
static int _simulations = 0;

/* ERRORES */

void mp_error(const int line, const char * const format, ...) {
	fflush(stdout);
	fprintf(stderr, "Error de ejecucion en la linea %d: ", line);
	va_list arguments;
	va_start(arguments, format);
	vfprintf(stderr, format, arguments);
	va_end(arguments);
	fputs(".\n", stderr);
	exit(1);
}

/** Lo que no depende del programa Meeple: el runtime se quedo sin memoria. */
static void _abort(const char * const message) {
	fflush(stdout);
	fprintf(stderr, "Error del runtime: %s.\n", message);
	exit(1);
}

void * mp_check(void * reference, const int line) {
	if (reference == NULL) {
		mp_error(line, "acceso a un miembro de none");
	}
	return reference;
}

/*
 * Una accion sobre none: el mensaje nombra la accion ("give a none"). Es el
 * mismo error que el acceso a un miembro, con un mensaje mas claro.
 */
static void * _require(void * reference, const int line, const char * const action) {
	if (reference == NULL) {
		mp_error(line, "%s none", action);
	}
	return reference;
}

int mp_div(const int dividend, const int divisor, const int line) {
	if (divisor == 0) {
		mp_error(line, "division por cero");
	}
	/* INT_MIN / -1 no entra en un int (y en x86 aborta): da la vuelta como el resto de los desbordes. */
	if (divisor == -1) {
		return (int) (0u - (unsigned) dividend);
	}
	return dividend / divisor;
}

int mp_mod(const int dividend, const int divisor, const int line) {
	if (divisor == 0) {
		mp_error(line, "modulo por cero");
	}
	if (divisor == -1) {
		return 0;
	}
	return dividend % divisor;
}

/* MEMORIA */

/*
 * La arena es una lista de bloques. Al empezar cada partida se vacian todos y
 * se reusan, asi que la memoria no crece con la cantidad de partidas. Un pedido
 * mas grande que un bloque tiene su propio bloque.
 */
/* max_align_t es de C11: en C99 hace falta uno propio. */
typedef union {
	long long integer;
	long double real;
	void * pointer;
	void (*function)(void);
} MaxAlign;

typedef struct Chunk {
	struct Chunk * next;
	size_t size;
	size_t used;
	MaxAlign data[];
} Chunk;

#define CHUNK_SIZE ((size_t) 64 * 1024)

static Chunk * _chunks = NULL;
static Chunk * _cursor = NULL;

static void _arenaReset(void) {
	for (Chunk * chunk = _chunks; chunk != NULL; chunk = chunk->next) {
		chunk->used = 0;
	}
	_cursor = _chunks;
}

static void _arenaRelease(void) {
	while (_chunks != NULL) {
		Chunk * next = _chunks->next;
		free(_chunks);
		_chunks = next;
	}
	_cursor = NULL;
}

void * mp_alloc(const size_t bytes) {
	const size_t alignment = __alignof__(MaxAlign);
	const size_t size = bytes == 0 ? alignment : (bytes + alignment - 1) / alignment * alignment;
	while (_cursor != NULL && _cursor->size - _cursor->used < size) {
		_cursor = _cursor->next;
	}
	if (_cursor == NULL) {
		const size_t capacity = size > CHUNK_SIZE ? size : CHUNK_SIZE;
		Chunk * chunk = malloc(sizeof(Chunk) + capacity);
		if (chunk == NULL) {
			_abort("no hay memoria");
		}
		chunk->size = capacity;
		chunk->used = 0;
		chunk->next = NULL;
		if (_chunks == NULL) {
			_chunks = chunk;
		}
		else {
			Chunk * last = _chunks;
			while (last->next != NULL) {
				last = last->next;
			}
			last->next = chunk;
		}
		_cursor = chunk;
	}
	void * memory = (char *) _cursor->data + _cursor->used;
	_cursor->used += size;
	memset(memory, 0, size);
	return memory;
}

/* LISTAS */

MpList * mp_list_new(void) {
	return mp_alloc(sizeof(MpList));
}

MpList * mp_list_copy(const MpList * list) {
	MpList * copy = mp_list_new();
	if (list->size > 0) {
		copy->items = mp_alloc(list->size * sizeof(MpValue));
		memcpy(copy->items, list->items, list->size * sizeof(MpValue));
		copy->size = list->size;
		copy->capacity = list->size;
	}
	return copy;
}

void mp_list_push(MpList * list, const MpValue value) {
	if (list->size == list->capacity) {
		/* El bloque viejo queda en la arena hasta que termina la partida. */
		const int capacity = list->capacity == 0 ? 8 : 2 * list->capacity;
		MpValue * items = mp_alloc(capacity * sizeof(MpValue));
		if (list->size > 0) {
			memcpy(items, list->items, list->size * sizeof(MpValue));
		}
		list->items = items;
		list->capacity = capacity;
	}
	list->items[list->size++] = value;
}

static void _checkIndex(const MpList * list, const int index, const int line) {
	if (index < 0 || index >= list->size) {
		mp_error(line, "el indice %d esta fuera de rango: hay %d %s", index, list->size,
			list->size == 1 ? "elemento" : "elementos");
	}
}

MpValue mp_list_get(const MpList * list, const int index, const int line) {
	_checkIndex(list, index, line);
	return list->items[index];
}

void mp_list_set(MpList * list, const int index, const MpValue value, const int line) {
	_checkIndex(list, index, line);
	list->items[index] = value;
}

static void _removeAt(MpList * list, const int index) {
	memmove(&list->items[index], &list->items[index + 1], (list->size - index - 1) * sizeof(MpValue));
	--list->size;
}

/* AZAR */

/*
 * splitmix64 (Steele, Lea y Flood, 2014), y no rand(): rand() cambia entre
 * bibliotecas de C, y la misma semilla tiene que dar lo mismo en todos lados.
 */
static uint64_t _state = 0;

static uint64_t _next(void) {
	uint64_t z = (_state += UINT64_C(0x9E3779B97F4A7C15));
	z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
	z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
	return z ^ (z >> 31);
}

int mp_random(const int bound) {
	/* Rechaza la cola que no completa un multiplo de bound, para no sesgar el resultado. */
	const uint64_t range = (uint64_t) bound;
	const uint64_t limit = UINT64_MAX - UINT64_MAX % range;
	uint64_t value;
	do {
		value = _next();
	} while (value >= limit);
	return (int) (value % range);
}

int mp_roll(const MpDie * die) {
	return die->faces[mp_random(die->count)];
}

/* COMPONENTES */

MpPiece * mp_piece_new(const char * name) {
	MpPiece * piece = mp_alloc(sizeof(MpPiece));
	piece->name = name;
	return piece;
}

MpPiece * mp_piece_per_player(const char * name) {
	MpPiece * pieces = mp_alloc(mp_player_count * sizeof(MpPiece));
	for (int k = 0; k < mp_player_count; ++k) {
		pieces[k].name = name;
		pieces[k].owner = &mp_players[k];
		mp_list_push(mp_players[k].pieces, MP_REFERENCE(&pieces[k]));
	}
	return pieces;
}

MpList ** mp_deck_per_player(void) {
	MpList ** decks = mp_alloc(mp_player_count * sizeof(MpList *));
	for (int k = 0; k < mp_player_count; ++k) {
		decks[k] = mp_list_new();
	}
	return decks;
}

MpList * mp_die_faces(const MpDie * die) {
	MpList * faces = mp_list_new();
	for (int k = 0; k < die->count; ++k) {
		mp_list_push(faces, MP_INTEGER(die->faces[k]));
	}
	return faces;
}

/* ACCIONES */

int mp_cell(const int cell, const int line) {
	if (cell < 0 || cell >= mp_board_size) {
		mp_error(line, "board.cell(%d) esta fuera del tablero: las celdas van de 0 a %d", cell, mp_board_size - 1);
	}
	return cell;
}

void mp_place(MpPiece * piece, const int cell, const int line) {
	((MpPiece *) _require(piece, line, "place de"))->cell = cell;
}

void mp_move(MpPiece * piece, const int steps, const int line) {
	_require(piece, line, "move de");
	const long long cell = (long long) piece->cell + steps;
	piece->cell = cell < 0 ? 0 : cell >= mp_board_size ? mp_board_size - 1 : (int) cell;
}

void mp_shuffle(MpList * deck) {
	for (int k = deck->size - 1; k > 0; --k) {
		const int other = mp_random(k + 1);
		const MpValue card = deck->items[k];
		deck->items[k] = deck->items[other];
		deck->items[other] = card;
	}
}

void mp_draw(const int count, MpList * from, MpList * to) {
	/* Carta por carta: "draw 1 from M to M" pasa el tope al fondo. */
	const int cards = count < from->size ? count : from->size;
	for (int k = 0; k < cards; ++k) {
		const MpValue card = from->items[0];
		_removeAt(from, 0);
		mp_list_push(to, card);
	}
}

void mp_play(void * card, MpList * from, MpList * to) {
	for (int k = 0; k < from->size; ++k) {
		if (from->items[k].p == card) {
			_removeAt(from, k);
			mp_list_push(to, MP_REFERENCE(card));
			return;
		}
	}
}

void mp_give(MpPlayer * player, const int amount, const int line) {
	((MpPlayer *) _require(player, line, "give a"))->score += amount;
}

void mp_take(MpPlayer * player, const int amount, const int line) {
	((MpPlayer *) _require(player, line, "take de"))->score -= amount;
}

void mp_uses(MpPlayer * player, const int strategy, const int line) {
	((MpPlayer *) _require(player, line, "uses sobre"))->strategy = strategy;
}

/* LOG Y DESCRIPCIONES */

void mp_describe_integer(FILE * out, const MpValue value) {
	fprintf(out, "%d", value.i);
}

void mp_describe_boolean(FILE * out, const MpValue value) {
	fputs(value.b ? "true" : "false", out);
}

void mp_describe_string(FILE * out, const MpValue value) {
	fputs(value.s, out);
}

void mp_describe_player(FILE * out, const MpValue value) {
	const MpPlayer * player = value.p;
	fputs(player == NULL ? "none" : player->name, out);
}

void mp_describe_piece(FILE * out, const MpValue value) {
	const MpPiece * piece = value.p;
	if (piece == NULL) {
		fputs("none", out);
	}
	else if (piece->owner == NULL) {
		fprintf(out, "%s (celda %d)", piece->name, piece->cell);
	}
	else {
		fprintf(out, "%s de %s (celda %d)", piece->name, piece->owner->name, piece->cell);
	}
}

static const char * _strategyName(const MpGame * game, const int strategy) {
	return strategy == MP_NO_STRATEGY ? "(sin estrategia)" : game->strategyNames[strategy - 1];
}

void mp_describe_strategy(FILE * out, const MpValue value) {
	fputs(_strategyName(_game, value.i), out);
}

void mp_log_text(const char * text) {
	if (mp_verbose) {
		fputs(text, stdout);
	}
}

void mp_log_value(const MpDescribe describe, const MpValue value) {
	if (mp_verbose) {
		describe(stdout, value);
	}
}

void mp_log_end(void) {
	if (mp_verbose) {
		putchar('\n');
	}
}

/* DECISIONES */

/*
 * "prefer input": muestra las opciones numeradas desde 1 y lee un numero por
 * stdin hasta que sea valido. Se muestra aunque no haya "verbose".
 */
static int _input(const MpDecision * decision, const MpPlayer * player, const MpValue * options, const int count, const int line) {
	printf("[partida %d, turno %d] %s decide %s:\n", _gameNumber, mp_turns, player->name, decision->name);
	for (int k = 0; k < count; ++k) {
		printf("  %d) ", k + 1);
		decision->describe(stdout, options[k]);
		putchar('\n');
	}
	char buffer[64];
	while (true) {
		printf("opcion (1 a %d): ", count);
		fflush(stdout);
		if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
			putchar('\n');
			mp_error(line, "la decision \"%s\" espera una opcion por la entrada estandar, y la entrada se termino", decision->name);
		}
		/* Una linea mas larga que el buffer se descarta entera, y es invalida. */
		const size_t length = strlen(buffer);
		if (length == sizeof(buffer) - 1 && buffer[length - 1] != '\n') {
			int character;
			do {
				character = getchar();
			} while (character != '\n' && character != EOF);
			buffer[0] = 'x';
		}
		char * end;
		const long option = strtol(buffer, &end, 10);
		while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') {
			++end;
		}
		if (end != buffer && *end == '\0' && 1 <= option && option <= count) {
			return (int) option - 1;
		}
		printf("opcion invalida: tiene que ser un numero del 1 al %d\n", count);
	}
}

MpValue mp_ask(const MpDecision * decision, MpPlayer * player, const MpList * options, const int line) {
	_require(player, line, "ask a");
	const MpPolicy * policy = &decision->policies[player->strategy];

	/* Las opciones son una copia de C: la politica no ve lo que pase despues. */
	MpValue * candidates = mp_alloc((options->size > 0 ? options->size : 1) * sizeof(MpValue));
	int count = 0;
	for (int k = 0; k < options->size; ++k) {
		if (policy->where == NULL || policy->where(options->items[k], player)) {
			candidates[count++] = options->items[k];
		}
	}
	if (count == 0) {
		if (decision->reference) {
			return MP_REFERENCE(NULL);
		}
		mp_error(line, "la decision \"%s\" no tiene opciones entre las que elegir", decision->name);
	}

	/* Cada criterio se queda con las que empatan en el mejor valor, en orden. */
	int * keys = mp_alloc(count * sizeof(int));
	for (int r = 0; r < policy->rankingCount && count > 1; ++r) {
		const MpRanking * ranking = &policy->rankings[r];
		int best = 0;
		for (int k = 0; k < count; ++k) {
			keys[k] = ranking->key(candidates[k], player);
			if (k == 0 || (ranking->maximize ? keys[k] > best : keys[k] < best)) {
				best = keys[k];
			}
		}
		int kept = 0;
		for (int k = 0; k < count; ++k) {
			if (keys[k] == best) {
				candidates[kept++] = candidates[k];
			}
		}
		count = kept;
	}

	switch (policy->final) {
		case MP_RANDOM:
			return candidates[mp_random(count)];
		case MP_INPUT:
			return candidates[_input(decision, player, candidates, count, line)];
		case MP_FIRST:
		default:
			return candidates[0];
	}
}

/* SIMULACION */

struct MpStats {
	const MpGame * game;
	int games;
	int players;
	int gamesWon;
	int * winsByPlayer;
	/* Por estrategia, de 0 (sin estrategia) a strategyCount. */
	int * winsByStrategy;
	int * usersByStrategy;
	long long turnsSum;
	int turnsMin;
	int turnsMax;
	long long * metricSum;
	int * metricMin;
	int * metricMax;
	/* Todas las estadisticas vivas: un error de ejecucion las deja alcanzables. */
	MpStats * next;
};

static MpStats * _allStats = NULL;

static void * _allocate(const size_t count, const size_t size) {
	void * memory = calloc(count > 0 ? count : 1, size);
	if (memory == NULL) {
		_abort("no hay memoria");
	}
	return memory;
}

static MpStats * _createStats(const MpGame * game, const int games, const int players) {
	MpStats * stats = _allocate(1, sizeof(MpStats));
	stats->game = game;
	stats->games = games;
	stats->players = players;
	stats->winsByPlayer = _allocate(players, sizeof(int));
	stats->winsByStrategy = _allocate(game->strategyCount + 1, sizeof(int));
	stats->usersByStrategy = _allocate(game->strategyCount + 1, sizeof(int));
	stats->turnsMin = INT_MAX;
	stats->turnsMax = INT_MIN;
	stats->metricSum = _allocate(game->metricCount, sizeof(long long));
	stats->metricMin = _allocate(game->metricCount, sizeof(int));
	stats->metricMax = _allocate(game->metricCount, sizeof(int));
	for (int m = 0; m < game->metricCount; ++m) {
		stats->metricMin[m] = INT_MAX;
		stats->metricMax[m] = INT_MIN;
	}
	stats->next = _allStats;
	_allStats = stats;
	return stats;
}

void mp_stats_destroy(MpStats * stats) {
	if (stats == NULL) {
		return;
	}
	for (MpStats ** link = &_allStats; *link != NULL; link = &(*link)->next) {
		if (*link == stats) {
			*link = stats->next;
			break;
		}
	}
	free(stats->winsByPlayer);
	free(stats->winsByStrategy);
	free(stats->usersByStrategy);
	free(stats->metricSum);
	free(stats->metricMin);
	free(stats->metricMax);
	free(stats);
}

/* K jugadores con indice 0 a K-1, "Jugador 1" a "Jugador K", score 0 y sin estrategia. */
static void _createPlayers(const int count) {
	mp_player_count = count;
	mp_players = mp_alloc(count * sizeof(MpPlayer));
	mp_player_list = mp_list_new();
	for (int k = 0; k < count; ++k) {
		char * name = mp_alloc(32);
		snprintf(name, 32, "Jugador %d", k + 1);
		mp_players[k].name = name;
		mp_players[k].index = k;
		mp_players[k].pieces = mp_list_new();
		mp_list_push(mp_player_list, MP_REFERENCE(&mp_players[k]));
	}
}

static void _record(MpStats * stats, const MpPlayer * winner, const int * values) {
	if (winner != NULL) {
		++stats->gamesWon;
		++stats->winsByPlayer[winner->index];
		++stats->winsByStrategy[winner->strategy];
	}
	for (int k = 0; k < mp_player_count; ++k) {
		++stats->usersByStrategy[mp_players[k].strategy];
	}
	stats->turnsSum += mp_turns;
	stats->turnsMin = mp_turns < stats->turnsMin ? mp_turns : stats->turnsMin;
	stats->turnsMax = mp_turns > stats->turnsMax ? mp_turns : stats->turnsMax;
	for (int m = 0; m < stats->game->metricCount; ++m) {
		stats->metricSum[m] += values[m];
		stats->metricMin[m] = values[m] < stats->metricMin[m] ? values[m] : stats->metricMin[m];
		stats->metricMax[m] = values[m] > stats->metricMax[m] ? values[m] : stats->metricMax[m];
	}
}

/*
 * Una partida: estado inicial, prepare, y turnos hasta que current gana o se
 * llega a la cota. Devuelve el ganador, o NULL.
 */
static MpPlayer * _play(const MpGame * game, MpPlayer ** last) {
	MpPlayer * current = &mp_players[0];
	*last = current;
	if (game->reset != NULL) {
		game->reset();
	}
	if (game->prepare != NULL) {
		game->prepare();
	}
	while (mp_turns < game->endAfter) {
		++mp_turns;
		if (game->turn != NULL) {
			game->turn(current);
		}
		*last = current;
		if (game->win != NULL && game->win(current)) {
			return current;
		}
		const int step = game->clockwise ? 1 : mp_player_count - 1;
		current = &mp_players[(current->index + step) % mp_player_count];
	}
	return NULL;
}

MpStats * mp_simulate(const MpGame * game, const int games, const int players, const int seed, const bool verbose) {
	if (_simulations++ > 0) {
		putchar('\n');
	}
	printf("simulate %d %s of \"%s\" with %d players seed %d\n", games, games == 1 ? "game" : "games", game->name, players, seed);

	MpStats * stats = _createStats(game, games, players);
	_state = (uint64_t) (unsigned) seed;
	_game = game;
	mp_verbose = verbose;
	mp_board_size = game->boardSize;
	for (_gameNumber = 1; _gameNumber <= games; ++_gameNumber) {
		_arenaReset();
		_createPlayers(players);
		mp_turns = 0;
		if (verbose) {
			printf("-- partida %d --\n", _gameNumber);
		}
		MpPlayer * last;
		MpPlayer * winner = _play(game, &last);
		int * values = mp_alloc(game->metricCount * sizeof(int));
		if (game->metrics != NULL) {
			game->metrics(last, values);
		}
		if (verbose) {
			if (winner != NULL) {
				printf("gana %s en el turno %d\n", winner->name, mp_turns);
			}
			else {
				printf("sin ganador despues de %d turnos\n", mp_turns);
			}
		}
		_record(stats, winner, values);
	}
	_arenaRelease();
	mp_players = NULL;
	mp_player_list = NULL;
	mp_player_count = 0;
	return stats;
}

/* REPORTE */

static double _percentage(const int part, const int total) {
	return 100.0 * part / total;
}

void mp_report_winrate(const MpStats * stats) {
	printf("winrate: %.2f%%\n", _percentage(stats->gamesWon, stats->games));
}

void mp_report_winrate_by_player(const MpStats * stats) {
	puts("winrate by player");
	for (int k = 0; k < stats->players; ++k) {
		printf("  Jugador %d: %.2f%%\n", k + 1, _percentage(stats->winsByPlayer[k], stats->games));
	}
}

/*
 * Victorias de los jugadores que terminaron la partida con la estrategia,
 * sobre cuantos jugadores la tuvieron. "(sin estrategia)" aparece solo si
 * algun jugador termino sin "uses".
 */
void mp_report_winrate_by_strategy(const MpStats * stats) {
	puts("winrate by strategy");
	for (int s = 1; s <= stats->game->strategyCount; ++s) {
		const char * name = _strategyName(stats->game, s);
		if (stats->usersByStrategy[s] == 0) {
			printf("  %s: sin jugadores\n", name);
		}
		else {
			printf("  %s: %.2f%%\n", name, _percentage(stats->winsByStrategy[s], stats->usersByStrategy[s]));
		}
	}
	if (stats->usersByStrategy[MP_NO_STRATEGY] > 0) {
		printf("  %s: %.2f%%\n", _strategyName(stats->game, MP_NO_STRATEGY),
			_percentage(stats->winsByStrategy[MP_NO_STRATEGY], stats->usersByStrategy[MP_NO_STRATEGY]));
	}
}

void mp_report_metric(const MpStats * stats, const int metric, const MpAggregator aggregator) {
	const char * name = metric == MP_TURNS ? "turns" : stats->game->metricNames[metric];
	const long long sum = metric == MP_TURNS ? stats->turnsSum : stats->metricSum[metric];
	const int min = metric == MP_TURNS ? stats->turnsMin : stats->metricMin[metric];
	const int max = metric == MP_TURNS ? stats->turnsMax : stats->metricMax[metric];
	switch (aggregator) {
		case MP_AVG:
			printf("avg %s: %.2f\n", name, (double) sum / stats->games);
			break;
		case MP_MIN:
			printf("min %s: %d\n", name, min);
			break;
		case MP_MAX:
			printf("max %s: %d\n", name, max);
			break;
	}
}
