#ifndef SYMBOL_TABLE_HEADER
#define SYMBOL_TABLE_HEADER

#include "../syntactic-analysis/AbstractSyntaxTree.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/**
 * Tabla de simbolos del analisis semantico: una pila de alcances, cada uno con
 * una lista de simbolos, y busqueda lineal (los programas son chicos). Vive
 * solo durante el analisis: el generador trabaja con el AST anotado.
 *
 * Los nombres apuntan al AST, que es su dueno: la tabla solo libera sus propios
 * nodos.
 */

typedef enum SymbolKind SymbolKind;
typedef struct Symbol Symbol;
typedef struct SymbolTable SymbolTable;

enum SymbolKind {
	CARD_TYPE_SYMBOL,
	DECISION_SYMBOL,
	DECK_SYMBOL,
	DIE_SYMBOL,
	GAME_VARIABLE_SYMBOL,
	LOCAL_SYMBOL,
	METRIC_SYMBOL,
	PIECE_SYMBOL,
	STRATEGY_SYMBOL
};

struct Symbol {
	const char * name;
	SymbolKind kind;
	/*
	 * El tipo del valor que nombra. En un cardtype es la carta; en una
	 * decision, el tipo de sus opciones (y de su resultado).
	 */
	Type type;
	/*
	 * Posicion del item que lo declara dentro del game. Las locales usan -1:
	 * siempre son visibles en su alcance.
	 */
	int position;
	int line;
	/* Mazos y fichas "per player". */
	bool perPlayer;
	/* Variables de un for o de una agregacion. */
	bool readOnly;
	Symbol * next;
};

SymbolTable * createSymbolTable();

/** Cierra los alcances que queden abiertos y libera la tabla. */
void destroySymbolTable(SymbolTable * table);

void openScope(SymbolTable * table);

/** Cierra el alcance mas interno y libera sus simbolos. */
void closeScope(SymbolTable * table);

/**
 * Agrega un simbolo al alcance mas interno. No chequea si el nombre ya existe:
 * eso lo decide quien llama, con lookupSymbol.
 */
Symbol * declareSymbol(SymbolTable * table, const char * name, const SymbolKind kind, const Type type, const int position, const int line);

/**
 * Busca el nombre desde el alcance mas interno hacia afuera, ignorando lo que
 * se declara despues de "position" (la posicion del item que se analiza).
 * NULL si no hay ningun simbolo visible con ese nombre.
 */
Symbol * lookupSymbol(const SymbolTable * table, const char * name, const int position);

#endif
