#include "Type.h"

/* PRIVATE FUNCTIONS */

static bool _isInternalKind(const TypeKind kind);
static bool _isSameName(const char * left, const char * right);

/**
 * board, cell, deck y die: existen en las expresiones, pero no se pueden
 * declarar ni comparar (salvo dos mazos del mismo cardtype).
 */
static bool _isInternalKind(const TypeKind kind) {
	return kind == BOARD_KIND || kind == CELL_KIND || kind == DECK_KIND || kind == DIE_KIND;
}

static bool _isSameName(const char * left, const char * right) {
	if (left == NULL || right == NULL) {
		return left == right;
	}
	return strcmp(left, right) == 0;
}

/* PUBLIC FUNCTIONS */

Type makeType(const TypeKind kind) {
	Type type = { .kind = kind, .isArray = false, .cardType = NULL };
	return type;
}

Type makeCardType(const char * cardType) {
	Type type = { .kind = CARD_KIND, .isArray = false, .cardType = cardType };
	return type;
}

Type makeDeckType(const char * cardType) {
	Type type = { .kind = DECK_KIND, .isArray = false, .cardType = cardType };
	return type;
}

Type makeArrayType(const Type element) {
	Type type = element;
	type.isArray = true;
	return type;
}

Type elementType(const Type collection) {
	if (collection.kind == DECK_KIND) {
		return makeCardType(collection.cardType);
	}
	Type type = collection;
	type.isArray = false;
	return type;
}

bool isErrorType(const Type type) {
	return type.kind == ERROR_KIND;
}

bool isCollectionType(const Type type) {
	return type.isArray || type.kind == DECK_KIND;
}

bool isReferenceType(const Type type) {
	return !type.isArray && (type.kind == PLAYER_KIND || type.kind == PIECE_KIND || type.kind == CARD_KIND);
}

bool isSameType(const Type left, const Type right) {
	if (left.kind != right.kind || left.isArray != right.isArray) {
		return false;
	}
	if (left.kind == CARD_KIND || left.kind == DECK_KIND) {
		return _isSameName(left.cardType, right.cardType);
	}
	return true;
}

bool isAssignableType(const Type target, const Type value) {
	if (isErrorType(target) || isErrorType(value)) {
		return true;
	}
	if (value.kind == NONE_KIND) {
		return isReferenceType(target);
	}
	if (value.kind == DECK_KIND) {
		return target.isArray && target.kind == CARD_KIND && _isSameName(target.cardType, value.cardType);
	}
	return isSameType(target, value);
}

bool isComparableType(const Type left, const Type right) {
	if (isErrorType(left) || isErrorType(right)) {
		return true;
	}
	if (left.isArray || right.isArray) {
		return false;
	}
	if (left.kind == NONE_KIND || right.kind == NONE_KIND) {
		const Type other = left.kind == NONE_KIND ? right : left;
		return other.kind == NONE_KIND || isReferenceType(other);
	}
	if (left.kind == DECK_KIND) {
		/* Los mazos son referencias: == compara si son el mismo mazo */
		return isSameType(left, right);
	}
	if (_isInternalKind(left.kind)) {
		return false;
	}
	return isSameType(left, right);
}

bool isPrintableType(const Type type) {
	if (isErrorType(type)) {
		return true;
	}
	if (type.isArray) {
		return false;
	}
	switch (type.kind) {
		case BOOLEAN_KIND:
		case CARD_KIND:
		case INTEGER_KIND:
		case PIECE_KIND:
		case PLAYER_KIND:
		case STRATEGY_KIND:
		case STRING_KIND:
			return true;
		default:
			return false;
	}
}

const char * typeName(const Type type, char * buffer, const size_t size) {
	const char * name = "?";
	switch (type.kind) {
		case UNCHECKED_KIND: name = "unchecked"; break;
		case ERROR_KIND: name = "error"; break;
		case BOOLEAN_KIND: name = "boolean"; break;
		case INTEGER_KIND: name = "integer"; break;
		case STRING_KIND: name = "string"; break;
		case PLAYER_KIND: name = "player"; break;
		case PIECE_KIND: name = "piece"; break;
		case CARD_KIND: name = type.cardType == NULL ? "?" : type.cardType; break;
		case STRATEGY_KIND: name = "strategy"; break;
		case NONE_KIND: name = "none"; break;
		case BOARD_KIND: name = "board"; break;
		case CELL_KIND: name = "cell"; break;
		case DIE_KIND: name = "die"; break;
		case DECK_KIND:
			if (type.cardType == NULL) {
				name = "deck";
				break;
			}
			snprintf(buffer, size, "deck of %s", type.cardType);
			return buffer;
	}
	snprintf(buffer, size, "%s%s", name, type.isArray ? "[]" : "");
	return buffer;
}
