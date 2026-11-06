#include "Generator.h"

/**
 * Generador de codigo: recorre el AST anotado por el analisis semantico y
 * emite un programa en C que usa el runtime (MeepleRuntime.h). El programa
 * corre los simulate y los report en el orden en que aparecen.
 *
 * Nombres en el C generado, para que no choquen entre si, con el runtime
 * (prefijos mp_, Mp y MP_) ni con las palabras reservadas de C:
 *
 *   g<k>_<nombre>   lo que declara el game k: dados, fichas, mazos, variables,
 *                   decisiones y el struct de cada cardtype.
 *   h<k>_...        lo que agrega el compilador al game k: las funciones del
 *                   juego, las politicas, las tablas y el MpGame.
 *   v_<nombre>      las variables locales y las de los for y las agregaciones.
 *   f_<nombre>      los campos de un cardtype.
 *   _t<n>           los temporales, numerados en todo el programa.
 *
 * Un identificador de Meeple puede empezar con "_", pero nunca con un digito,
 * asi que "g1_" mas un nombre no se confunde con "g10_" mas otro.
 *
 * Las expresiones se traducen en el lugar, en funcion de sus hijos. Las
 * agregaciones necesitan un bucle, y se traducen como statement expressions
 * de GCC ("({ ... })"): por eso el C generado se compila con -std=gnu99.
 */

/* MODULE INTERNAL STATE */

static Logger * _logger = NULL;

/* El archivo de salida y el estado del recorrido. */
static FILE * _output = NULL;
static unsigned int _level = 0;
static unsigned int _temporaries = 0;
/* La linea de la sentencia o del item que se traduce (para los errores de ejecucion). */
static int _line = 0;
/* El game que se traduce y su numero (k en g<k>_ y h<k>_). */
static GameDeclaration * _game = NULL;
static int _gameIndex = 0;

/** Shutdown module's internal state. */
void _shutdownGeneratorModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: Generator...");
		destroyLogger(_logger);
		_logger = NULL;
	}
}

ModuleDestructor initializeGeneratorModule() {
	_logger = createLogger("Generator");
	return _shutdownGeneratorModule;
}

/* PRIVATE FUNCTIONS */

static void _print(const char * const format, ...) __attribute__((format(printf, 1, 2)));
static void _indent();
static void _printLine(const char * const format, ...) __attribute__((format(printf, 1, 2)));
static void _blankLine();
static void _printString(const char * string);
static unsigned int _temporary();
static int _lineOf(Expression * expression);

static Type _typeOfSpec(TypeSpec * typeSpec);
static void _printCType(const Type type);
static void _printDefault(const Type type);
static void _printDescribe(const Type type);
static void _printUnwrap(const Type type, const char * value);
static void _printUnwrapBegin(const Type type);
static void _printUnwrapEnd(const Type type);
static void _printWrapBegin(const Type type);

static int _metricIndex(GameDeclaration * game, const char * name);
static int _strategyIndex(const char * name);
static int _policyNumber(GameItem * strategy, Policy * policy);
static Policy * _findPolicy(GameItem * strategy, const char * decision);
static GameItem * _findCardType(const char * name);
static GameItem * _findItem(const GameItemType type);

static void _expression(Expression * expression);
static void _aggregation(Expression * expression);
static void _binary(Expression * expression, const char * operator);
static void _equality(Expression * expression);
static void _identifier(Expression * expression);
static void _member(Expression * expression);
static void _playerOf(Expression * object, const int line);
static void _value(const Type target, Expression * value);

static void _statement(Statement * statement);
static void _statements(Statement * statement);
static void _body(Statement * statement);
static void _action(Statement * statement);
static void _assignment(Statement * statement);
static void _declaration(VariableDeclaration * declaration);
static void _forEach(Statement * statement);
static void _forRange(Statement * statement);
static void _log(LogPart * parts);
static void _uses(Statement * statement);

static void _cardTypes();
static void _components();
static void _deck(GameItem * item);
static void _card(const char * deck, GameItem * cardType, Card * card);
static void _generators(const char * deck, GameItem * cardType, Generator * generator, Generator * generators, unsigned int * values);
static void _names();
static void _policies();
static void _decisions();
static void _reset();
static void _prepare();
static void _turn();
static void _win();
static void _metrics();
static void _gameDescriptor();
static void _gameDeclaration(GameDeclaration * game, const int index);
static void _main(Program * program);

/* SALIDA */

static void _print(const char * const format, ...) {
	va_list arguments;
	va_start(arguments, format);
	vfprintf(_output, format, arguments);
	va_end(arguments);
}

static void _indent() {
	for (unsigned int k = 0; k < _level; ++k) {
		fputc('\t', _output);
	}
}

/** Una linea entera, con la indentacion del nivel actual. */
static void _printLine(const char * const format, ...) {
	_indent();
	va_list arguments;
	va_start(arguments, format);
	vfprintf(_output, format, arguments);
	va_end(arguments);
	fputc('\n', _output);
}

static void _blankLine() {
	fputc('\n', _output);
}

/**
 * Un literal de cadena de C. Las cadenas de Meeple no tienen secuencias de
 * escape ni comillas, pero pueden tener barras invertidas, tabulaciones y
 * bytes que no son ASCII: todo lo que no es imprimible va en octal.
 */
static void _printString(const char * string) {
	fputc('"', _output);
	for (const unsigned char * c = (const unsigned char *) string; *c != '\0'; ++c) {
		if (*c == '\\' || *c == '"') {
			fprintf(_output, "\\%c", *c);
		}
		else if (*c < 0x20 || 0x7F <= *c || *c == '?') {
			/* El "?" tambien, para no armar un trigrafo. */
			fprintf(_output, "\\%03o", *c);
		}
		else {
			fputc(*c, _output);
		}
	}
	fputc('"', _output);
}

static unsigned int _temporary() {
	return _temporaries++;
}

/** La linea de una expresion, o la de la sentencia que la contiene si no tiene. */
static int _lineOf(Expression * expression) {
	return expression->line > 0 ? expression->line : _line;
}

/* TIPOS */

/** El tipo anotado que corresponde a un tipo escrito en el programa. */
static Type _typeOfSpec(TypeSpec * typeSpec) {
	Type type = makeType(ERROR_KIND);
	switch (typeSpec->baseType) {
		case BOOLEAN_TYPE: type = makeType(BOOLEAN_KIND); break;
		case DECK_TYPE: type = makeDeckType(NULL); break;
		case DIE_TYPE: type = makeType(DIE_KIND); break;
		case INTEGER_TYPE: type = makeType(INTEGER_KIND); break;
		case NAMED_TYPE: type = makeCardType(typeSpec->name); break;
		case PIECE_TYPE: type = makeType(PIECE_KIND); break;
		case PLAYER_TYPE: type = makeType(PLAYER_KIND); break;
		case STRATEGY_TYPE: type = makeType(STRATEGY_KIND); break;
		case STRING_TYPE: type = makeType(STRING_KIND); break;
	}
	return typeSpec->isArray ? makeArrayType(type) : type;
}

/** El tipo de C de un valor de Meeple. Los arreglos y los mazos son la misma lista. */
static void _printCType(const Type type) {
	if (type.isArray || type.kind == DECK_KIND) {
		_print("MpList *");
		return;
	}
	switch (type.kind) {
		case BOOLEAN_KIND: _print("bool"); break;
		case CARD_KIND: _print("g%d_%s *", _gameIndex, type.cardType); break;
		case DIE_KIND: _print("const MpDie *"); break;
		case PIECE_KIND: _print("MpPiece *"); break;
		case PLAYER_KIND: _print("MpPlayer *"); break;
		case STRING_KIND: _print("const char *"); break;
		case NONE_KIND: _print("void *"); break;
		default: _print("int"); break;
	}
}

/** El valor por defecto de cada tipo: 0, false, "", none o un arreglo vacio. */
static void _printDefault(const Type type) {
	if (type.isArray) {
		_print("mp_list_new()");
		return;
	}
	switch (type.kind) {
		case BOOLEAN_KIND: _print("false"); break;
		case STRING_KIND: _print("\"\""); break;
		case CARD_KIND:
		case PIECE_KIND:
		case PLAYER_KIND:
		case NONE_KIND:
			_print("NULL");
			break;
		default: _print("0"); break;
	}
}

/** La funcion que muestra un valor en un log o en un "prefer input". */
static void _printDescribe(const Type type) {
	switch (type.kind) {
		case BOOLEAN_KIND: _print("mp_describe_boolean"); break;
		case CARD_KIND: _print("h%d_describe_%s", _gameIndex, type.cardType); break;
		case PIECE_KIND: _print("mp_describe_piece"); break;
		case STRATEGY_KIND: _print("mp_describe_strategy"); break;
		case STRING_KIND: _print("mp_describe_string"); break;
		case PLAYER_KIND:
		case NONE_KIND:
			_print("mp_describe_player");
			break;
		default: _print("mp_describe_integer"); break;
	}
}

/**
 * Un MpValue (una opcion, un elemento de una lista, lo que devuelve un ask)
 * como valor del tipo de C: ".i", ".b", ".s" o el puntero convertido.
 */
static void _printUnwrapBegin(const Type type) {
	if (isReferenceType(type)) {
		_print("((");
		_printCType(type);
		_print(") (");
	}
	else {
		_print("(");
	}
}

static void _printUnwrapEnd(const Type type) {
	if (isReferenceType(type)) {
		_print(").p)");
	}
	else if (type.isArray || type.kind == DECK_KIND) {
		/* No pasa: las listas no guardan listas. */
		_print(").p");
	}
	else if (type.kind == BOOLEAN_KIND) {
		_print(").b");
	}
	else if (type.kind == STRING_KIND) {
		_print(").s");
	}
	else {
		_print(").i");
	}
}

static void _printUnwrap(const Type type, const char * value) {
	_printUnwrapBegin(type);
	_print("%s", value);
	_printUnwrapEnd(type);
}

/** Un valor de C como MpValue. Se cierra con ")". */
static void _printWrapBegin(const Type type) {
	if (isReferenceType(type) || type.kind == NONE_KIND || type.isArray || type.kind == DECK_KIND) {
		_print("MP_REFERENCE(");
	}
	else if (type.kind == BOOLEAN_KIND) {
		_print("MP_BOOLEAN(");
	}
	else if (type.kind == STRING_KIND) {
		_print("MP_STRING(");
	}
	else {
		_print("MP_INTEGER(");
	}
}

/* BUSQUEDAS EN EL AST */

/** Las metricas van de 0 en adelante, en el orden de la declaracion. */
static int _metricIndex(GameDeclaration * game, const char * name) {
	int index = 0;
	for (GameItem * item = game->items; item != NULL; item = item->next) {
		if (item->type == METRIC_ITEM) {
			if (strcmp(item->metric.name, name) == 0) {
				return index;
			}
			++index;
		}
	}
	return -1;
}

/** Las estrategias van de 1 en adelante, en el orden de la declaracion (0 es "sin estrategia"). */
static int _strategyIndex(const char * name) {
	int index = 0;
	for (GameItem * item = _game->items; item != NULL; item = item->next) {
		if (item->type == STRATEGY_ITEM) {
			++index;
			if (strcmp(item->strategy.name, name) == 0) {
				return index;
			}
		}
	}
	return 0;
}

/** Cada politica del game tiene un numero, en el orden en que aparece. */
static int _policyNumber(GameItem * strategy, Policy * policy) {
	int number = 0;
	for (GameItem * item = _game->items; item != NULL; item = item->next) {
		if (item->type != STRATEGY_ITEM) {
			continue;
		}
		for (Policy * other = item->strategy.policies; other != NULL; other = other->next, ++number) {
			if (item == strategy && other == policy) {
				return number;
			}
		}
	}
	return -1;
}

static Policy * _findPolicy(GameItem * strategy, const char * decision) {
	for (Policy * policy = strategy->strategy.policies; policy != NULL; policy = policy->next) {
		if (strcmp(policy->decision, decision) == 0) {
			return policy;
		}
	}
	return NULL;
}

static GameItem * _findCardType(const char * name) {
	for (GameItem * item = _game->items; item != NULL; item = item->next) {
		if (item->type == CARD_TYPE_ITEM && strcmp(item->cardType.name, name) == 0) {
			return item;
		}
	}
	return NULL;
}

/** El primer item de ese tipo (board, prepare, turn...), o NULL si el game no lo declara. */
static GameItem * _findItem(const GameItemType type) {
	for (GameItem * item = _game->items; item != NULL; item = item->next) {
		if (item->type == type) {
			return item;
		}
	}
	return NULL;
}

/* EXPRESIONES */

static void _binary(Expression * expression, const char * operator) {
	_print("(");
	_expression(expression->left);
	_print(" %s ", operator);
	_expression(expression->right);
	_print(")");
}

/** Las cadenas se comparan por contenido; el resto (incluidas las referencias), por valor. */
static void _equality(Expression * expression) {
	const bool equal = expression->type == EQUAL_EXPRESSION;
	if (expression->left->semanticType.kind == STRING_KIND) {
		_print("(strcmp(");
		_expression(expression->left);
		_print(", ");
		_expression(expression->right);
		_print(") %s 0)", equal ? "==" : "!=");
		return;
	}
	_binary(expression, equal ? "==" : "!=");
}

static void _identifier(Expression * expression) {
	switch (expression->resolution) {
		case LOCAL_RESOLUTION:
			_print("v_%s", expression->identifier);
			break;
		case DIE_RESOLUTION:
			_print("(&g%d_%s)", _gameIndex, expression->identifier);
			break;
		case STRATEGY_RESOLUTION:
			_print("%d", _strategyIndex(expression->identifier));
			break;
		default:
			/* Variables del game, mazos y fichas. */
			_print("g%d_%s", _gameIndex, expression->identifier);
			break;
	}
}

/** Un jugador al que se le accede un miembro: error de ejecucion si es none. */
static void _playerOf(Expression * object, const int line) {
	_print("((MpPlayer *) mp_check(");
	_expression(object);
	_print(", %d))", line);
}

static void _member(Expression * expression) {
	Expression * object = expression->postfix.object;
	const char * member = expression->postfix.member;
	const Type objectType = object->semanticType;
	const int line = _lineOf(expression);
	switch (expression->resolution) {
		case PER_PLAYER_DECK_RESOLUTION:
			_print("g%d_%s[", _gameIndex, member);
			_playerOf(object, line);
			_print("->index]");
			return;
		case PER_PLAYER_PIECE_RESOLUTION:
			_print("(&g%d_%s[", _gameIndex, member);
			_playerOf(object, line);
			_print("->index])");
			return;
		case CARD_FIELD_RESOLUTION:
			_print("((");
			_printCType(objectType);
			_print(") mp_check(");
			_expression(object);
			_print(", %d))->f_%s", line, member);
			return;
		default:
			break;
	}
	switch (objectType.kind) {
		case PLAYER_KIND:
			_playerOf(object, line);
			_print("->%s", member);
			break;
		case PIECE_KIND:
			_print("((MpPiece *) mp_check(");
			_expression(object);
			_print(", %d))->%s", line, member);
			break;
		case BOARD_KIND:
			/* board.size; board.cell(i) es una llamada. */
			_print("mp_board_size");
			break;
		case DIE_KIND:
			if (strcmp(member, "faces") == 0) {
				_print("mp_die_faces(");
				_expression(object);
				_print(")");
			}
			else {
				_expression(object);
				_print("->%s", member);
			}
			break;
		case DECK_KIND:
			if (strcmp(member, "cardtype") == 0) {
				_printString(objectType.cardType);
			}
			else {
				_print("(");
				_expression(object);
				_print(")->size");
			}
			break;
		default:
			logError(_logger, "Linea %d: miembro \"%s\" sin traduccion.", line, member);
			_print("0");
			break;
	}
}

/**
 * count, sum, max, min y select, como statement expressions. La variable
 * ligada se declara en el cuerpo del bucle. max y min se quedan con el primero
 * que tiene el mejor valor; si no hay ninguno, devuelven none, o es un error
 * de ejecucion si los elementos no son referencias.
 */
static void _aggregation(Expression * expression) {
	const AggregationType type = expression->aggregation.type;
	const Type element = elementType(expression->aggregation.collection->semanticType);
	const char * variable = expression->aggregation.variable;
	const int line = _lineOf(expression);
	const bool ranked = type == MAX_AGGREGATION || type == MIN_AGGREGATION;
	const unsigned int result = _temporary();
	const unsigned int key = ranked ? _temporary() : 0;
	const unsigned int found = ranked ? _temporary() : 0;
	const unsigned int list = _temporary();
	const unsigned int index = _temporary();
	_print("({ ");
	switch (type) {
		case COUNT_AGGREGATION:
		case SUM_AGGREGATION:
			_print("int _t%u = 0; ", result);
			break;
		case SELECT_AGGREGATION:
			_print("MpList * _t%u = mp_list_new(); ", result);
			break;
		case MAX_AGGREGATION:
		case MIN_AGGREGATION:
			_printCType(element);
			_print(" _t%u = ", result);
			_printDefault(element);
			_print("; int _t%u = 0; bool _t%u = false; ", key, found);
			break;
	}
	_print("MpList * _t%u = ", list);
	_expression(expression->aggregation.collection);
	_print("; for (int _t%u = 0; _t%u < _t%u->size; ++_t%u) { ", index, index, list, index);
	_printCType(element);
	_print(" v_%s = ", variable);
	_printUnwrapBegin(element);
	_print("_t%u->items[_t%u]", list, index);
	_printUnwrapEnd(element);
	_print("; (void) v_%s; ", variable);
	if (expression->aggregation.where != NULL) {
		_print("if (");
		_expression(expression->aggregation.where);
		_print(") { ");
	}
	switch (type) {
		case COUNT_AGGREGATION:
			_print("++_t%u; ", result);
			break;
		case SUM_AGGREGATION:
			_print("_t%u += ", result);
			_expression(expression->aggregation.by);
			_print("; ");
			break;
		case SELECT_AGGREGATION:
			_print("mp_list_push(_t%u, ", result);
			_printWrapBegin(element);
			_print("v_%s)); ", variable);
			break;
		case MAX_AGGREGATION:
		case MIN_AGGREGATION: {
			const unsigned int value = _temporary();
			_print("const int _t%u = ", value);
			_expression(expression->aggregation.by);
			_print("; if (!_t%u || _t%u %s _t%u) { _t%u = v_%s; _t%u = _t%u; _t%u = true; } ",
				found, value, type == MAX_AGGREGATION ? ">" : "<", key, result, variable, key, value, found);
			break;
		}
	}
	if (expression->aggregation.where != NULL) {
		_print("} ");
	}
	_print("} ");
	if (ranked && !isReferenceType(element)) {
		_print("if (!_t%u) { mp_error(%d, \"%s de una coleccion sin elementos\"); } ",
			found, line, type == MAX_AGGREGATION ? "max" : "min");
	}
	_print("_t%u; })", result);
}

/**
 * Traduce una expresion a una expresion de C del tipo que anoto el analisis
 * semantico. Los accesos a miembros, los indices, las divisiones y las
 * celdas llevan la linea, para los errores de ejecucion.
 */
static void _expression(Expression * expression) {
	const int line = _lineOf(expression);
	switch (expression->type) {
		case ADDITION_EXPRESSION: _binary(expression, "+"); break;
		case SUBTRACTION_EXPRESSION: _binary(expression, "-"); break;
		case MULTIPLICATION_EXPRESSION: _binary(expression, "*"); break;
		case GREATER_EQUAL_EXPRESSION: _binary(expression, ">="); break;
		case GREATER_EXPRESSION: _binary(expression, ">"); break;
		case LESS_EQUAL_EXPRESSION: _binary(expression, "<="); break;
		case LESS_EXPRESSION: _binary(expression, "<"); break;
		/* and y or cortan como en C. */
		case AND_EXPRESSION: _binary(expression, "&&"); break;
		case OR_EXPRESSION: _binary(expression, "||"); break;
		case EQUAL_EXPRESSION:
		case NOT_EQUAL_EXPRESSION:
			_equality(expression);
			break;
		case DIVISION_EXPRESSION:
		case MODULE_EXPRESSION:
			_print("%s(", expression->type == DIVISION_EXPRESSION ? "mp_div" : "mp_mod");
			_expression(expression->left);
			_print(", ");
			_expression(expression->right);
			_print(", %d)", line);
			break;
		case NEGATION_EXPRESSION:
			_print("(-");
			_expression(expression->operand);
			_print(")");
			break;
		case NOT_EXPRESSION:
			_print("(!");
			_expression(expression->operand);
			_print(")");
			break;
		case CALL_EXPRESSION:
			/* La unica llamada es board.cell(i). */
			_print("mp_cell(");
			_expression(expression->postfix.arguments->expression);
			_print(", %d)", line);
			break;
		case INDEX_EXPRESSION:
			_printUnwrapBegin(expression->semanticType);
			_print("mp_list_get(");
			_expression(expression->postfix.object);
			_print(", ");
			_expression(expression->postfix.index);
			_print(", %d)", line);
			_printUnwrapEnd(expression->semanticType);
			break;
		case MEMBER_EXPRESSION:
			_member(expression);
			break;
		case ASK_EXPRESSION:
			_printUnwrapBegin(expression->semanticType);
			_print("mp_ask(&g%d_%s, ", _gameIndex, expression->ask.decision);
			_expression(expression->ask.player);
			_print(", ");
			_expression(expression->ask.arguments->expression);
			_print(", %d)", line);
			_printUnwrapEnd(expression->semanticType);
			break;
		case AGGREGATION_EXPRESSION:
			_aggregation(expression);
			break;
		case BOOLEAN_EXPRESSION:
			_print("%s", expression->boolean ? "true" : "false");
			break;
		case CURRENT_EXPRESSION:
			_print("current");
			break;
		case IDENTIFIER_EXPRESSION:
			_identifier(expression);
			break;
		case INTEGER_EXPRESSION:
			_print("%d", expression->integer);
			break;
		case NONE_EXPRESSION:
			_print("NULL");
			break;
		case OPTION_EXPRESSION:
			_printUnwrap(expression->semanticType, "option");
			break;
		case PLAYERS_EXPRESSION:
			_print("mp_player_list");
			break;
		case ROLL_EXPRESSION:
			_print("mp_roll(&g%d_%s)", _gameIndex, expression->die);
			break;
		case STRING_EXPRESSION:
			_printString(expression->string);
			break;
		case TURNS_EXPRESSION:
			_print("mp_turns");
			break;
		case BOARD_EXPRESSION:
			/* Solo aparece como objeto de board.size o board.cell(i). */
			_print("0");
			break;
	}
}

/**
 * El valor que se guarda en un lugar de tipo "target". Asignar un arreglo (o
 * un mazo a un arreglo) lo copia: si no, "lista = players; lista[0] = none;"
 * cambiaria players. Un select ya es una lista nueva.
 */
static void _value(const Type target, Expression * value) {
	const bool copy = target.isArray && value->type != AGGREGATION_EXPRESSION;
	if (copy) {
		_print("mp_list_copy(");
	}
	_expression(value);
	if (copy) {
		_print(")");
	}
}

/* SENTENCIAS */

static void _statements(Statement * statement) {
	for (; statement != NULL; statement = statement->next) {
		_statement(statement);
	}
}

/** El cuerpo de un if, un for o un while: las llaves ya las puso quien lo llama. */
static void _body(Statement * statement) {
	if (statement->type == BLOCK_STATEMENT) {
		_statements(statement->statements);
	}
	else {
		_statement(statement);
	}
}

static void _assignment(Statement * statement) {
	Expression * target = statement->assignment.target;
	Expression * value = statement->assignment.value;
	_indent();
	if (target->type == INDEX_EXPRESSION) {
		/* a[i] = x: el indice se valida en el runtime. */
		_print("mp_list_set(");
		_expression(target->postfix.object);
		_print(", ");
		_expression(target->postfix.index);
		_print(", ");
		_printWrapBegin(target->semanticType);
		_value(target->semanticType, value);
		_print("), %d);\n", _lineOf(target));
		return;
	}
	/* Una variable, el score de un jugador o el campo de una carta: son lvalues de C. */
	_expression(target);
	_print(" = ");
	_value(target->semanticType, value);
	_print(";\n");
}

static void _declaration(VariableDeclaration * declaration) {
	const Type type = _typeOfSpec(declaration->type);
	_indent();
	_printCType(type);
	_print(" v_%s = ", declaration->name);
	if (declaration->initializer == NULL) {
		_printDefault(type);
	}
	else {
		_value(type, declaration->initializer);
	}
	_print(";\n");
	_printLine("(void) v_%s;", declaration->name);
}

/** for (x in C): recorre una copia, asi el cuerpo puede cambiar la coleccion. */
static void _forEach(Statement * statement) {
	const Type element = elementType(statement->forLoop.collection->semanticType);
	const unsigned int list = _temporary();
	const unsigned int position = _temporary();
	_printLine("{");
	++_level;
	_indent();
	_print("MpList * _t%u = mp_list_copy(", list);
	_expression(statement->forLoop.collection);
	_print(");\n");
	_printLine("for (int _t%u = 0; _t%u < _t%u->size; ++_t%u) {", position, position, list, position);
	++_level;
	_indent();
	_printCType(element);
	_print(" v_%s = ", statement->forLoop.variable);
	_printUnwrapBegin(element);
	_print("_t%u->items[_t%u]", list, position);
	_printUnwrapEnd(element);
	_print(";\n");
	_printLine("(void) v_%s;", statement->forLoop.variable);
	_body(statement->forLoop.body);
	--_level;
	_printLine("}");
	--_level;
	_printLine("}");
}

/**
 * for (i in a to b): los extremos se evaluan una vez, en orden. El contador es
 * un long long para que "to" igual al maximo entero no desborde.
 */
static void _forRange(Statement * statement) {
	const unsigned int from = _temporary();
	const unsigned int to = _temporary();
	const unsigned int counter = _temporary();
	_printLine("{");
	++_level;
	_indent();
	_print("const int _t%u = ", from);
	_expression(statement->forLoop.from);
	_print(";\n");
	_indent();
	_print("const int _t%u = ", to);
	_expression(statement->forLoop.to);
	_print(";\n");
	_printLine("for (long long _t%u = _t%u; _t%u <= _t%u; ++_t%u) {", counter, from, counter, to, counter);
	++_level;
	_printLine("const int v_%s = (int) _t%u;", statement->forLoop.variable, counter);
	_printLine("(void) v_%s;", statement->forLoop.variable);
	_body(statement->forLoop.body);
	--_level;
	_printLine("}");
	--_level;
	_printLine("}");
}

/**
 * Un log evalua todas sus partes, haya verbose o no; el runtime solo las
 * imprime con verbose. Asi la simulacion da lo mismo en los dos modos.
 */
static void _log(LogPart * parts) {
	for (LogPart * part = parts; part != NULL; part = part->next) {
		if (part->type == TEXT_LOG_PART) {
			if (part->text[0] != '\0') {
				_indent();
				_print("mp_log_text(");
				_printString(part->text);
				_print(");\n");
			}
			continue;
		}
		const Type type = part->expression->semanticType;
		_indent();
		_print("mp_log_value(");
		_printDescribe(type);
		_print(", ");
		_printWrapBegin(type);
		_expression(part->expression);
		_print("));\n");
	}
	_printLine("mp_log_end();");
}

static void _uses(Statement * statement) {
	_indent();
	_print("mp_uses(");
	_expression(statement->assignment.target);
	_print(", ");
	const char * name = statement->assignment.strategy;
	switch (statement->assignment.strategyResolution) {
		case LOCAL_RESOLUTION: _print("v_%s", name); break;
		case GAME_VARIABLE_RESOLUTION: _print("g%d_%s", _gameIndex, name); break;
		default: _print("%d", _strategyIndex(name)); break;
	}
	_print(", %d);\n", statement->line);
}

static void _action(Statement * statement) {
	const int line = statement->line;
	_indent();
	switch (statement->type) {
		case DRAW_STATEMENT:
			_print("mp_draw(");
			_expression(statement->action.amount);
			_print(", ");
			_expression(statement->action.source);
			_print(", ");
			_expression(statement->action.target);
			_print(");\n");
			break;
		case PLAY_STATEMENT:
			_print("mp_play(");
			_expression(statement->action.subject);
			_print(", ");
			_expression(statement->action.source);
			_print(", ");
			_expression(statement->action.target);
			_print(");\n");
			break;
		case SHUFFLE_STATEMENT:
			_print("mp_shuffle(");
			_expression(statement->action.subject);
			_print(");\n");
			break;
		case GIVE_STATEMENT:
			_print("mp_give(");
			_expression(statement->action.target);
			_print(", ");
			_expression(statement->action.amount);
			_print(", %d);\n", line);
			break;
		case TAKE_STATEMENT:
			_print("mp_take(");
			_expression(statement->action.source);
			_print(", ");
			_expression(statement->action.amount);
			_print(", %d);\n", line);
			break;
		case MOVE_STATEMENT:
			_print("mp_move(");
			_expression(statement->action.subject);
			_print(", ");
			_expression(statement->action.amount);
			_print(", %d);\n", line);
			break;
		case PLACE_STATEMENT:
			_print("mp_place(");
			_expression(statement->action.subject);
			_print(", ");
			_expression(statement->action.target);
			_print(", %d);\n", line);
			break;
		default:
			_print(";\n");
			break;
	}
}

static void _statement(Statement * statement) {
	_line = statement->line;
	switch (statement->type) {
		case ASSIGNMENT_STATEMENT:
			_assignment(statement);
			break;
		case BLOCK_STATEMENT:
			_printLine("{");
			++_level;
			_statements(statement->statements);
			--_level;
			_printLine("}");
			break;
		case DECLARATION_STATEMENT:
			_declaration(statement->declaration);
			break;
		case FOR_EACH_STATEMENT:
			_forEach(statement);
			break;
		case FOR_RANGE_STATEMENT:
			_forRange(statement);
			break;
		case IF_STATEMENT:
			_indent();
			_print("if (");
			_expression(statement->ifStatement.condition);
			_print(") {\n");
			++_level;
			_body(statement->ifStatement.thenBranch);
			--_level;
			if (statement->ifStatement.elseBranch != NULL) {
				_printLine("}");
				_printLine("else {");
				++_level;
				_body(statement->ifStatement.elseBranch);
				--_level;
			}
			_printLine("}");
			break;
		case LOG_STATEMENT:
			_log(statement->log);
			break;
		case REPEAT_STATEMENT: {
			/* repeat n: n se evalua una vez; sin indice. */
			const unsigned int count = _temporary();
			const unsigned int counter = _temporary();
			_indent();
			_print("for (int _t%u = 0, _t%u = ", counter, count);
			_expression(statement->loop.condition);
			_print("; _t%u < _t%u; ++_t%u) {\n", counter, count, counter);
			++_level;
			_body(statement->loop.body);
			--_level;
			_printLine("}");
			break;
		}
		case USES_STATEMENT:
			_uses(statement);
			break;
		case WHILE_STATEMENT:
			_indent();
			_print("while (");
			_expression(statement->loop.condition);
			_print(") {\n");
			++_level;
			_body(statement->loop.body);
			--_level;
			_printLine("}");
			break;
		case DRAW_STATEMENT:
		case GIVE_STATEMENT:
		case MOVE_STATEMENT:
		case PLACE_STATEMENT:
		case PLAY_STATEMENT:
		case SHUFFLE_STATEMENT:
		case TAKE_STATEMENT:
			_action(statement);
			break;
	}
}

/* EL GAME */

/** Un struct por cardtype y la funcion que muestra sus cartas ({ataque: 3, costo: 2}). */
static void _cardTypes() {
	for (GameItem * item = _game->items; item != NULL; item = item->next) {
		if (item->type != CARD_TYPE_ITEM) {
			continue;
		}
		const char * name = item->cardType.name;
		_printLine("/* cardtype %s */", name);
		_printLine("typedef struct {");
		++_level;
		for (Field * field = item->cardType.fields; field != NULL; field = field->next) {
			_indent();
			_printCType(_typeOfSpec(field->type));
			_print(" f_%s;\n", field->name);
		}
		--_level;
		_printLine("} g%d_%s;", _gameIndex, name);
		_blankLine();
		_printLine("static void h%d_describe_%s(FILE * out, MpValue value) {", _gameIndex, name);
		++_level;
		_printLine("const g%d_%s * card = value.p;", _gameIndex, name);
		_printLine("if (card == NULL) {");
		_printLine("\tfputs(\"none\", out);");
		_printLine("\treturn;");
		_printLine("}");
		_indent();
		_print("fprintf(out, \"{");
		for (Field * field = item->cardType.fields; field != NULL; field = field->next) {
			const bool isInteger = field->type->baseType == INTEGER_TYPE;
			_print("%s%s: %s", field == item->cardType.fields ? "" : ", ", field->name, isInteger ? "%d" : "%s");
		}
		_print("}\"");
		for (Field * field = item->cardType.fields; field != NULL; field = field->next) {
			if (field->type->baseType == BOOLEAN_TYPE) {
				_print(", card->f_%s ? \"true\" : \"false\"", field->name);
			}
			else {
				_print(", card->f_%s", field->name);
			}
		}
		_print(");\n");
		--_level;
		_printLine("}");
		_blankLine();
	}
}

/**
 * Los dados son constantes. Los mazos, las fichas y las variables del juego
 * se arman de nuevo al empezar cada partida (en h<k>_reset).
 */
static void _components() {
	for (GameItem * item = _game->items; item != NULL; item = item->next) {
		switch (item->type) {
			case DIE_ITEM: {
				const char * name = item->die.name;
				ValueSet * faces = item->die.faces;
				int count = 0;
				int min = 0;
				int max = 0;
				_indent();
				_print("static const int g%d_%s_faces[] = { ", _gameIndex, name);
				if (faces->type == RANGE_VALUE_SET) {
					for (int face = faces->min; face <= faces->max; ++face, ++count) {
						_print("%s%d", count == 0 ? "" : ", ", face);
					}
					min = faces->min;
					max = faces->max;
				}
				else {
					for (Literal * face = faces->elements; face != NULL; face = face->next, ++count) {
						_print("%s%d", count == 0 ? "" : ", ", face->integer);
						if (count == 0 || face->integer < min) {
							min = face->integer;
						}
						if (count == 0 || face->integer > max) {
							max = face->integer;
						}
					}
				}
				_print(" };\n");
				_printLine("static const MpDie g%d_%s = { g%d_%s_faces, %d, %d, %d };", _gameIndex, name, _gameIndex, name, count, min, max);
				break;
			}
			case DECK_ITEM:
				_printLine("static MpList *%s g%d_%s;", item->deck->perPlayer ? "*" : "", _gameIndex, item->deck->name);
				break;
			case PIECE_ITEM:
				_printLine("static MpPiece * g%d_%s;", _gameIndex, item->piece.name);
				break;
			case VARIABLE_ITEM:
				_indent();
				_print("static ");
				_printCType(_typeOfSpec(item->variable->type));
				_print(" g%d_%s;\n", _gameIndex, item->variable->name);
				break;
			default:
				break;
		}
	}
	_blankLine();
}

/** Los nombres de las estrategias (la 1 es la primera) y de las metricas (la 0 es la primera). */
static void _names() {
	const GameItemType types[] = { STRATEGY_ITEM, METRIC_ITEM };
	const char * arrays[] = { "strategy_names", "metric_names" };
	for (int k = 0; k < 2; ++k) {
		if (_findItem(types[k]) == NULL) {
			continue;
		}
		_indent();
		_print("static const char * const h%d_%s[] = { ", _gameIndex, arrays[k]);
		bool first = true;
		for (GameItem * item = _game->items; item != NULL; item = item->next) {
			if (item->type == types[k]) {
				_print("%s", first ? "" : ", ");
				_printString(types[k] == STRATEGY_ITEM ? item->strategy.name : item->metric.name);
				first = false;
			}
		}
		_print(" };\n");
	}
	_blankLine();
}

/**
 * Cada politica de cada strategy: una funcion para el where y una por
 * criterio max o min, que reciben la opcion y el jugador que decide.
 */
static void _policies() {
	for (GameItem * item = _game->items; item != NULL; item = item->next) {
		if (item->type != STRATEGY_ITEM) {
			continue;
		}
		_line = item->line;
		for (Policy * policy = item->strategy.policies; policy != NULL; policy = policy->next) {
			const int number = _policyNumber(item, policy);
			_printLine("/* strategy %s, %s */", item->strategy.name, policy->decision);
			if (policy->where != NULL) {
				_printLine("static bool h%d_where%d(MpValue option, MpPlayer * current) {", _gameIndex, number);
				++_level;
				_printLine("(void) option;");
				_printLine("(void) current;");
				_indent();
				_print("return ");
				_expression(policy->where);
				_print(";\n");
				--_level;
				_printLine("}");
			}
			int rankings = 0;
			for (Criterion * criterion = policy->criteria; criterion != NULL; criterion = criterion->next) {
				if (criterion->type != MAX_CRITERION && criterion->type != MIN_CRITERION) {
					continue;
				}
				_printLine("static int h%d_key%d_%d(MpValue option, MpPlayer * current) {", _gameIndex, number, rankings);
				++_level;
				_printLine("(void) option;");
				_printLine("(void) current;");
				_indent();
				_print("return ");
				_expression(criterion->expression);
				_print(";\n");
				--_level;
				_printLine("}");
				++rankings;
			}
			if (rankings > 0) {
				_indent();
				_print("static const MpRanking h%d_rankings%d[] = { ", _gameIndex, number);
				int ranking = 0;
				for (Criterion * criterion = policy->criteria; criterion != NULL; criterion = criterion->next) {
					if (criterion->type == MAX_CRITERION || criterion->type == MIN_CRITERION) {
						_print("%s{ h%d_key%d_%d, %s }", ranking == 0 ? "" : ", ", _gameIndex, number, ranking,
							criterion->type == MAX_CRITERION ? "true" : "false");
						++ranking;
					}
				}
				_print(" };\n");
			}
			_blankLine();
		}
	}
}

/**
 * Cada decision tiene una tabla de politicas indexada por la estrategia del
 * jugador. La posicion 0 es la de un jugador sin "uses": "prefer first".
 */
static void _decisions() {
	for (GameItem * item = _game->items; item != NULL; item = item->next) {
		if (item->type != DECISION_ITEM) {
			continue;
		}
		const char * name = item->decision.name;
		const Type type = _typeOfSpec(item->decision.type);
		_printLine("/* decision %s */", name);
		_printLine("static const MpPolicy h%d_policies_%s[] = {", _gameIndex, name);
		++_level;
		_indent();
		_print("{ NULL, NULL, 0, MP_FIRST }");
		for (GameItem * strategy = _game->items; strategy != NULL; strategy = strategy->next) {
			if (strategy->type != STRATEGY_ITEM) {
				continue;
			}
			Policy * policy = _findPolicy(strategy, name);
			const int number = _policyNumber(strategy, policy);
			int rankings = 0;
			const char * final = "MP_FIRST";
			for (Criterion * criterion = policy->criteria; criterion != NULL; criterion = criterion->next) {
				switch (criterion->type) {
					case MAX_CRITERION:
					case MIN_CRITERION:
						++rankings;
						break;
					case RANDOM_CRITERION:
						final = "MP_RANDOM";
						break;
					case INPUT_CRITERION:
						final = "MP_INPUT";
						break;
					case FIRST_CRITERION:
						final = "MP_FIRST";
						break;
				}
			}
			_print(",\n");
			_indent();
			_print("{ ");
			if (policy->where != NULL) {
				_print("h%d_where%d, ", _gameIndex, number);
			}
			else {
				_print("NULL, ");
			}
			if (rankings > 0) {
				_print("h%d_rankings%d, %d, ", _gameIndex, number, rankings);
			}
			else {
				_print("NULL, 0, ");
			}
			_print("%s }", final);
		}
		_print("\n");
		--_level;
		_printLine("};");
		_indent();
		_print("static const MpDecision g%d_%s = { ", _gameIndex, name);
		_printString(name);
		_print(", %s, ", isReferenceType(type) ? "true" : "false");
		_printDescribe(type);
		_print(", h%d_policies_%s };\n", _gameIndex, name);
		_blankLine();
	}
}

/** Una carta nueva al final del mazo, con sus campos en el orden en que estan escritos. */
static void _card(const char * deck, GameItem * cardType, Card * card) {
	const unsigned int created = _temporary();
	_printLine("{");
	++_level;
	_printLine("g%d_%s * _t%u = mp_alloc(sizeof(g%d_%s));", _gameIndex, cardType->cardType.name, created, _gameIndex, cardType->cardType.name);
	if (card->type == NAMED_CARD) {
		for (CardField * value = card->fields; value != NULL; value = value->next) {
			_indent();
			_print("_t%u->f_%s = ", created, value->name);
			_expression(value->value);
			_print(";\n");
		}
	}
	else {
		Field * field = cardType->cardType.fields;
		for (ExpressionList * value = card->values; value != NULL && field != NULL; value = value->next, field = field->next) {
			_indent();
			_print("_t%u->f_%s = ", created, field->name);
			_expression(value->expression);
			_print(";\n");
		}
	}
	_printLine("mp_list_push(g%d_%s, MP_REFERENCE(_t%u));", _gameIndex, deck, created);
	--_level;
	_printLine("}");
}

/**
 * El producto cartesiano de los generadores: un bucle por generador, con el
 * primero como el de afuera. "values" guarda el temporal de cada uno.
 */
static void _generators(const char * deck, GameItem * cardType, Generator * generator, Generator * generators, unsigned int * values) {
	if (generator == NULL) {
		const unsigned int created = _temporary();
		_printLine("g%d_%s * _t%u = mp_alloc(sizeof(g%d_%s));", _gameIndex, cardType->cardType.name, created, _gameIndex, cardType->cardType.name);
		int k = 0;
		for (Generator * each = generators; each != NULL; each = each->next, ++k) {
			_printLine("_t%u->f_%s = _t%u;", created, each->field, values[k]);
		}
		_printLine("mp_list_push(g%d_%s, MP_REFERENCE(_t%u));", _gameIndex, deck, created);
		return;
	}
	int depth = 0;
	for (Generator * each = generators; each != generator; each = each->next) {
		++depth;
	}
	ValueSet * set = generator->values;
	const unsigned int value = _temporary();
	values[depth] = value;
	if (set->type == RANGE_VALUE_SET) {
		_printLine("for (int _t%u = %d; _t%u <= %d; ++_t%u) {", value, set->min, value, set->max, value);
	}
	else {
		const unsigned int array = _temporary();
		const unsigned int index = _temporary();
		int count = 0;
		_indent();
		switch (set->elements->type) {
			case BOOLEAN_LITERAL: _print("static const bool"); break;
			case INTEGER_LITERAL: _print("static const int"); break;
			case STRING_LITERAL: _print("static const char * const"); break;
		}
		_print(" _t%u[] = { ", array);
		for (Literal * literal = set->elements; literal != NULL; literal = literal->next, ++count) {
			_print("%s", count == 0 ? "" : ", ");
			switch (literal->type) {
				case BOOLEAN_LITERAL: _print("%s", literal->boolean ? "true" : "false"); break;
				case INTEGER_LITERAL: _print("%d", literal->integer); break;
				case STRING_LITERAL: _printString(literal->string); break;
			}
		}
		_print(" };\n");
		_printLine("for (int _t%u = 0; _t%u < %d; ++_t%u) {", index, index, count, index);
		++_level;
		switch (set->elements->type) {
			case BOOLEAN_LITERAL: _printLine("const bool _t%u = _t%u[_t%u];", value, array, index); break;
			case INTEGER_LITERAL: _printLine("const int _t%u = _t%u[_t%u];", value, array, index); break;
			case STRING_LITERAL: _printLine("const char * const _t%u = _t%u[_t%u];", value, array, index); break;
		}
		--_level;
	}
	++_level;
	_generators(deck, cardType, generator->next, generators, values);
	--_level;
	_printLine("}");
}

static void _deck(GameItem * item) {
	DeckDeclaration * deck = item->deck;
	if (deck->perPlayer) {
		/* Un mazo vacio por jugador. */
		_printLine("g%d_%s = mp_deck_per_player();", _gameIndex, deck->name);
		return;
	}
	_printLine("g%d_%s = mp_list_new();", _gameIndex, deck->name);
	GameItem * cardType = _findCardType(deck->cardType);
	if (deck->bodyType == CARDS_DECK_BODY) {
		for (Card * card = deck->cards; card != NULL; card = card->next) {
			_card(deck->name, cardType, card);
		}
	}
	else if (deck->bodyType == GENERATORS_DECK_BODY) {
		int count = 0;
		for (Generator * generator = deck->generators; generator != NULL; generator = generator->next) {
			++count;
		}
		unsigned int * values = calloc((size_t) count, sizeof(unsigned int));
		_printLine("{");
		++_level;
		_generators(deck->name, cardType, deck->generators, deck->generators, values);
		--_level;
		_printLine("}");
		free(values);
	}
}

/**
 * Al empezar cada partida: los mazos con sus cartas, sin mezclar; las fichas
 * en la celda 0; las variables del juego con su valor inicial. En el orden de
 * la declaracion, porque un valor inicial puede usar lo declarado antes.
 */
static void _reset() {
	_printLine("static void h%d_reset(void) {", _gameIndex);
	++_level;
	for (GameItem * item = _game->items; item != NULL; item = item->next) {
		_line = item->line;
		switch (item->type) {
			case DECK_ITEM:
				_deck(item);
				break;
			case PIECE_ITEM:
				_indent();
				_print("g%d_%s = %s(", _gameIndex, item->piece.name, item->piece.perPlayer ? "mp_piece_per_player" : "mp_piece_new");
				_printString(item->piece.name);
				_print(");\n");
				break;
			case VARIABLE_ITEM: {
				VariableDeclaration * variable = item->variable;
				const Type type = _typeOfSpec(variable->type);
				_indent();
				_print("g%d_%s = ", _gameIndex, variable->name);
				if (variable->initializer == NULL) {
					_printDefault(type);
				}
				else {
					_value(type, variable->initializer);
				}
				_print(";\n");
				break;
			}
			default:
				break;
		}
	}
	--_level;
	_printLine("}");
	_blankLine();
}

static void _prepare() {
	GameItem * item = _findItem(PREPARE_ITEM);
	if (item == NULL) {
		return;
	}
	_line = item->line;
	_printLine("static void h%d_prepare(void) {", _gameIndex);
	++_level;
	_body(item->block);
	--_level;
	_printLine("}");
	_blankLine();
}

static void _turn() {
	GameItem * item = _findItem(TURN_ITEM);
	if (item == NULL) {
		return;
	}
	_line = item->line;
	_printLine("static void h%d_turn(MpPlayer * current) {", _gameIndex);
	++_level;
	_printLine("(void) current;");
	_body(item->block);
	--_level;
	_printLine("}");
	_blankLine();
}

static void _win() {
	GameItem * item = _findItem(WIN_ITEM);
	if (item == NULL) {
		return;
	}
	_line = item->line;
	_printLine("static bool h%d_win(MpPlayer * current) {", _gameIndex);
	++_level;
	_printLine("(void) current;");
	_indent();
	_print("return ");
	_expression(item->condition);
	_print(";\n");
	--_level;
	_printLine("}");
	_blankLine();
}

/** Las metric, en orden, con el ultimo jugador que jugo. */
static void _metrics() {
	if (_findItem(METRIC_ITEM) == NULL) {
		return;
	}
	_printLine("static void h%d_metric_values(MpPlayer * current, int * values) {", _gameIndex);
	++_level;
	_printLine("(void) current;");
	int index = 0;
	for (GameItem * item = _game->items; item != NULL; item = item->next) {
		if (item->type != METRIC_ITEM) {
			continue;
		}
		_line = item->line;
		_indent();
		_print("values[%d] = ", index++);
		_expression(item->metric.value);
		_print(";\n");
	}
	--_level;
	_printLine("}");
	_blankLine();
}

static void _gameDescriptor() {
	GameItem * board = _findItem(BOARD_ITEM);
	GameItem * end = _findItem(END_ITEM);
	GameItem * order = _findItem(TURN_ORDER_ITEM);
	int strategies = 0;
	int metrics = 0;
	for (GameItem * item = _game->items; item != NULL; item = item->next) {
		strategies += item->type == STRATEGY_ITEM;
		metrics += item->type == METRIC_ITEM;
	}
	_printLine("static const MpGame h%d_game = {", _gameIndex);
	++_level;
	_indent();
	_print(".name = ");
	_printString(_game->name);
	_print(",\n");
	_printLine(".boardSize = %d,", board == NULL ? 0 : board->cells);
	if (end == NULL) {
		_printLine(".endAfter = MP_DEFAULT_END_AFTER,");
	}
	else {
		_printLine(".endAfter = %d,", end->turns);
	}
	_printLine(".clockwise = %s,", order == NULL || order->clockwise ? "true" : "false");
	if (strategies > 0) {
		_printLine(".strategyCount = %d,", strategies);
		_printLine(".strategyNames = h%d_strategy_names,", _gameIndex);
	}
	if (metrics > 0) {
		_printLine(".metricCount = %d,", metrics);
		_printLine(".metricNames = h%d_metric_names,", _gameIndex);
		_printLine(".metrics = h%d_metric_values,", _gameIndex);
	}
	if (_findItem(PREPARE_ITEM) != NULL) {
		_printLine(".prepare = h%d_prepare,", _gameIndex);
	}
	if (_findItem(TURN_ITEM) != NULL) {
		_printLine(".turn = h%d_turn,", _gameIndex);
	}
	if (_findItem(WIN_ITEM) != NULL) {
		_printLine(".win = h%d_win,", _gameIndex);
	}
	_printLine(".reset = h%d_reset", _gameIndex);
	--_level;
	_printLine("};");
	_blankLine();
}

static void _gameDeclaration(GameDeclaration * game, const int index) {
	_game = game;
	_gameIndex = index;
	_print("/* game ");
	_printString(game->name);
	_print(" */\n\n");
	_cardTypes();
	_components();
	_names();
	_policies();
	_decisions();
	_reset();
	_prepare();
	_turn();
	_win();
	_metrics();
	_gameDescriptor();
}

/** Los simulate y los report, en el orden del programa. Cada report usa el simulate anterior. */
static void _main(Program * program) {
	_printLine("int main(void) {");
	++_level;
	_printLine("MpStats * _s = NULL;");
	GameDeclaration * simulated = NULL;
	for (TopLevel * topLevel = program->items; topLevel != NULL; topLevel = topLevel->next) {
		if (topLevel->type == SIMULATION_TOP_LEVEL) {
			Simulation * simulation = topLevel->simulation;
			int index = 0;
			simulated = NULL;
			for (TopLevel * other = program->items; other != NULL; other = other->next) {
				if (other->type != GAME_TOP_LEVEL) {
					continue;
				}
				if (strcmp(other->game->name, simulation->gameName) == 0) {
					simulated = other->game;
					break;
				}
				++index;
			}
			_printLine("if (_s != NULL) {");
			_printLine("\tmp_stats_destroy(_s);");
			_printLine("}");
			_printLine("_s = mp_simulate(&h%d_game, %d, %d, %d, %s);", index, simulation->games, simulation->players,
				simulation->hasSeed ? simulation->seed : 0, simulation->verbose ? "true" : "false");
		}
		else if (topLevel->type == REPORT_TOP_LEVEL) {
			for (ReportItem * item = topLevel->report; item != NULL; item = item->next) {
				switch (item->type) {
					case WINRATE_REPORT_ITEM:
						_printLine("mp_report_winrate(_s);");
						break;
					case WINRATE_BY_PLAYER_REPORT_ITEM:
						_printLine("mp_report_winrate_by_player(_s);");
						break;
					case WINRATE_BY_STRATEGY_REPORT_ITEM:
						_printLine("mp_report_winrate_by_strategy(_s);");
						break;
					case AGGREGATE_REPORT_ITEM: {
						const char * aggregator = item->aggregator == AVG_REPORT_AGGREGATOR ? "MP_AVG"
							: item->aggregator == MIN_REPORT_AGGREGATOR ? "MP_MIN" : "MP_MAX";
						if (item->metric == NULL) {
							_printLine("mp_report_metric(_s, MP_TURNS, %s);", aggregator);
						}
						else {
							_printLine("mp_report_metric(_s, %d, %s);", _metricIndex(simulated, item->metric), aggregator);
						}
						break;
					}
				}
			}
		}
	}
	_printLine("if (_s != NULL) {");
	_printLine("\tmp_stats_destroy(_s);");
	_printLine("}");
	_printLine("return 0;");
	--_level;
	_printLine("}");
}

/* PUBLIC FUNCTIONS */

CompilationStatus executeGenerator(CompilerState * compilerState) {
	logDebugging(_logger, "Generating final output...");
	Program * program = compilerState->abstractSyntaxtTree;
	const char * path = compilerState->outputPath;
	/* Sin ruta de salida, el codigo se genera igual y se descarta (asi lo prueba test.sh). */
	_output = fopen(path == NULL ? "/dev/null" : path, "w");
	if (_output == NULL) {
		logError(_logger, "No se puede escribir el programa generado en \"%s\".", path == NULL ? "/dev/null" : path);
		return FAILED;
	}
	_level = 0;
	_temporaries = 0;
	_line = 0;
	_print("/* Generado por el compilador de Meeple. */\n\n");
	_print("#include \"MeepleRuntime.h\"\n\n");
	_print("/* Un game no tiene por que usar todo lo que declara, ni simularse. */\n");
	_print("#pragma GCC diagnostic ignored \"-Wunused-function\"\n");
	_print("#pragma GCC diagnostic ignored \"-Wunused-const-variable\"\n\n");
	int index = 0;
	for (TopLevel * topLevel = program->items; topLevel != NULL; topLevel = topLevel->next) {
		if (topLevel->type == GAME_TOP_LEVEL) {
			_gameDeclaration(topLevel->game, index++);
		}
	}
	_main(program);
	const bool failed = ferror(_output) != 0;
	if (fclose(_output) != 0 || failed) {
		logError(_logger, "No se pudo terminar de escribir el programa generado.");
		_output = NULL;
		return FAILED;
	}
	_output = NULL;
	if (path != NULL) {
		logInformation(_logger, "Programa generado en \"%s\".", path);
	}
	return SUCCEEDED;
}
