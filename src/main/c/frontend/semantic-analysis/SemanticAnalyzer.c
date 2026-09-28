#include "SemanticAnalyzer.h"

/* MODULE INTERNAL STATE */

static unsigned int _errors = 0;
static Logger * _logger = NULL;

/** Shutdown module's internal state. */
void _shutdownSemanticAnalyzerModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: SemanticAnalyzer...");
		destroyLogger(_logger);
		_logger = NULL;
	}
	_errors = 0;
}

ModuleDestructor initializeSemanticAnalyzerModule() {
	_logger = createLogger("SemanticAnalyzer");
	return _shutdownSemanticAnalyzerModule;
}

/* PRIVATE FUNCTIONS */

static void _checkAsksInCard(Card * card, GameItem * items, const int line);
static void _checkAsksInExpression(Expression * expression, GameItem * items, const int line);
static void _checkAsksInExpressionList(ExpressionList * expressionList, GameItem * items, const int line);
static void _checkAsksInStatement(Statement * statement, GameItem * items);
static void _checkAsksInStatements(Statement * statement, GameItem * items);
static void _checkGame(GameDeclaration * game);
static void _checkSimulation(Simulation * simulation, TopLevel * topLevels, const int line);
static void _checkStrategy(GameItem * strategy, GameItem * items);
static void _error(const int line, const char * const format, ...);
static GameItem * _findDecision(GameItem * items, const char * name);
static GameDeclaration * _findGame(TopLevel * topLevels, const char * name);
static ValueSet * _findPlayers(GameItem * items);
static Policy * _findPolicy(Policy * policies, const char * decision);
static bool _valueSetContains(ValueSet * valueSet, const int value);

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

/**
 * Busca el "decision <name>: ..." del game. NULL si no esta declarado.
 */
static GameItem * _findDecision(GameItem * items, const char * name) {
	for (GameItem * item = items; item != NULL; item = item->next) {
		if (item->type == DECISION_ITEM && strcmp(item->decision.name, name) == 0) {
			return item;
		}
	}
	return NULL;
}

/**
 * Busca la politica que resuelve una decision dentro de una strategy. NULL si
 * la strategy no la resuelve.
 */
static Policy * _findPolicy(Policy * policies, const char * decision) {
	for (Policy * policy = policies; policy != NULL; policy = policy->next) {
		if (strcmp(policy->decision, decision) == 0) {
			return policy;
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

/**
 * Caso 9: recorre una expresion y chequea que cada "ask" nombre un decision
 * declarado en el game. "line" es la linea de la sentencia o del item que
 * contiene la expresion, porque los nodos Expression no guardan la suya.
 */
static void _checkAsksInExpression(Expression * expression, GameItem * items, const int line) {
	if (expression == NULL) {
		return;
	}
	switch (expression->type) {
		case ADDITION_EXPRESSION:
		case AND_EXPRESSION:
		case DIVISION_EXPRESSION:
		case EQUAL_EXPRESSION:
		case GREATER_EQUAL_EXPRESSION:
		case GREATER_EXPRESSION:
		case LESS_EQUAL_EXPRESSION:
		case LESS_EXPRESSION:
		case MODULE_EXPRESSION:
		case MULTIPLICATION_EXPRESSION:
		case NOT_EQUAL_EXPRESSION:
		case OR_EXPRESSION:
		case SUBTRACTION_EXPRESSION:
			_checkAsksInExpression(expression->left, items, line);
			_checkAsksInExpression(expression->right, items, line);
			break;
		case NEGATION_EXPRESSION:
		case NOT_EXPRESSION:
			_checkAsksInExpression(expression->operand, items, line);
			break;
		case CALL_EXPRESSION:
			_checkAsksInExpression(expression->postfix.object, items, line);
			_checkAsksInExpressionList(expression->postfix.arguments, items, line);
			break;
		case INDEX_EXPRESSION:
			_checkAsksInExpression(expression->postfix.object, items, line);
			_checkAsksInExpression(expression->postfix.index, items, line);
			break;
		case MEMBER_EXPRESSION:
			_checkAsksInExpression(expression->postfix.object, items, line);
			break;
		case ASK_EXPRESSION:
			if (_findDecision(items, expression->ask.decision) == NULL) {
				_error(line, "ask a \"%s\", que no es una decision declarada en el game", expression->ask.decision);
			}
			_checkAsksInExpression(expression->ask.player, items, line);
			_checkAsksInExpressionList(expression->ask.arguments, items, line);
			break;
		case AGGREGATION_EXPRESSION:
			_checkAsksInExpression(expression->aggregation.collection, items, line);
			_checkAsksInExpression(expression->aggregation.where, items, line);
			_checkAsksInExpression(expression->aggregation.by, items, line);
			break;
		case BOARD_EXPRESSION:
		case BOOLEAN_EXPRESSION:
		case CURRENT_EXPRESSION:
		case IDENTIFIER_EXPRESSION:
		case INTEGER_EXPRESSION:
		case NONE_EXPRESSION:
		case OPTION_EXPRESSION:
		case PLAYERS_EXPRESSION:
		case ROLL_EXPRESSION:
		case STRING_EXPRESSION:
		case TURNS_EXPRESSION:
			/* Hojas: no contienen subexpresiones. */
			break;
	}
}

static void _checkAsksInExpressionList(ExpressionList * expressionList, GameItem * items, const int line) {
	for (ExpressionList * item = expressionList; item != NULL; item = item->next) {
		_checkAsksInExpression(item->expression, items, line);
	}
}

static void _checkAsksInStatements(Statement * statement, GameItem * items) {
	for (; statement != NULL; statement = statement->next) {
		_checkAsksInStatement(statement, items);
	}
}

static void _checkAsksInStatement(Statement * statement, GameItem * items) {
	if (statement == NULL) {
		return;
	}
	const int line = statement->line;
	switch (statement->type) {
		case ASSIGNMENT_STATEMENT:
			_checkAsksInExpression(statement->assignment.target, items, line);
			_checkAsksInExpression(statement->assignment.value, items, line);
			break;
		case BLOCK_STATEMENT:
			_checkAsksInStatements(statement->statements, items);
			break;
		case DECLARATION_STATEMENT:
			_checkAsksInExpression(statement->declaration->initializer, items, line);
			break;
		case FOR_EACH_STATEMENT:
			_checkAsksInExpression(statement->forLoop.collection, items, line);
			_checkAsksInStatement(statement->forLoop.body, items);
			break;
		case FOR_RANGE_STATEMENT:
			_checkAsksInExpression(statement->forLoop.from, items, line);
			_checkAsksInExpression(statement->forLoop.to, items, line);
			_checkAsksInStatement(statement->forLoop.body, items);
			break;
		case IF_STATEMENT:
			_checkAsksInExpression(statement->ifStatement.condition, items, line);
			_checkAsksInStatement(statement->ifStatement.thenBranch, items);
			_checkAsksInStatement(statement->ifStatement.elseBranch, items);
			break;
		case LOG_STATEMENT:
			for (LogPart * part = statement->log; part != NULL; part = part->next) {
				if (part->type == EXPRESSION_LOG_PART) {
					_checkAsksInExpression(part->expression, items, line);
				}
			}
			break;
		case REPEAT_STATEMENT:
		case WHILE_STATEMENT:
			_checkAsksInExpression(statement->loop.condition, items, line);
			_checkAsksInStatement(statement->loop.body, items);
			break;
		case USES_STATEMENT:
			/* Un "uses" no usa el campo value: solo tiene target y strategy. */
			_checkAsksInExpression(statement->assignment.target, items, line);
			break;
		case DRAW_STATEMENT:
		case GIVE_STATEMENT:
		case MOVE_STATEMENT:
		case PLACE_STATEMENT:
		case PLAY_STATEMENT:
		case SHUFFLE_STATEMENT:
		case TAKE_STATEMENT:
			_checkAsksInExpression(statement->action.subject, items, line);
			_checkAsksInExpression(statement->action.amount, items, line);
			_checkAsksInExpression(statement->action.source, items, line);
			_checkAsksInExpression(statement->action.target, items, line);
			break;
	}
}

static void _checkAsksInCard(Card * card, GameItem * items, const int line) {
	if (card->type == NAMED_CARD) {
		for (CardField * field = card->fields; field != NULL; field = field->next) {
			_checkAsksInExpression(field->value, items, line);
		}
		return;
	}
	_checkAsksInExpressionList(card->values, items, line);
}

/**
 * Caso 8: una strategy resuelve todos los decision declarados, y no define
 * politicas para decisiones que no existen (un "jugra:" mal escrito, por
 * ejemplo). De paso recorre los criterios, que tambien pueden tener un "ask".
 */
static void _checkStrategy(GameItem * strategy, GameItem * items) {
	for (GameItem * item = items; item != NULL; item = item->next) {
		if (item->type == DECISION_ITEM && _findPolicy(strategy->strategy.policies, item->decision.name) == NULL) {
			_error(strategy->line, "la strategy \"%s\" no resuelve la decision \"%s\"",
				strategy->strategy.name, item->decision.name);
		}
	}
	for (Policy * policy = strategy->strategy.policies; policy != NULL; policy = policy->next) {
		if (_findDecision(items, policy->decision) == NULL) {
			_error(strategy->line, "la strategy \"%s\" tiene una politica para \"%s\", que no es una decision declarada",
				strategy->strategy.name, policy->decision);
		}
		for (Criterion * criterion = policy->criteria; criterion != NULL; criterion = criterion->next) {
			_checkAsksInExpression(criterion->expression, items, strategy->line);
		}
		_checkAsksInExpression(policy->where, items, strategy->line);
	}
}

/**
 * Chequea un game: sus strategy (caso 8) y todas las expresiones que puede
 * contener, para encontrar los "ask" (caso 9). Las decisiones se buscan sobre la
 * lista completa de items, asi que da lo mismo si el decision esta declarado
 * antes o despues de quien lo usa.
 */
static void _checkGame(GameDeclaration * game) {
	GameItem * items = game->items;
	for (GameItem * item = items; item != NULL; item = item->next) {
		switch (item->type) {
			case DECK_ITEM:
				if (item->deck->bodyType == CARDS_DECK_BODY) {
					for (Card * card = item->deck->cards; card != NULL; card = card->next) {
						_checkAsksInCard(card, items, item->line);
					}
				}
				break;
			case METRIC_ITEM:
				_checkAsksInExpression(item->metric.value, items, item->line);
				break;
			case PREPARE_ITEM:
			case TURN_ITEM:
				_checkAsksInStatements(item->block->statements, items);
				break;
			case STRATEGY_ITEM:
				_checkStrategy(item, items);
				break;
			case VARIABLE_ITEM:
				_checkAsksInExpression(item->variable->initializer, items, item->line);
				break;
			case WIN_ITEM:
				_checkAsksInExpression(item->condition, items, item->line);
				break;
			case BOARD_ITEM:
			case CARD_TYPE_ITEM:
			case DECISION_ITEM:
			case DIE_ITEM:
			case END_ITEM:
			case PIECE_ITEM:
			case PLAYERS_ITEM:
			case TURN_ORDER_ITEM:
				/* No llevan expresiones. */
				break;
		}
	}
}

/**
 * Caso 10: el simulate nombra un game declarado, y la cantidad de jugadores
 * pertenece al conjunto que ese game admite. Si el game no declara "players" no
 * hay contra que comparar, y la cantidad se acepta.
 */
static void _checkSimulation(Simulation * simulation, TopLevel * topLevels, const int line) {
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

/* PUBLIC FUNCTIONS */

CompilationStatus executeSemanticAnalysis(CompilerState * compilerState) {
	logDebugging(_logger, "Analyzing the AST...");
	Program * program = compilerState->abstractSyntaxtTree;
	_errors = 0;
	for (TopLevel * topLevel = program->items; topLevel != NULL; topLevel = topLevel->next) {
		switch (topLevel->type) {
			case GAME_TOP_LEVEL:
				_checkGame(topLevel->game);
				break;
			case SIMULATION_TOP_LEVEL:
				_checkSimulation(topLevel->simulation, program->items, topLevel->line);
				break;
			case REPORT_TOP_LEVEL:
				/* Los items de un report no llevan expresiones. */
				break;
		}
	}
	logDebugging(_logger, "Semantic errors found: %u.", _errors);
	return _errors == 0 ? SUCCEEDED : FAILED;
}
