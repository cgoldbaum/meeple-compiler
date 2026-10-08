#include "SymbolTable.h"

typedef struct Scope Scope;

/* Los simbolos van en orden de declaracion: si un nombre se repite (que es
 * un error), la busqueda encuentra el primero y el repetido no lo tapa. */
struct Scope {
	Symbol * symbols;
	Symbol * last;
	Scope * parent;
};

struct SymbolTable {
	Scope * innermost;
};

/* PUBLIC FUNCTIONS */

SymbolTable * createSymbolTable() {
	return calloc(1, sizeof(SymbolTable));
}

void destroySymbolTable(SymbolTable * table) {
	if (table == NULL) {
		return;
	}
	while (table->innermost != NULL) {
		closeScope(table);
	}
	free(table);
}

void openScope(SymbolTable * table) {
	Scope * scope = calloc(1, sizeof(Scope));
	scope->parent = table->innermost;
	table->innermost = scope;
}

void closeScope(SymbolTable * table) {
	Scope * scope = table->innermost;
	if (scope == NULL) {
		return;
	}
	Symbol * symbol = scope->symbols;
	while (symbol != NULL) {
		Symbol * next = symbol->next;
		free(symbol);
		symbol = next;
	}
	table->innermost = scope->parent;
	free(scope);
}

Symbol * declareSymbol(SymbolTable * table, const char * name, const SymbolKind kind, const Type type, const int position, const int line) {
	Symbol * symbol = calloc(1, sizeof(Symbol));
	symbol->name = name;
	symbol->kind = kind;
	symbol->type = type;
	symbol->position = position;
	symbol->line = line;
	Scope * scope = table->innermost;
	if (scope->last == NULL) {
		scope->symbols = symbol;
	}
	else {
		scope->last->next = symbol;
	}
	scope->last = symbol;
	return symbol;
}

Symbol * lookupSymbol(const SymbolTable * table, const char * name, const int position) {
	for (Scope * scope = table->innermost; scope != NULL; scope = scope->parent) {
		for (Symbol * symbol = scope->symbols; symbol != NULL; symbol = symbol->next) {
			if (symbol->position <= position && strcmp(symbol->name, name) == 0) {
				return symbol;
			}
		}
	}
	return NULL;
}
