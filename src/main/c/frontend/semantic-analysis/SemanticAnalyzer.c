#include "SemanticAnalyzer.h"

/* Largo maximo del nombre de un tipo en los mensajes de error. */
#define TYPE_NAME_LENGTH 128

/**
 * Donde se usa un tipo declarado. Cada lugar admite tipos distintos.
 */
typedef enum {
	CARD_FIELD_TYPE_USE,
	DECISION_TYPE_USE,
	VARIABLE_TYPE_USE
} TypeUse;

/**
 * Lo que depende de donde esta la expresion que se analiza: que nombres
 * predefinidos estan en alcance, que parte del game se ve y que linea se
 * reporta.
 */
typedef struct {
	/* El game que se analiza, y el item y su posicion: lo declarado despues no se ve. */
	GameDeclaration * game;
	GameItem * item;
	int position;
	/* Linea de la sentencia o del item. Los nodos Expression no guardan la suya. */
	int line;
	/* Donde no se puede usar current ("prepare", ...); NULL si se puede. */
	const char * currentForbiddenIn;
	/* En una politica, option tiene el tipo de la decision y no se puede usar ask. */
	bool inPolicy;
	Type optionType;
} AnalysisContext;

/* MODULE INTERNAL STATE */

static AnalysisContext _context;
static unsigned int _errors = 0;
static Logger * _logger = NULL;
static SymbolTable * _table = NULL;

/** Shutdown module's internal state. */
void _shutdownSemanticAnalyzerModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: SemanticAnalyzer...");
		destroyLogger(_logger);
		_logger = NULL;
	}
	destroySymbolTable(_table);
	_table = NULL;
	_errors = 0;
}

ModuleDestructor initializeSemanticAnalyzerModule() {
	_logger = createLogger("SemanticAnalyzer");
	return _shutdownSemanticAnalyzerModule;
}

/* PRIVATE FUNCTIONS */

static void _error(const int line, const char * const format, ...);

static const char * _declaredName(GameItem * item);
static Symbol * _declare(const char * name, const SymbolKind kind, const Type type, const int position);
static Symbol * _declareLocal(const char * name, const Type type, const bool readOnly);
static GameItem * _findLaterDeclaration(const char * name);
static Symbol * _lookup(const char * name);
static void _reportUndeclared(const char * name);

static GameItem * _findCardType(const char * name);
static Field * _findField(GameItem * cardType, const char * name);
static GameDeclaration * _findGame(TopLevel * topLevels, const char * name);
static ValueSet * _findPlayers(GameItem * items);
static Policy * _findPolicy(Policy * policies, const char * decision);
static bool _hasMetric(GameDeclaration * game, const char * name);
static bool _isBoardDeclared();
static bool _isPlayerMember(const char * name);
static bool _valueSetContains(ValueSet * valueSet, const int value);

static Type _errorType();
static Type _fieldType(Field * field);
static bool _isKind(const Type type, const TypeKind kind);
static TypeKind _literalKind(Literal * literal);
static const char * _operatorName(const ExpressionType type);
static Type _resolveTypeSpec(TypeSpec * typeSpec, const TypeUse use, const char * name);

static Type _checkAggregation(Expression * expression);
static Type _checkAsk(Expression * expression);
static bool _checkAssignable(Expression * target);
static Type _checkBinary(Expression * expression, const TypeKind operand, const TypeKind result);
static Type _checkCall(Expression * expression);
static Type _checkEquality(Expression * expression);
static Type _checkExpression(Expression * expression);
static Type _checkIdentifier(Expression * expression);
static Type _checkIndex(Expression * expression);
static Type _checkMember(Expression * expression);
static Type _checkPlayerMember(Expression * expression);
static Type _checkRoll(Expression * expression);
static Type _checkUnary(Expression * expression, const TypeKind kind);
static Type _elementOf(const Type collection, const char * what);
static Type _expectDeck(Expression * expression, const char * action);
static Type _expectType(Expression * expression, const TypeKind kind, const char * what);
static Type _member(Expression * expression, const Resolution resolution, const Type type);

static void _checkAction(Statement * statement);
static void _checkAssignment(Statement * statement);
static void _checkDeclaration(VariableDeclaration * declaration);
static void _checkForEach(Statement * statement);
static void _checkForRange(Statement * statement);
static void _checkInitializer(VariableDeclaration * variable, const Type type);
static void _checkLog(LogPart * parts);
static void _checkStatement(Statement * statement);
static void _checkStatements(Statement * statement);
static void _checkUses(Statement * statement);

static void _checkCard(Card * card, GameItem * cardType);
static void _checkCardType(GameItem * item);
static void _checkDeck(GameItem * item);
static void _checkFieldValue(Field * field, const Type value, const int line);
static void _checkGame(GameDeclaration * game, const int line);
static void _checkGameItem(GameItem * item);
static void _checkGenerators(Generator * generators, GameItem * cardType);
static void _checkPlayers(GameItem * item);
static void _checkStrategy(GameItem * strategy);
static void _checkUniqueComponent(GameItem * item, const char * component);
static void _enterItem(GameItem * item, const int position);

static void _checkGameNames(TopLevel * topLevels);
static void _checkReport(ReportItem * report, const bool simulated, GameDeclaration * game, const int line);
static void _checkSimulation(Simulation * simulation, TopLevel * topLevels, const int line);
static void _checkSimulationsAndReports(TopLevel * topLevels);

/**
 * Reporta un error semantico y lo cuenta. Imita el formato de yyerror en
 * BisonGrammar.y ("Line N: mensaje."), pero pasando por el Logger.
 *
 * El analisis no aborta en el primer error, asi que un programa con varios
 * problemas los muestra todos en una sola corrida.
 */
static void _error(const int line, const char * const format, ...) {
	char message[512];
	va_list arguments;
	va_start(arguments, format);
	vsnprintf(message, sizeof(message), format, arguments);
	va_end(arguments);
	logError(_logger, "Line %d: %s.", line, message);
	++_errors;
}

/* NOMBRES Y ALCANCES */

/**
 * El nombre que declara un item del game, o NULL si el item no declara
 * ninguno (board, prepare, turn, etc.).
 */
static const char * _declaredName(GameItem * item) {
	switch (item->type) {
		case CARD_TYPE_ITEM: return item->cardType.name;
		case DECISION_ITEM: return item->decision.name;
		case DECK_ITEM: return item->deck->name;
		case DIE_ITEM: return item->die.name;
		case METRIC_ITEM: return item->metric.name;
		case PIECE_ITEM: return item->piece.name;
		case STRATEGY_ITEM: return item->strategy.name;
		case VARIABLE_ITEM: return item->variable->name;
		default: return NULL;
	}
}

/**
 * Busca el nombre visible desde donde se esta analizando: lo declarado en un
 * item posterior no se ve, aunque ya este en la tabla.
 */
static Symbol * _lookup(const char * name) {
	return lookupSymbol(_table, name, _context.position);
}

/**
 * Declara un nombre en el alcance mas interno. No se puede redeclarar un
 * nombre visible (no hay shadowing), pero se declara igual: los usos que
 * siguen encuentran el nuevo y no arrastran errores.
 */
static Symbol * _declare(const char * name, const SymbolKind kind, const Type type, const int position) {
	Symbol * previous = _lookup(name);
	if (previous != NULL) {
		_error(_context.line, "\"%s\" ya esta declarado en la linea %d", name, previous->line);
	}
	return declareSymbol(_table, name, kind, type, position, _context.line);
}

static Symbol * _declareLocal(const char * name, const Type type, const bool readOnly) {
	Symbol * symbol = _declare(name, LOCAL_SYMBOL, type, -1);
	symbol->readOnly = readOnly;
	return symbol;
}

/**
 * Busca un item del game, posterior al que se analiza, que declare ese nombre.
 * Distingue "no declarado" de "declarado despues" en los mensajes.
 */
static GameItem * _findLaterDeclaration(const char * name) {
	if (_context.item == NULL) {
		return NULL;
	}
	for (GameItem * item = _context.item->next; item != NULL; item = item->next) {
		const char * declared = _declaredName(item);
		if (declared != NULL && strcmp(declared, name) == 0) {
			return item;
		}
	}
	return NULL;
}

static void _reportUndeclared(const char * name) {
	GameItem * later = _findLaterDeclaration(name);
	if (later == NULL) {
		_error(_context.line, "\"%s\" no esta declarado", name);
	}
	else {
		_error(_context.line, "\"%s\" se usa antes de su declaracion, en la linea %d", name, later->line);
	}
}

/* BUSQUEDAS EN EL AST */

static GameItem * _findCardType(const char * name) {
	if (name == NULL) {
		return NULL;
	}
	for (GameItem * item = _context.game->items; item != NULL; item = item->next) {
		if (item->type == CARD_TYPE_ITEM && strcmp(item->cardType.name, name) == 0) {
			return item;
		}
	}
	return NULL;
}

static Field * _findField(GameItem * cardType, const char * name) {
	for (Field * field = cardType->cardType.fields; field != NULL; field = field->next) {
		if (strcmp(field->name, name) == 0) {
			return field;
		}
	}
	return NULL;
}

/**
 * Busca el game declarado con ese nombre en todo el programa, sin importar si
 * aparece antes o despues del simulate. NULL si no existe.
 */
static GameDeclaration * _findGame(TopLevel * topLevels, const char * name) {
	for (TopLevel * topLevel = topLevels; topLevel != NULL; topLevel = topLevel->next) {
		if (topLevel->type == GAME_TOP_LEVEL && strcmp(topLevel->game->name, name) == 0) {
			return topLevel->game;
		}
	}
	return NULL;
}

/**
 * Devuelve el conjunto de cantidades de jugadores que admite el game, o NULL si
 * el game no declara "players".
 */
static ValueSet * _findPlayers(GameItem * items) {
	for (GameItem * item = items; item != NULL; item = item->next) {
		if (item->type == PLAYERS_ITEM) {
			return item->players;
		}
	}
	return NULL;
}

/**
 * Busca la primera politica que resuelve una decision dentro de una strategy.
 * NULL si la strategy no la resuelve.
 */
static Policy * _findPolicy(Policy * policies, const char * decision) {
	for (Policy * policy = policies; policy != NULL; policy = policy->next) {
		if (strcmp(policy->decision, decision) == 0) {
			return policy;
		}
	}
	return NULL;
}

static bool _hasMetric(GameDeclaration * game, const char * name) {
	for (GameItem * item = game->items; item != NULL; item = item->next) {
		if (item->type == METRIC_ITEM && strcmp(item->metric.name, name) == 0) {
			return true;
		}
	}
	return false;
}

/**
 * "board" solo existe si el game lo declara antes del item que se analiza.
 */
static bool _isBoardDeclared() {
	for (GameItem * item = _context.game->items; item != NULL && item != _context.item; item = item->next) {
		if (item->type == BOARD_ITEM) {
			return true;
		}
	}
	return false;
}

/** Los miembros predefinidos de player: un mazo o una ficha per player no puede llamarse asi. */
static bool _isPlayerMember(const char * name) {
	const char * members[] = { "index", "name", "pieces", "score", "strategy" };
	for (size_t k = 0; k < sizeof(members) / sizeof(members[0]); ++k) {
		if (strcmp(members[k], name) == 0) {
			return true;
		}
	}
	return false;
}

/**
 * Pertenencia a un conjunto de valores. "A to B" incluye los dos extremos.
 */
static bool _valueSetContains(ValueSet * valueSet, const int value) {
	if (valueSet->type == RANGE_VALUE_SET) {
		return valueSet->min <= value && value <= valueSet->max;
	}
	for (Literal * literal = valueSet->elements; literal != NULL; literal = literal->next) {
		if (literal->type == INTEGER_LITERAL && literal->integer == value) {
			return true;
		}
	}
	return false;
}

/* TIPOS */

static Type _errorType() {
	return makeType(ERROR_KIND);
}

static bool _isKind(const Type type, const TypeKind kind) {
	return type.kind == kind && !type.isArray;
}

/**
 * El tipo de un campo de cardtype. Un campo de un tipo no permitido ya se
 * reporto al declarar el cardtype, asi que aca toma el tipo error.
 */
static Type _fieldType(Field * field) {
	if (field->type->isArray) {
		return _errorType();
	}
	switch (field->type->baseType) {
		case BOOLEAN_TYPE: return makeType(BOOLEAN_KIND);
		case INTEGER_TYPE: return makeType(INTEGER_KIND);
		case STRING_TYPE: return makeType(STRING_KIND);
		default: return _errorType();
	}
}

static TypeKind _literalKind(Literal * literal) {
	switch (literal->type) {
		case BOOLEAN_LITERAL: return BOOLEAN_KIND;
		case INTEGER_LITERAL: return INTEGER_KIND;
		case STRING_LITERAL: return STRING_KIND;
	}
	return ERROR_KIND;
}

static const char * _operatorName(const ExpressionType type) {
	switch (type) {
		case ADDITION_EXPRESSION: return "+";
		case AND_EXPRESSION: return "and";
		case DIVISION_EXPRESSION: return "/";
		case GREATER_EQUAL_EXPRESSION: return ">=";
		case GREATER_EXPRESSION: return ">";
		case LESS_EQUAL_EXPRESSION: return "<=";
		case LESS_EXPRESSION: return "<";
		case MODULE_EXPRESSION: return "%";
		case MULTIPLICATION_EXPRESSION: return "*";
		case NEGATION_EXPRESSION: return "-";
		case NOT_EXPRESSION: return "not";
		case OR_EXPRESSION: return "or";
		case SUBTRACTION_EXPRESSION: return "-";
		default: return "?";
	}
}

/**
 * Traduce un tipo escrito en el programa ("integer", "Carta[]") al tipo
 * anotado, y aplica las reglas de cada lugar:
 *
 *   - Un campo de cardtype solo puede ser integer, boolean o string: son los
 *     unicos literales que admiten los generadores de mazos.
 *   - Una decision no puede ser un arreglo, un mazo, un dado ni una strategy.
 *   - Una variable no puede ser un mazo ni un dado: "deck" no dice de que
 *     cardtype es, asi que un draw sobre esa variable no se podria chequear.
 *     Los mazos y los dados se usan por su nombre.
 */
static Type _resolveTypeSpec(TypeSpec * typeSpec, const TypeUse use, const char * name) {
	Type type = _errorType();
	switch (typeSpec->baseType) {
		case BOOLEAN_TYPE: type = makeType(BOOLEAN_KIND); break;
		case DECK_TYPE: type = makeDeckType(NULL); break;
		case DIE_TYPE: type = makeType(DIE_KIND); break;
		case INTEGER_TYPE: type = makeType(INTEGER_KIND); break;
		case PIECE_TYPE: type = makeType(PIECE_KIND); break;
		case PLAYER_TYPE: type = makeType(PLAYER_KIND); break;
		case STRATEGY_TYPE: type = makeType(STRATEGY_KIND); break;
		case STRING_TYPE: type = makeType(STRING_KIND); break;
		case NAMED_TYPE: {
			Symbol * symbol = _lookup(typeSpec->name);
			if (symbol == NULL) {
				GameItem * later = _findLaterDeclaration(typeSpec->name);
				if (later != NULL && later->type == CARD_TYPE_ITEM) {
					_error(_context.line, "el tipo \"%s\" se usa antes de su declaracion, en la linea %d", typeSpec->name, later->line);
				}
				else {
					_error(_context.line, "el tipo \"%s\" no esta declarado", typeSpec->name);
				}
				return _errorType();
			}
			if (symbol->kind != CARD_TYPE_SYMBOL) {
				_error(_context.line, "\"%s\" no es un tipo: los tipos con nombre son los cardtype", typeSpec->name);
				return _errorType();
			}
			type = symbol->type;
			break;
		}
	}
	if (typeSpec->isArray) {
		type = makeArrayType(type);
	}
	char found[TYPE_NAME_LENGTH];
	switch (use) {
		case CARD_FIELD_TYPE_USE:
			if (!_isKind(type, BOOLEAN_KIND) && !_isKind(type, INTEGER_KIND) && !_isKind(type, STRING_KIND)) {
				_error(_context.line, "el campo \"%s\" no puede ser de tipo %s: los campos de un cardtype son integer, boolean o string",
					name, typeName(type, found, sizeof(found)));
				return _errorType();
			}
			break;
		case DECISION_TYPE_USE:
			if (type.isArray || type.kind == DECK_KIND || type.kind == DIE_KIND || type.kind == STRATEGY_KIND) {
				_error(_context.line, "la decision \"%s\" no puede ser de tipo %s: las opciones son integer, boolean, string, player, piece o un cardtype",
					name, typeName(type, found, sizeof(found)));
				return _errorType();
			}
			break;
		case VARIABLE_TYPE_USE:
			if (type.kind == DECK_KIND || type.kind == DIE_KIND) {
				_error(_context.line, "la variable \"%s\" no puede ser de tipo %s: los mazos y los dados se usan por su nombre",
					name, typeName(type, found, sizeof(found)));
				return _errorType();
			}
			break;
	}
	return type;
}

/* EXPRESIONES */

/**
 * Chequea una expresion, anota su tipo y lo devuelve. Si la expresion esta
 * mal tipada reporta el error y devuelve el tipo error. Mientras la chequea,
 * los errores se reportan en la linea de la expresion.
 */
static Type _checkExpression(Expression * expression) {
	const int line = _context.line;
	if (expression->line > 0) {
		_context.line = expression->line;
	}
	Type type = _errorType();
	switch (expression->type) {
		case ADDITION_EXPRESSION:
		case DIVISION_EXPRESSION:
		case MODULE_EXPRESSION:
		case MULTIPLICATION_EXPRESSION:
		case SUBTRACTION_EXPRESSION:
			type = _checkBinary(expression, INTEGER_KIND, INTEGER_KIND);
			break;
		case GREATER_EQUAL_EXPRESSION:
		case GREATER_EXPRESSION:
		case LESS_EQUAL_EXPRESSION:
		case LESS_EXPRESSION:
			type = _checkBinary(expression, INTEGER_KIND, BOOLEAN_KIND);
			break;
		case AND_EXPRESSION:
		case OR_EXPRESSION:
			type = _checkBinary(expression, BOOLEAN_KIND, BOOLEAN_KIND);
			break;
		case EQUAL_EXPRESSION:
		case NOT_EQUAL_EXPRESSION:
			type = _checkEquality(expression);
			break;
		case NEGATION_EXPRESSION:
			type = _checkUnary(expression, INTEGER_KIND);
			break;
		case NOT_EXPRESSION:
			type = _checkUnary(expression, BOOLEAN_KIND);
			break;
		case CALL_EXPRESSION:
			type = _checkCall(expression);
			break;
		case INDEX_EXPRESSION:
			type = _checkIndex(expression);
			break;
		case MEMBER_EXPRESSION:
			type = _checkMember(expression);
			break;
		case ASK_EXPRESSION:
			type = _checkAsk(expression);
			break;
		case AGGREGATION_EXPRESSION:
			type = _checkAggregation(expression);
			break;
		case BOARD_EXPRESSION:
			if (_isBoardDeclared()) {
				type = makeType(BOARD_KIND);
			}
			else {
				_error(_context.line, "se usa board, pero el game no declara un board antes");
			}
			break;
		case BOOLEAN_EXPRESSION:
			type = makeType(BOOLEAN_KIND);
			break;
		case CURRENT_EXPRESSION:
			if (_context.currentForbiddenIn == NULL) {
				type = makeType(PLAYER_KIND);
			}
			else {
				_error(_context.line, "current no se puede usar en %s: todavia no hay un jugador activo", _context.currentForbiddenIn);
			}
			break;
		case IDENTIFIER_EXPRESSION:
			type = _checkIdentifier(expression);
			break;
		case INTEGER_EXPRESSION:
			type = makeType(INTEGER_KIND);
			break;
		case NONE_EXPRESSION:
			type = makeType(NONE_KIND);
			break;
		case OPTION_EXPRESSION:
			if (_context.inPolicy) {
				type = _context.optionType;
			}
			else {
				_error(_context.line, "option solo se puede usar en la politica de una strategy");
			}
			break;
		case PLAYERS_EXPRESSION:
			type = makeArrayType(makeType(PLAYER_KIND));
			break;
		case ROLL_EXPRESSION:
			type = _checkRoll(expression);
			break;
		case STRING_EXPRESSION:
			type = makeType(STRING_KIND);
			break;
		case TURNS_EXPRESSION:
			type = makeType(INTEGER_KIND);
			break;
	}
	expression->semanticType = type;
	_context.line = line;
	return type;
}

/**
 * Chequea una subexpresion que tiene que ser de un tipo simple. "what" la
 * describe en el mensaje ("la condicion del if").
 */
static Type _expectType(Expression * expression, const TypeKind kind, const char * what) {
	const Type type = _checkExpression(expression);
	if (!isErrorType(type) && !_isKind(type, kind)) {
		char expected[TYPE_NAME_LENGTH];
		char found[TYPE_NAME_LENGTH];
		_error(expression->line, "%s tiene que ser %s, no %s",
			what, typeName(makeType(kind), expected, sizeof(expected)), typeName(type, found, sizeof(found)));
		return _errorType();
	}
	return type;
}

/**
 * Los mazos de draw, play y shuffle. Un T[] no sirve: las acciones mueven
 * cartas entre mazos.
 */
static Type _expectDeck(Expression * expression, const char * action) {
	const Type type = _checkExpression(expression);
	if (!isErrorType(type) && type.kind != DECK_KIND) {
		char found[TYPE_NAME_LENGTH];
		_error(expression->line, "%s espera un mazo, no %s", action, typeName(type, found, sizeof(found)));
		return _errorType();
	}
	return type;
}

/**
 * El tipo de los elementos de lo que recorre un for, una agregacion o un ask.
 */
static Type _elementOf(const Type collection, const char * what) {
	if (isErrorType(collection)) {
		return collection;
	}
	if (!isCollectionType(collection)) {
		char found[TYPE_NAME_LENGTH];
		_error(_context.line, "%s recorre un arreglo o un mazo, no %s", what, typeName(collection, found, sizeof(found)));
		return _errorType();
	}
	return elementType(collection);
}

static Type _checkBinary(Expression * expression, const TypeKind operand, const TypeKind result) {
	const Type left = _checkExpression(expression->left);
	const Type right = _checkExpression(expression->right);
	if (isErrorType(left) || isErrorType(right)) {
		return _errorType();
	}
	if (!_isKind(left, operand) || !_isKind(right, operand)) {
		char expected[TYPE_NAME_LENGTH];
		char leftName[TYPE_NAME_LENGTH];
		char rightName[TYPE_NAME_LENGTH];
		_error(_context.line, "el operador \"%s\" espera dos %s y recibe %s y %s",
			_operatorName(expression->type), typeName(makeType(operand), expected, sizeof(expected)),
			typeName(left, leftName, sizeof(leftName)), typeName(right, rightName, sizeof(rightName)));
		return _errorType();
	}
	return makeType(result);
}

static Type _checkEquality(Expression * expression) {
	const Type left = _checkExpression(expression->left);
	const Type right = _checkExpression(expression->right);
	if (!isComparableType(left, right)) {
		char leftName[TYPE_NAME_LENGTH];
		char rightName[TYPE_NAME_LENGTH];
		_error(_context.line, "no se puede comparar %s con %s",
			typeName(left, leftName, sizeof(leftName)), typeName(right, rightName, sizeof(rightName)));
		return _errorType();
	}
	return makeType(BOOLEAN_KIND);
}

static Type _checkUnary(Expression * expression, const TypeKind kind) {
	const Type operand = _checkExpression(expression->operand);
	if (isErrorType(operand)) {
		return operand;
	}
	if (!_isKind(operand, kind)) {
		char expected[TYPE_NAME_LENGTH];
		char found[TYPE_NAME_LENGTH];
		_error(_context.line, "el operador \"%s\" espera un %s y recibe %s",
			_operatorName(expression->type), typeName(makeType(kind), expected, sizeof(expected)), typeName(operand, found, sizeof(found)));
		return _errorType();
	}
	return makeType(kind);
}

/**
 * Un identificador suelto: una variable, o un componente del game que tiene
 * valor (un mazo, una ficha, un dado o una strategy). Los mazos y las fichas
 * per player solo se usan a traves de un jugador.
 */
static Type _checkIdentifier(Expression * expression) {
	const char * name = expression->identifier;
	Symbol * symbol = _lookup(name);
	if (symbol == NULL) {
		_reportUndeclared(name);
		return _errorType();
	}
	switch (symbol->kind) {
		case LOCAL_SYMBOL:
			expression->resolution = LOCAL_RESOLUTION;
			return symbol->type;
		case GAME_VARIABLE_SYMBOL:
			expression->resolution = GAME_VARIABLE_RESOLUTION;
			return symbol->type;
		case DIE_SYMBOL:
			expression->resolution = DIE_RESOLUTION;
			return symbol->type;
		case STRATEGY_SYMBOL:
			expression->resolution = STRATEGY_RESOLUTION;
			return symbol->type;
		case DECK_SYMBOL:
		case PIECE_SYMBOL:
			if (symbol->perPlayer) {
				_error(_context.line, "\"%s\" es %s per player: se usa a traves de un jugador, como current.%s",
					name, symbol->kind == DECK_SYMBOL ? "un mazo" : "una ficha", name);
				return _errorType();
			}
			expression->resolution = symbol->kind == DECK_SYMBOL ? DECK_RESOLUTION : PIECE_RESOLUTION;
			return symbol->type;
		case CARD_TYPE_SYMBOL:
			_error(_context.line, "\"%s\" es un cardtype, no un valor", name);
			return _errorType();
		case DECISION_SYMBOL:
			_error(_context.line, "\"%s\" es una decision, no un valor: se consulta con ask", name);
			return _errorType();
		case METRIC_SYMBOL:
			_error(_context.line, "\"%s\" es una metric: solo se puede usar en un report", name);
			return _errorType();
	}
	return _errorType();
}

/** Anota la resolucion de un miembro y devuelve su tipo. */
static Type _member(Expression * expression, const Resolution resolution, const Type type) {
	expression->resolution = resolution;
	return type;
}

/**
 * Los miembros de cada tipo: los predefinidos de la especificacion (§4.1), los
 * mazos y fichas per player de un jugador, y los campos de una carta.
 */
static Type _checkMember(Expression * expression) {
	const char * member = expression->postfix.member;
	const Type object = _checkExpression(expression->postfix.object);
	char found[TYPE_NAME_LENGTH];
	if (isErrorType(object)) {
		return object;
	}
	if (object.isArray) {
		_error(_context.line, "%s es un arreglo y no tiene el miembro \"%s\"", typeName(object, found, sizeof(found)), member);
		return _errorType();
	}
	switch (object.kind) {
		case PLAYER_KIND:
			return _checkPlayerMember(expression);
		case PIECE_KIND:
			if (strcmp(member, "owner") == 0) {
				return _member(expression, PREDEFINED_MEMBER_RESOLUTION, makeType(PLAYER_KIND));
			}
			if (strcmp(member, "cell") == 0) {
				return _member(expression, PREDEFINED_MEMBER_RESOLUTION, makeType(INTEGER_KIND));
			}
			break;
		case BOARD_KIND:
			if (strcmp(member, "size") == 0) {
				return _member(expression, PREDEFINED_MEMBER_RESOLUTION, makeType(INTEGER_KIND));
			}
			if (strcmp(member, "cell") == 0) {
				_error(_context.line, "board.cell lleva el numero de celda: board.cell(i)");
				return _errorType();
			}
			break;
		case DIE_KIND:
			if (strcmp(member, "min") == 0 || strcmp(member, "max") == 0) {
				return _member(expression, PREDEFINED_MEMBER_RESOLUTION, makeType(INTEGER_KIND));
			}
			if (strcmp(member, "faces") == 0) {
				return _member(expression, PREDEFINED_MEMBER_RESOLUTION, makeArrayType(makeType(INTEGER_KIND)));
			}
			break;
		case DECK_KIND:
			if (strcmp(member, "size") == 0) {
				return _member(expression, PREDEFINED_MEMBER_RESOLUTION, makeType(INTEGER_KIND));
			}
			if (strcmp(member, "cardtype") == 0) {
				return _member(expression, PREDEFINED_MEMBER_RESOLUTION, makeType(STRING_KIND));
			}
			break;
		case CARD_KIND: {
			GameItem * cardType = _findCardType(object.cardType);
			Field * field = cardType == NULL ? NULL : _findField(cardType, member);
			if (field == NULL) {
				_error(_context.line, "el cardtype \"%s\" no tiene el campo \"%s\"", object.cardType, member);
				return _errorType();
			}
			return _member(expression, CARD_FIELD_RESOLUTION, _fieldType(field));
		}
		case NONE_KIND:
			_error(_context.line, "none no tiene miembros");
			return _errorType();
		default:
			break;
	}
	_error(_context.line, "%s no tiene el miembro \"%s\"", typeName(object, found, sizeof(found)), member);
	return _errorType();
}

static Type _checkPlayerMember(Expression * expression) {
	const char * member = expression->postfix.member;
	if (strcmp(member, "name") == 0) {
		return _member(expression, PREDEFINED_MEMBER_RESOLUTION, makeType(STRING_KIND));
	}
	if (strcmp(member, "index") == 0 || strcmp(member, "score") == 0) {
		return _member(expression, PREDEFINED_MEMBER_RESOLUTION, makeType(INTEGER_KIND));
	}
	if (strcmp(member, "pieces") == 0) {
		return _member(expression, PREDEFINED_MEMBER_RESOLUTION, makeArrayType(makeType(PIECE_KIND)));
	}
	if (strcmp(member, "strategy") == 0) {
		return _member(expression, PREDEFINED_MEMBER_RESOLUTION, makeType(STRATEGY_KIND));
	}
	Symbol * symbol = _lookup(member);
	if (symbol != NULL && (symbol->kind == DECK_SYMBOL || symbol->kind == PIECE_SYMBOL)) {
		if (symbol->perPlayer) {
			const Resolution resolution = symbol->kind == DECK_SYMBOL ? PER_PLAYER_DECK_RESOLUTION : PER_PLAYER_PIECE_RESOLUTION;
			return _member(expression, resolution, symbol->type);
		}
		_error(_context.line, "\"%s\" no es per player: se usa sin jugador, como \"%s\"", member, member);
		return _errorType();
	}
	GameItem * later = _findLaterDeclaration(member);
	if (later != NULL && ((later->type == DECK_ITEM && later->deck->perPlayer) || (later->type == PIECE_ITEM && later->piece.perPlayer))) {
		_error(_context.line, "\"%s\" se usa antes de su declaracion, en la linea %d", member, later->line);
	}
	else {
		_error(_context.line, "player no tiene el miembro \"%s\"", member);
	}
	return _errorType();
}

static Type _checkIndex(Expression * expression) {
	const Type object = _checkExpression(expression->postfix.object);
	_expectType(expression->postfix.index, INTEGER_KIND, "el indice");
	if (isErrorType(object)) {
		return object;
	}
	if (!isCollectionType(object)) {
		char found[TYPE_NAME_LENGTH];
		_error(_context.line, "solo se pueden indexar arreglos y mazos, no %s", typeName(object, found, sizeof(found)));
		return _errorType();
	}
	return elementType(object);
}

/**
 * La unica llamada del lenguaje es board.cell(i), que solo sirve como destino
 * de un place. El miembro "board.cell" se anota como predefinido.
 */
static Type _checkCall(Expression * expression) {
	Expression * function = expression->postfix.object;
	ExpressionList * arguments = expression->postfix.arguments;
	const bool isBoardCell = function->type == MEMBER_EXPRESSION
		&& function->postfix.object->type == BOARD_EXPRESSION
		&& strcmp(function->postfix.member, "cell") == 0;
	if (!isBoardCell) {
		_checkExpression(function);
		for (ExpressionList * argument = arguments; argument != NULL; argument = argument->next) {
			_checkExpression(argument->expression);
		}
		_error(_context.line, "la unica llamada es board.cell(i)");
		return _errorType();
	}
	const Type board = _checkExpression(function->postfix.object);
	function->semanticType = makeType(CELL_KIND);
	function->resolution = PREDEFINED_MEMBER_RESOLUTION;
	int count = 0;
	for (ExpressionList * argument = arguments; argument != NULL; argument = argument->next, ++count) {
		_expectType(argument->expression, INTEGER_KIND, "la celda de board.cell");
	}
	if (count != 1) {
		_error(_context.line, "board.cell lleva un argumento, la celda, y recibe %d", count);
		return _errorType();
	}
	return isErrorType(board) ? board : makeType(CELL_KIND);
}

static Type _checkRoll(Expression * expression) {
	Symbol * symbol = _lookup(expression->die);
	if (symbol == NULL) {
		_reportUndeclared(expression->die);
	}
	else if (symbol->kind != DIE_SYMBOL) {
		_error(_context.line, "roll de \"%s\", que no es un dado", expression->die);
	}
	return makeType(INTEGER_KIND);
}

/**
 * ask P for d(C): P es un jugador, d una decision declarada antes (caso 9) y
 * C un unico argumento, un T[] o un mazo de T, con T el tipo de d. No se puede
 * usar dentro de una politica: una strategy que consulta a otra puede no
 * terminar nunca.
 */
static Type _checkAsk(Expression * expression) {
	const char * name = expression->ask.decision;
	if (_context.inPolicy) {
		_error(_context.line, "no se puede usar ask dentro de una politica");
	}
	_expectType(expression->ask.player, PLAYER_KIND, "el jugador del ask");
	Type result = _errorType();
	Symbol * decision = _lookup(name);
	if (decision == NULL) {
		GameItem * later = _findLaterDeclaration(name);
		if (later != NULL && later->type == DECISION_ITEM) {
			_error(_context.line, "ask a \"%s\", que se declara despues, en la linea %d", name, later->line);
		}
		else {
			_error(_context.line, "ask a \"%s\", que no es una decision declarada en el game", name);
		}
	}
	else if (decision->kind != DECISION_SYMBOL) {
		_error(_context.line, "ask a \"%s\", que no es una decision", name);
	}
	else {
		result = decision->type;
	}
	int count = 0;
	Type options = _errorType();
	for (ExpressionList * argument = expression->ask.arguments; argument != NULL; argument = argument->next, ++count) {
		const Type type = _checkExpression(argument->expression);
		if (count == 0) {
			options = type;
		}
	}
	if (count != 1) {
		_error(_context.line, "ask a \"%s\" lleva un argumento, las opciones, y recibe %d", name, count);
	}
	else if (!isErrorType(options) && !isErrorType(result)
		&& !(isCollectionType(options) && isSameType(elementType(options), result))) {
		char expected[TYPE_NAME_LENGTH];
		char found[TYPE_NAME_LENGTH];
		typeName(result, expected, sizeof(expected));
		typeName(options, found, sizeof(found));
		if (result.kind == CARD_KIND) {
			_error(_context.line, "las opciones de \"%s\" tienen que ser %s[] o un mazo de %s, no %s", name, expected, expected, found);
		}
		else {
			_error(_context.line, "las opciones de \"%s\" tienen que ser %s[], no %s", name, expected, found);
		}
	}
	return result;
}

/**
 * count, sum, max, min y select. La variable ligada vive en su propio alcance
 * y es de solo lectura.
 */
static Type _checkAggregation(Expression * expression) {
	const char * names[] = {
		[COUNT_AGGREGATION] = "count",
		[MAX_AGGREGATION] = "max",
		[MIN_AGGREGATION] = "min",
		[SELECT_AGGREGATION] = "select",
		[SUM_AGGREGATION] = "sum"
	};
	const Type element = _elementOf(_checkExpression(expression->aggregation.collection), names[expression->aggregation.type]);
	openScope(_table);
	_declareLocal(expression->aggregation.variable, element, true);
	if (expression->aggregation.where != NULL) {
		_expectType(expression->aggregation.where, BOOLEAN_KIND, "el where");
	}
	if (expression->aggregation.by != NULL) {
		_expectType(expression->aggregation.by, INTEGER_KIND, "el by");
	}
	closeScope(_table);
	switch (expression->aggregation.type) {
		case COUNT_AGGREGATION:
		case SUM_AGGREGATION:
			return makeType(INTEGER_KIND);
		case MAX_AGGREGATION:
		case MIN_AGGREGATION:
			return element;
		case SELECT_AGGREGATION:
			return isErrorType(element) ? element : makeArrayType(element);
	}
	return _errorType();
}

/**
 * Lo que puede ir a la izquierda de un "=": una variable (salvo las de un for
 * o una agregacion), el score de un jugador, el campo de una carta, o un
 * elemento de un arreglo que a su vez es asignable. Si no, reporta por que,
 * salvo que la expresion ya tenga un error.
 */
static bool _checkAssignable(Expression * target) {
	if (isErrorType(target->semanticType)) {
		return false;
	}
	switch (target->type) {
		case IDENTIFIER_EXPRESSION: {
			if (target->resolution == GAME_VARIABLE_RESOLUTION) {
				return true;
			}
			if (target->resolution == LOCAL_RESOLUTION) {
				Symbol * symbol = _lookup(target->identifier);
				if (symbol != NULL && symbol->readOnly) {
					_error(_context.line, "no se puede asignar a \"%s\": es la variable de un for o de una agregacion", target->identifier);
					return false;
				}
				return true;
			}
			_error(_context.line, "no se puede asignar a \"%s\": es un componente del game", target->identifier);
			return false;
		}
		case MEMBER_EXPRESSION:
			if (target->resolution == CARD_FIELD_RESOLUTION) {
				return true;
			}
			if (target->resolution == PREDEFINED_MEMBER_RESOLUTION && strcmp(target->postfix.member, "score") == 0
				&& _isKind(target->postfix.object->semanticType, PLAYER_KIND)) {
				return true;
			}
			_error(_context.line, "no se puede asignar al miembro \"%s\": es de solo lectura", target->postfix.member);
			return false;
		case INDEX_EXPRESSION:
			if (target->postfix.object->semanticType.kind == DECK_KIND) {
				_error(_context.line, "no se puede asignar una carta de un mazo: las cartas se mueven con draw y play");
				return false;
			}
			return _checkAssignable(target->postfix.object);
		case PLAYERS_EXPRESSION:
			_error(_context.line, "no se puede asignar a players: es de solo lectura");
			return false;
		default:
			_error(_context.line, "no se puede asignar a esta expresion");
			return false;
	}
}

/* SENTENCIAS */

static void _checkStatements(Statement * statement) {
	for (; statement != NULL; statement = statement->next) {
		_checkStatement(statement);
	}
}

/**
 * Chequea una sentencia. Cada bloque y cada for abren un alcance; las
 * expresiones propias de la sentencia se chequean antes que sus cuerpos, asi
 * los errores llevan la linea correcta.
 */
static void _checkStatement(Statement * statement) {
	_context.line = statement->line;
	switch (statement->type) {
		case ASSIGNMENT_STATEMENT:
			_checkAssignment(statement);
			break;
		case BLOCK_STATEMENT:
			openScope(_table);
			_checkStatements(statement->statements);
			closeScope(_table);
			break;
		case DECLARATION_STATEMENT:
			_checkDeclaration(statement->declaration);
			break;
		case FOR_EACH_STATEMENT:
			_checkForEach(statement);
			break;
		case FOR_RANGE_STATEMENT:
			_checkForRange(statement);
			break;
		case IF_STATEMENT:
			_expectType(statement->ifStatement.condition, BOOLEAN_KIND, "la condicion del if");
			_checkStatement(statement->ifStatement.thenBranch);
			if (statement->ifStatement.elseBranch != NULL) {
				_checkStatement(statement->ifStatement.elseBranch);
			}
			break;
		case LOG_STATEMENT:
			_checkLog(statement->log);
			break;
		case REPEAT_STATEMENT:
			_expectType(statement->loop.condition, INTEGER_KIND, "la cantidad del repeat");
			_checkStatement(statement->loop.body);
			break;
		case USES_STATEMENT:
			_checkUses(statement);
			break;
		case WHILE_STATEMENT:
			_expectType(statement->loop.condition, BOOLEAN_KIND, "la condicion del while");
			_checkStatement(statement->loop.body);
			break;
		case DRAW_STATEMENT:
		case GIVE_STATEMENT:
		case MOVE_STATEMENT:
		case PLACE_STATEMENT:
		case PLAY_STATEMENT:
		case SHUFFLE_STATEMENT:
		case TAKE_STATEMENT:
			_checkAction(statement);
			break;
	}
}

static void _checkAssignment(Statement * statement) {
	const Type target = _checkExpression(statement->assignment.target);
	const Type value = _checkExpression(statement->assignment.value);
	if (_checkAssignable(statement->assignment.target) && !isAssignableType(target, value)) {
		char targetName[TYPE_NAME_LENGTH];
		char valueName[TYPE_NAME_LENGTH];
		_error(_context.line, "se asigna %s donde se espera %s",
			typeName(value, valueName, sizeof(valueName)), typeName(target, targetName, sizeof(targetName)));
	}
}

/**
 * Inicializador de una variable local o del game. Se chequea antes de declarar
 * la variable: "integer x = x;" usa una x que todavia no existe.
 */
static void _checkInitializer(VariableDeclaration * variable, const Type type) {
	if (variable->initializer == NULL) {
		return;
	}
	const Type value = _checkExpression(variable->initializer);
	if (!isAssignableType(type, value)) {
		char declaredTypeName[TYPE_NAME_LENGTH];
		char valueName[TYPE_NAME_LENGTH];
		_error(_context.line, "la variable \"%s\" es %s y se inicializa con %s",
			variable->name, typeName(type, declaredTypeName, sizeof(declaredTypeName)), typeName(value, valueName, sizeof(valueName)));
	}
}

static void _checkDeclaration(VariableDeclaration * declaration) {
	const Type type = _resolveTypeSpec(declaration->type, VARIABLE_TYPE_USE, declaration->name);
	_checkInitializer(declaration, type);
	_declareLocal(declaration->name, type, false);
}

static void _checkForEach(Statement * statement) {
	const Type element = _elementOf(_checkExpression(statement->forLoop.collection), "el for");
	openScope(_table);
	_declareLocal(statement->forLoop.variable, element, true);
	_checkStatement(statement->forLoop.body);
	closeScope(_table);
}

static void _checkForRange(Statement * statement) {
	_expectType(statement->forLoop.from, INTEGER_KIND, "el inicio del for");
	_expectType(statement->forLoop.to, INTEGER_KIND, "el final del for");
	openScope(_table);
	_declareLocal(statement->forLoop.variable, makeType(INTEGER_KIND), true);
	_checkStatement(statement->forLoop.body);
	closeScope(_table);
}

static void _checkLog(LogPart * parts) {
	for (LogPart * part = parts; part != NULL; part = part->next) {
		if (part->type == EXPRESSION_LOG_PART) {
			const Type type = _checkExpression(part->expression);
			if (!isPrintableType(type)) {
				char found[TYPE_NAME_LENGTH];
				_error(_context.line, "un log no puede mostrar %s", typeName(type, found, sizeof(found)));
			}
		}
	}
}

/**
 * "P uses S": P es un jugador y S una strategy del game o una variable de tipo
 * strategy. La resolucion de S queda anotada en el Statement.
 */
static void _checkUses(Statement * statement) {
	_expectType(statement->assignment.target, PLAYER_KIND, "el jugador del uses");
	const char * name = statement->assignment.strategy;
	Symbol * symbol = _lookup(name);
	if (symbol == NULL) {
		_reportUndeclared(name);
		return;
	}
	if (symbol->kind == STRATEGY_SYMBOL) {
		statement->assignment.strategyResolution = STRATEGY_RESOLUTION;
		return;
	}
	const bool isVariable = symbol->kind == LOCAL_SYMBOL || symbol->kind == GAME_VARIABLE_SYMBOL;
	if (isVariable && (isErrorType(symbol->type) || _isKind(symbol->type, STRATEGY_KIND))) {
		statement->assignment.strategyResolution = symbol->kind == LOCAL_SYMBOL ? LOCAL_RESOLUTION : GAME_VARIABLE_RESOLUTION;
		return;
	}
	_error(_context.line, "\"%s\" no es una strategy", name);
}

/**
 * Las acciones del dominio (§4.5). draw y play mueven cartas entre mazos del
 * mismo cardtype (caso 6); move y place necesitan un board.
 */
static void _checkAction(Statement * statement) {
	char leftName[TYPE_NAME_LENGTH];
	char rightName[TYPE_NAME_LENGTH];
	switch (statement->type) {
		case DRAW_STATEMENT: {
			_expectType(statement->action.amount, INTEGER_KIND, "la cantidad del draw");
			const Type from = _expectDeck(statement->action.source, "draw");
			const Type to = _expectDeck(statement->action.target, "draw");
			if (!isErrorType(from) && !isErrorType(to) && !isSameType(from, to)) {
				_error(_context.line, "draw entre mazos de cardtype distinto: %s y %s",
					typeName(from, leftName, sizeof(leftName)), typeName(to, rightName, sizeof(rightName)));
			}
			break;
		}
		case PLAY_STATEMENT: {
			const Type card = _checkExpression(statement->action.subject);
			const Type from = _expectDeck(statement->action.source, "play");
			const Type to = _expectDeck(statement->action.target, "play");
			if (!isErrorType(from) && !isErrorType(to) && !isSameType(from, to)) {
				_error(_context.line, "play entre mazos de cardtype distinto: %s y %s",
					typeName(from, leftName, sizeof(leftName)), typeName(to, rightName, sizeof(rightName)));
			}
			if (isErrorType(card)) {
				break;
			}
			if (!_isKind(card, CARD_KIND)) {
				_error(_context.line, "play mueve una carta, no %s", typeName(card, leftName, sizeof(leftName)));
			}
			else if (!isErrorType(from) && !isSameType(elementType(from), card)) {
				_error(_context.line, "play de una carta %s desde un mazo de %s", card.cardType, from.cardType);
			}
			break;
		}
		case SHUFFLE_STATEMENT:
			_expectDeck(statement->action.subject, "shuffle");
			break;
		case GIVE_STATEMENT:
			_expectType(statement->action.amount, INTEGER_KIND, "la cantidad del give");
			_expectType(statement->action.target, PLAYER_KIND, "el jugador del give");
			break;
		case TAKE_STATEMENT:
			_expectType(statement->action.amount, INTEGER_KIND, "la cantidad del take");
			_expectType(statement->action.source, PLAYER_KIND, "el jugador del take");
			break;
		case MOVE_STATEMENT:
			_expectType(statement->action.subject, PIECE_KIND, "lo que mueve un move");
			_expectType(statement->action.amount, INTEGER_KIND, "la cantidad del move");
			if (!_isBoardDeclared()) {
				_error(_context.line, "move necesita un board, y el game no declara uno antes");
			}
			break;
		case PLACE_STATEMENT: {
			_expectType(statement->action.subject, PIECE_KIND, "lo que coloca un place");
			const Type cell = _checkExpression(statement->action.target);
			if (!isErrorType(cell) && !_isKind(cell, CELL_KIND)) {
				_error(_context.line, "place coloca en una celda, board.cell(i), y no en %s", typeName(cell, leftName, sizeof(leftName)));
			}
			break;
		}
		default:
			break;
	}
}

/* ITEMS DEL GAME */

static void _enterItem(GameItem * item, const int position) {
	_context.item = item;
	_context.position = position;
	_context.line = item->line;
	_context.currentForbiddenIn = NULL;
	_context.inPolicy = false;
	_context.optionType = _errorType();
}

/**
 * board, players, prepare, turn, turn order, win when y end after aparecen a
 * lo sumo una vez por game.
 */
static void _checkUniqueComponent(GameItem * item, const char * component) {
	for (GameItem * previous = _context.game->items; previous != item; previous = previous->next) {
		if (previous->type == item->type) {
			_error(item->line, "el game ya declara %s en la linea %d", component, previous->line);
			return;
		}
	}
}

static void _checkPlayers(GameItem * item) {
	ValueSet * players = item->players;
	bool valid = true;
	if (players->type == RANGE_VALUE_SET) {
		valid = 1 <= players->min;
	}
	for (Literal * literal = players->elements; literal != NULL; literal = literal->next) {
		valid = valid && 1 <= literal->integer;
	}
	if (!valid) {
		_error(item->line, "la cantidad de jugadores tiene que ser al menos 1");
	}
}

/**
 * Se declara antes de chequear los campos, asi un campo de tipo cardtype
 * recibe el mensaje de tipo no permitido, y no el de tipo no declarado.
 */
static void _checkCardType(GameItem * item) {
	_declare(item->cardType.name, CARD_TYPE_SYMBOL, makeCardType(item->cardType.name), _context.position);
	for (Field * field = item->cardType.fields; field != NULL; field = field->next) {
		for (Field * previous = item->cardType.fields; previous != field; previous = previous->next) {
			if (strcmp(previous->name, field->name) == 0) {
				_error(item->line, "el cardtype \"%s\" repite el campo \"%s\"", item->cardType.name, field->name);
				break;
			}
		}
		_resolveTypeSpec(field->type, CARD_FIELD_TYPE_USE, field->name);
	}
}

static void _checkFieldValue(Field * field, const Type value, const int line) {
	const Type type = _fieldType(field);
	if (!isAssignableType(type, value)) {
		char declaredTypeName[TYPE_NAME_LENGTH];
		char valueName[TYPE_NAME_LENGTH];
		_error(line, "el campo \"%s\" es %s y recibe %s",
			field->name, typeName(type, declaredTypeName, sizeof(declaredTypeName)), typeName(value, valueName, sizeof(valueName)));
	}
}

/**
 * Una carta tiene exactamente los campos de su cardtype (caso 5), cada uno una
 * vez y con un valor de su tipo. La forma posicional los da en el orden de la
 * declaracion. Si el cardtype no existe solo se chequean las expresiones.
 */
static void _checkCard(Card * card, GameItem * cardType) {
	const char * cardTypeName = cardType == NULL ? NULL : cardType->cardType.name;
	if (card->type == POSITIONAL_CARD) {
		int values = 0;
		Field * field = cardType == NULL ? NULL : cardType->cardType.fields;
		for (ExpressionList * value = card->values; value != NULL; value = value->next, ++values) {
			const Type type = _checkExpression(value->expression);
			if (field != NULL) {
				_checkFieldValue(field, type, value->expression->line);
				field = field->next;
			}
		}
		if (cardType != NULL) {
			int fields = 0;
			for (Field * declared = cardType->cardType.fields; declared != NULL; declared = declared->next) {
				++fields;
			}
			if (values != fields) {
				_error(_context.line, "la carta tiene %d %s y el cardtype \"%s\" tiene %d %s",
					values, values == 1 ? "valor" : "valores", cardTypeName, fields, fields == 1 ? "campo" : "campos");
			}
		}
		return;
	}
	for (CardField * value = card->fields; value != NULL; value = value->next) {
		const Type type = _checkExpression(value->value);
		for (CardField * previous = card->fields; previous != value; previous = previous->next) {
			if (strcmp(previous->name, value->name) == 0) {
				_error(value->value->line, "la carta repite el campo \"%s\"", value->name);
				break;
			}
		}
		if (cardType == NULL) {
			continue;
		}
		Field * field = _findField(cardType, value->name);
		if (field == NULL) {
			_error(value->value->line, "el cardtype \"%s\" no tiene el campo \"%s\"", cardTypeName, value->name);
		}
		else {
			_checkFieldValue(field, type, value->value->line);
		}
	}
	if (cardType == NULL) {
		return;
	}
	for (Field * field = cardType->cardType.fields; field != NULL; field = field->next) {
		bool present = false;
		for (CardField * value = card->fields; value != NULL && !present; value = value->next) {
			present = strcmp(value->name, field->name) == 0;
		}
		if (!present) {
			_error(_context.line, "a la carta le falta el campo \"%s\" del cardtype \"%s\"", field->name, cardTypeName);
		}
	}
}

/**
 * Los generadores de un mazo: uno por campo del cardtype, cada uno con
 * literales del tipo del campo. Un rango (a to b) solo sirve para enteros.
 */
static void _checkGenerators(Generator * generators, GameItem * cardType) {
	if (cardType == NULL) {
		return;
	}
	const char * cardTypeName = cardType->cardType.name;
	char declaredTypeName[TYPE_NAME_LENGTH];
	char valueName[TYPE_NAME_LENGTH];
	for (Generator * generator = generators; generator != NULL; generator = generator->next) {
		bool repeated = false;
		for (Generator * previous = generators; previous != generator && !repeated; previous = previous->next) {
			repeated = strcmp(previous->field, generator->field) == 0;
		}
		if (repeated) {
			_error(_context.line, "el mazo repite el generador de \"%s\"", generator->field);
			continue;
		}
		Field * field = _findField(cardType, generator->field);
		if (field == NULL) {
			_error(_context.line, "el cardtype \"%s\" no tiene el campo \"%s\"", cardTypeName, generator->field);
			continue;
		}
		const Type type = _fieldType(field);
		if (isErrorType(type)) {
			continue;
		}
		typeName(type, declaredTypeName, sizeof(declaredTypeName));
		if (generator->values->type == RANGE_VALUE_SET) {
			if (!_isKind(type, INTEGER_KIND)) {
				_error(_context.line, "el campo \"%s\" es %s: un rango (a to b) solo sirve para campos integer", field->name, declaredTypeName);
			}
			continue;
		}
		for (Literal * literal = generator->values->elements; literal != NULL; literal = literal->next) {
			if (_literalKind(literal) != type.kind) {
				_error(_context.line, "el campo \"%s\" es %s y el generador tiene un valor %s",
					field->name, declaredTypeName, typeName(makeType(_literalKind(literal)), valueName, sizeof(valueName)));
				break;
			}
		}
	}
	for (Field * field = cardType->cardType.fields; field != NULL; field = field->next) {
		bool present = false;
		for (Generator * generator = generators; generator != NULL && !present; generator = generator->next) {
			present = strcmp(generator->field, field->name) == 0;
		}
		if (!present) {
			_error(_context.line, "el mazo no genera el campo \"%s\" del cardtype \"%s\"", field->name, cardTypeName);
		}
	}
}

/**
 * Un mazo es de un cardtype declarado antes. Se declara despues de chequear
 * sus cartas, que no pueden referirse al mazo mismo.
 */
static void _checkDeck(GameItem * item) {
	DeckDeclaration * deck = item->deck;
	GameItem * cardType = NULL;
	Type type = _errorType();
	Symbol * symbol = _lookup(deck->cardType);
	if (symbol == NULL) {
		GameItem * later = _findLaterDeclaration(deck->cardType);
		if (later != NULL && later->type == CARD_TYPE_ITEM) {
			_error(item->line, "el cardtype \"%s\" se usa antes de su declaracion, en la linea %d", deck->cardType, later->line);
		}
		else {
			_error(item->line, "el cardtype \"%s\" no esta declarado", deck->cardType);
		}
	}
	else if (symbol->kind != CARD_TYPE_SYMBOL) {
		_error(item->line, "\"%s\" no es un cardtype", deck->cardType);
	}
	else {
		cardType = _findCardType(symbol->name);
		type = makeDeckType(symbol->name);
	}
	if (deck->perPlayer && _isPlayerMember(deck->name)) {
		_error(item->line, "\"%s\" es un miembro de player: un mazo per player no puede llamarse asi", deck->name);
	}
	_context.currentForbiddenIn = "las cartas de un mazo";
	if (deck->bodyType == CARDS_DECK_BODY) {
		for (Card * card = deck->cards; card != NULL; card = card->next) {
			_checkCard(card, cardType);
		}
	}
	else if (deck->bodyType == GENERATORS_DECK_BODY) {
		_checkGenerators(deck->generators, cardType);
	}
	Symbol * declared = _declare(deck->name, DECK_SYMBOL, type, _context.position);
	declared->perPlayer = deck->perPlayer;
}

/**
 * Chequea las politicas de una strategy. Corre al cerrar el game, porque una
 * strategy tiene que resolver todas las decisiones, aun las declaradas despues
 * (caso 8). La posicion del contexto es la de la strategy, asi que el resto de
 * los nombres siguen las reglas de declarar antes de usar.
 */
static void _checkStrategy(GameItem * strategy) {
	const char * name = strategy->strategy.name;
	for (GameItem * item = _context.game->items; item != NULL; item = item->next) {
		if (item->type == DECISION_ITEM && _findPolicy(strategy->strategy.policies, item->decision.name) == NULL) {
			_error(strategy->line, "la strategy \"%s\" no resuelve la decision \"%s\"", name, item->decision.name);
		}
	}
	for (Policy * policy = strategy->strategy.policies; policy != NULL; policy = policy->next) {
		if (_findPolicy(strategy->strategy.policies, policy->decision) != policy) {
			_error(strategy->line, "la strategy \"%s\" tiene dos politicas para \"%s\"", name, policy->decision);
		}
		Symbol * decision = lookupSymbol(_table, policy->decision, INT_MAX);
		_context.optionType = _errorType();
		if (decision == NULL || decision->kind != DECISION_SYMBOL) {
			_error(strategy->line, "la strategy \"%s\" tiene una politica para \"%s\", que no es una decision declarada",
				name, policy->decision);
		}
		else {
			_context.optionType = decision->type;
		}
		_context.inPolicy = true;
		for (Criterion * criterion = policy->criteria; criterion != NULL; criterion = criterion->next) {
			if (criterion->type == MAX_CRITERION) {
				_expectType(criterion->expression, INTEGER_KIND, "el criterio max");
			}
			else if (criterion->type == MIN_CRITERION) {
				_expectType(criterion->expression, INTEGER_KIND, "el criterio min");
			}
		}
		if (policy->where != NULL) {
			_expectType(policy->where, BOOLEAN_KIND, "el where de la politica");
		}
		_context.inPolicy = false;
	}
}

static void _checkGameItem(GameItem * item) {
	switch (item->type) {
		case BOARD_ITEM:
			_checkUniqueComponent(item, "board");
			if (item->cells < 1) {
				_error(item->line, "el board tiene que tener al menos una celda");
			}
			break;
		case CARD_TYPE_ITEM:
			_checkCardType(item);
			break;
		case DECISION_ITEM: {
			const Type type = _resolveTypeSpec(item->decision.type, DECISION_TYPE_USE, item->decision.name);
			_declare(item->decision.name, DECISION_SYMBOL, type, _context.position);
			break;
		}
		case DECK_ITEM:
			_checkDeck(item);
			break;
		case DIE_ITEM:
			_declare(item->die.name, DIE_SYMBOL, makeType(DIE_KIND), _context.position);
			break;
		case END_ITEM:
			_checkUniqueComponent(item, "end after");
			break;
		case METRIC_ITEM: {
			char what[TYPE_NAME_LENGTH];
			snprintf(what, sizeof(what), "la metric \"%s\"", item->metric.name);
			_expectType(item->metric.value, INTEGER_KIND, what);
			_declare(item->metric.name, METRIC_SYMBOL, makeType(INTEGER_KIND), _context.position);
			break;
		}
		case PIECE_ITEM: {
			if (item->piece.perPlayer && _isPlayerMember(item->piece.name)) {
				_error(item->line, "\"%s\" es un miembro de player: una ficha per player no puede llamarse asi", item->piece.name);
			}
			Symbol * symbol = _declare(item->piece.name, PIECE_SYMBOL, makeType(PIECE_KIND), _context.position);
			symbol->perPlayer = item->piece.perPlayer;
			break;
		}
		case PLAYERS_ITEM:
			_checkUniqueComponent(item, "players");
			_checkPlayers(item);
			break;
		case PREPARE_ITEM:
			_checkUniqueComponent(item, "prepare");
			_context.currentForbiddenIn = "prepare";
			_checkStatement(item->block);
			break;
		case STRATEGY_ITEM:
			/* Sus politicas se chequean al cerrar el game (_checkStrategy). */
			_declare(item->strategy.name, STRATEGY_SYMBOL, makeType(STRATEGY_KIND), _context.position);
			break;
		case TURN_ITEM:
			_checkUniqueComponent(item, "turn");
			_checkStatement(item->block);
			break;
		case TURN_ORDER_ITEM:
			_checkUniqueComponent(item, "turn order");
			break;
		case VARIABLE_ITEM: {
			VariableDeclaration * variable = item->variable;
			const Type type = _resolveTypeSpec(variable->type, VARIABLE_TYPE_USE, variable->name);
			_context.currentForbiddenIn = "el valor inicial de una variable del game";
			_checkInitializer(variable, type);
			_declare(variable->name, GAME_VARIABLE_SYMBOL, type, _context.position);
			break;
		}
		case WIN_ITEM:
			_checkUniqueComponent(item, "win when");
			_expectType(item->condition, BOOLEAN_KIND, "la condicion del win when");
			break;
	}
}

/**
 * Chequea un game en una sola pasada: la especificacion exige declarar antes
 * de usar (§4.2), asi que cada item se chequea con lo declarado hasta ahi. Las
 * strategy se chequean al final (ver _checkStrategy).
 */
static void _checkGame(GameDeclaration * game, const int line) {
	_context = (AnalysisContext) { .game = game, .line = line, .optionType = _errorType() };
	openScope(_table);
	int position = 0;
	for (GameItem * item = game->items; item != NULL; item = item->next, ++position) {
		_enterItem(item, position);
		_checkGameItem(item);
	}
	position = 0;
	for (GameItem * item = game->items; item != NULL; item = item->next, ++position) {
		if (item->type == STRATEGY_ITEM) {
			_enterItem(item, position);
			_checkStrategy(item);
		}
	}
	if (_findPlayers(game->items) == NULL) {
		_error(line, "el game \"%s\" no declara players", game->name);
	}
	closeScope(_table);
}

/* SIMULACIONES Y REPORTES */

static void _checkGameNames(TopLevel * topLevels) {
	for (TopLevel * topLevel = topLevels; topLevel != NULL; topLevel = topLevel->next) {
		if (topLevel->type != GAME_TOP_LEVEL) {
			continue;
		}
		for (TopLevel * previous = topLevels; previous != topLevel; previous = previous->next) {
			if (previous->type == GAME_TOP_LEVEL && strcmp(previous->game->name, topLevel->game->name) == 0) {
				_error(topLevel->line, "el game \"%s\" ya esta declarado en la linea %d", topLevel->game->name, previous->line);
				break;
			}
		}
	}
}

/**
 * Caso 10: el simulate nombra un game declarado, y la cantidad de jugadores
 * pertenece al conjunto que ese game admite. Ademas, corre al menos una
 * partida.
 */
static void _checkSimulation(Simulation * simulation, TopLevel * topLevels, const int line) {
	if (simulation->games < 1) {
		_error(line, "simulate de %d partidas: tiene que correr al menos una", simulation->games);
	}
	GameDeclaration * game = _findGame(topLevels, simulation->gameName);
	if (game == NULL) {
		_error(line, "simulate de \"%s\", que no es un game declarado", simulation->gameName);
		return;
	}
	ValueSet * players = _findPlayers(game->items);
	if (players == NULL || _valueSetContains(players, simulation->players)) {
		return;
	}
	if (players->type == RANGE_VALUE_SET) {
		_error(line, "simulate de \"%s\" con %d jugadores: el game admite de %d a %d",
			simulation->gameName, simulation->players, players->min, players->max);
		return;
	}
	_error(line, "simulate de \"%s\" con %d jugadores: el game no admite esa cantidad",
		simulation->gameName, simulation->players);
}

/**
 * Un report opera sobre el simulate anterior: tiene que haber uno, y cada
 * metric que nombra tiene que estar declarada en el game simulado.
 */
static void _checkReport(ReportItem * report, const bool simulated, GameDeclaration * game, const int line) {
	if (!simulated) {
		_error(line, "report sin un simulate antes: no hay partidas sobre las que reportar");
		return;
	}
	if (game == NULL) {
		/* El simulate ya reporto que el game no existe. */
		return;
	}
	for (ReportItem * item = report; item != NULL; item = item->next) {
		if (item->type == AGGREGATE_REPORT_ITEM && item->metric != NULL && !_hasMetric(game, item->metric)) {
			_error(line, "report de \"%s\", que no es una metric del game \"%s\"", item->metric, game->name);
		}
	}
}

static void _checkSimulationsAndReports(TopLevel * topLevels) {
	bool simulated = false;
	GameDeclaration * simulatedGame = NULL;
	for (TopLevel * topLevel = topLevels; topLevel != NULL; topLevel = topLevel->next) {
		switch (topLevel->type) {
			case SIMULATION_TOP_LEVEL:
				_checkSimulation(topLevel->simulation, topLevels, topLevel->line);
				simulated = true;
				simulatedGame = _findGame(topLevels, topLevel->simulation->gameName);
				break;
			case REPORT_TOP_LEVEL:
				_checkReport(topLevel->report, simulated, simulatedGame, topLevel->line);
				break;
			case GAME_TOP_LEVEL:
				break;
		}
	}
}

/* PUBLIC FUNCTIONS */

CompilationStatus executeSemanticAnalysis(CompilerState * compilerState) {
	logDebugging(_logger, "Analyzing the AST...");
	Program * program = compilerState->abstractSyntaxtTree;
	_errors = 0;
	_table = createSymbolTable();
	_checkGameNames(program->items);
	for (TopLevel * topLevel = program->items; topLevel != NULL; topLevel = topLevel->next) {
		if (topLevel->type == GAME_TOP_LEVEL) {
			_checkGame(topLevel->game, topLevel->line);
		}
	}
	_checkSimulationsAndReports(program->items);
	destroySymbolTable(_table);
	_table = NULL;
	logDebugging(_logger, "Semantic errors found: %u.", _errors);
	return _errors == 0 ? SUCCEEDED : FAILED;
}
