#ifndef TYPE_HEADER
#define TYPE_HEADER

#include "../syntactic-analysis/AbstractSyntaxTree.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/**
 * Operaciones sobre los tipos de Meeple. El struct Type esta en
 * AbstractSyntaxTree.h porque es una anotacion del AST. Los tipos se pasan por
 * valor y no reservan memoria.
 *
 * ERROR_KIND es el tipo de una expresion que ya tiene un error reportado: los
 * chequeos de compatibilidad lo aceptan siempre, para no reportar errores en
 * cascada.
 */

/** Un tipo sin arreglo ni cardtype: integer, player, board, etc. */
Type makeType(const TypeKind kind);

/** Una carta del cardtype indicado. */
Type makeCardType(const char * cardType);

/** Un "deck of T", con T el cardtype indicado. */
Type makeDeckType(const char * cardType);

/** T[], a partir de T. */
Type makeArrayType(const Type element);

/** El tipo de los elementos de un T[] o de un "deck of T". */
Type elementType(const Type collection);

bool isErrorType(const Type type);

/** T[] o "deck of T": se puede recorrer, indexar y agregar. */
bool isCollectionType(const Type type);

/** player, piece o una carta (no arreglos): son los que admiten none. */
bool isReferenceType(const Type type);

/** El mismo tipo exacto. No trata a ERROR_KIND como compatible. */
bool isSameType(const Type left, const Type right);

/**
 * Un valor de tipo "value" se puede guardar en un lugar de tipo "target": el
 * mismo tipo, none en una referencia, o un "deck of T" en un T[] (se copia).
 */
bool isAssignableType(const Type target, const Type value);

/**
 * Operandos validos de == y !=: el mismo tipo (no arreglos ni tipos internos,
 * salvo los mazos), o una referencia contra none.
 */
bool isComparableType(const Type left, const Type right);

/**
 * Se puede interpolar en un log: integer, boolean, string, player, piece, una
 * carta o strategy.
 */
bool isPrintableType(const Type type);

/** Escribe el nombre del tipo en buffer, para los mensajes ("Carta[]", "deck of Carta"). */
const char * typeName(const Type type, char * buffer, const size_t size);

#endif
