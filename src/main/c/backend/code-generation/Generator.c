#include "Generator.h"

/* MODULE INTERNAL STATE */

static const unsigned int _indentationSize = 2;
static Logger * _logger = NULL;

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

/** PRIVATE FUNCTIONS */

static void _begin(const unsigned int level, const char * label);
static void _line(const unsigned int level, const char * label, const char * const format, ...);
static const char * _baseTypeName(TypeSpec * typeSpec);
static const char * _binaryOperator(const ExpressionType type);
static void _printCard(const unsigned int level, Card * card);
static void _printExpression(const unsigned int level, const char * label, Expression * expression);
static void _printExpressionList(const unsigned int level, const char * label, ExpressionList * expressionList);
static void _printGameItem(const unsigned int level, GameItem * gameItem);
static void _printLiteral(Literal * literal);
static void _printPolicy(const unsigned int level, Policy * policy);
static void _printReportItem(const unsigned int level, ReportItem * reportItem);
static void _printStatement(const unsigned int level, const char * label, Statement * statement);
static void _printStatements(const unsigned int level, Statement * statement);
static void _printTopLevel(const unsigned int level, TopLevel * topLevel);
static void _printValueSet(ValueSet * valueSet);

static void _begin(const unsigned int level, const char * label) {
	printf("%*s", level * _indentationSize, "");
	if (label != NULL) {
		printf("%s: ", label);
	}
}

static void _line(const unsigned int level, const char * label, const char * const format, ...) {
	_begin(level, label);
	va_list arguments;
	va_start(arguments, format);
	vfprintf(stdout, format, arguments);
	va_end(arguments);
	printf("\n");
	fflush(stdout);
}

static const char * _baseTypeName(TypeSpec * typeSpec) {
	switch (typeSpec->baseType) {
		case BOOLEAN_TYPE: return "boolean";
		case DECK_TYPE: return "deck";
		case DIE_TYPE: return "die";
		case INTEGER_TYPE: return "integer";
		case NAMED_TYPE: return typeSpec->name;
		case PIECE_TYPE: return "piece";
		case PLAYER_TYPE: return "player";
		case STRATEGY_TYPE: return "strategy";
		case STRING_TYPE: return "string";
		default: return "?";
	}
}

static const char * _binaryOperator(const ExpressionType type) {
	switch (type) {
		case ADDITION_EXPRESSION: return "+";
		case AND_EXPRESSION: return "and";
		case DIVISION_EXPRESSION: return "/";
		case EQUAL_EXPRESSION: return "==";
		case GREATER_EQUAL_EXPRESSION: return ">=";
		case GREATER_EXPRESSION: return ">";
		case LESS_EQUAL_EXPRESSION: return "<=";
		case LESS_EXPRESSION: return "<";
		case MODULE_EXPRESSION: return "%";
		case MULTIPLICATION_EXPRESSION: return "*";
		case NOT_EQUAL_EXPRESSION: return "!=";
		case OR_EXPRESSION: return "or";
		case SUBTRACTION_EXPRESSION: return "-";
		default: return "?";
	}
}

static void _printLiteral(Literal * literal) {
	switch (literal->type) {
		case BOOLEAN_LITERAL:
			printf("%s", literal->boolean ? "true" : "false");
			break;
		case INTEGER_LITERAL:
			printf("%d", literal->integer);
			break;
		case STRING_LITERAL:
			printf("\"%s\"", literal->string);
			break;
	}
}

static void _printValueSet(ValueSet * valueSet) {
	if (valueSet->type == RANGE_VALUE_SET) {
		printf("%d to %d", valueSet->min, valueSet->max);
		return;
	}
	printf("{");
	for (Literal * literal = valueSet->elements; literal != NULL; literal = literal->next) {
		_printLiteral(literal);
		if (literal->next != NULL) {
			printf(", ");
		}
	}
	printf("}");
}

static void _printExpressionList(const unsigned int level, const char * label, ExpressionList * expressionList) {
	for (ExpressionList * item = expressionList; item != NULL; item = item->next) {
		_printExpression(level, label, item->expression);
	}
}

static void _printExpression(const unsigned int level, const char * label, Expression * expression) {
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
			_line(level, label, "Binary %s", _binaryOperator(expression->type));
			_printExpression(level + 1, "left", expression->left);
			_printExpression(level + 1, "right", expression->right);
			break;
		case NEGATION_EXPRESSION:
			_line(level, label, "Unary -");
			_printExpression(level + 1, "operand", expression->operand);
			break;
		case NOT_EXPRESSION:
			_line(level, label, "Unary not");
			_printExpression(level + 1, "operand", expression->operand);
			break;
		case CALL_EXPRESSION:
			_line(level, label, "Call");
			_printExpression(level + 1, "function", expression->postfix.object);
			_printExpressionList(level + 1, "argument", expression->postfix.arguments);
			break;
		case INDEX_EXPRESSION:
			_line(level, label, "Index");
			_printExpression(level + 1, "object", expression->postfix.object);
			_printExpression(level + 1, "index", expression->postfix.index);
			break;
		case MEMBER_EXPRESSION:
			_line(level, label, "Member .%s", expression->postfix.member);
			_printExpression(level + 1, "object", expression->postfix.object);
			break;
		case ASK_EXPRESSION:
			_line(level, label, "Ask %s", expression->ask.decision);
			_printExpression(level + 1, "player", expression->ask.player);
			_printExpressionList(level + 1, "argument", expression->ask.arguments);
			break;
		case AGGREGATION_EXPRESSION: {
			const char * names[] = {
				[COUNT_AGGREGATION] = "count",
				[MAX_AGGREGATION] = "max",
				[MIN_AGGREGATION] = "min",
				[SELECT_AGGREGATION] = "select",
				[SUM_AGGREGATION] = "sum"
			};
			_line(level, label, "Aggregation %s (%s in ...)", names[expression->aggregation.type], expression->aggregation.variable);
			_printExpression(level + 1, "collection", expression->aggregation.collection);
			_printExpression(level + 1, "where", expression->aggregation.where);
			_printExpression(level + 1, "by", expression->aggregation.by);
			break;
		}
		case BOARD_EXPRESSION:
			_line(level, label, "board");
			break;
		case BOOLEAN_EXPRESSION:
			_line(level, label, "Boolean %s", expression->boolean ? "true" : "false");
			break;
		case CURRENT_EXPRESSION:
			_line(level, label, "current");
			break;
		case IDENTIFIER_EXPRESSION:
			_line(level, label, "Identifier %s", expression->identifier);
			break;
		case INTEGER_EXPRESSION:
			_line(level, label, "Integer %d", expression->integer);
			break;
		case NONE_EXPRESSION:
			_line(level, label, "none");
			break;
		case OPTION_EXPRESSION:
			_line(level, label, "option");
			break;
		case PLAYERS_EXPRESSION:
			_line(level, label, "players");
			break;
		case ROLL_EXPRESSION:
			_line(level, label, "Roll %s", expression->die);
			break;
		case STRING_EXPRESSION:
			_line(level, label, "String \"%s\"", expression->string);
			break;
		case TURNS_EXPRESSION:
			_line(level, label, "turns");
			break;
	}
}

static void _printStatements(const unsigned int level, Statement * statement) {
	for (; statement != NULL; statement = statement->next) {
		_printStatement(level, NULL, statement);
	}
}

static void _printStatement(const unsigned int level, const char * label, Statement * statement) {
	if (statement == NULL) {
		return;
	}
	const unsigned int line = statement->line;
	switch (statement->type) {
		case ASSIGNMENT_STATEMENT:
			_line(level, label, "Assignment (line %u)", line);
			_printExpression(level + 1, "target", statement->assignment.target);
			_printExpression(level + 1, "value", statement->assignment.value);
			break;
		case BLOCK_STATEMENT:
			_line(level, label, "Block (line %u)", line);
			_printStatements(level + 1, statement->statements);
			break;
		case DECLARATION_STATEMENT: {
			VariableDeclaration * declaration = statement->declaration;
			_line(level, label, "Declaration %s%s %s (line %u)", _baseTypeName(declaration->type), declaration->type->isArray ? "[]" : "", declaration->name, line);
			_printExpression(level + 1, "initializer", declaration->initializer);
			break;
		}
		case FOR_EACH_STATEMENT:
			_line(level, label, "For %s in (line %u)", statement->forLoop.variable, line);
			_printExpression(level + 1, "collection", statement->forLoop.collection);
			_printStatement(level + 1, "body", statement->forLoop.body);
			break;
		case FOR_RANGE_STATEMENT:
			_line(level, label, "For %s in ... to ... (line %u)", statement->forLoop.variable, line);
			_printExpression(level + 1, "from", statement->forLoop.from);
			_printExpression(level + 1, "to", statement->forLoop.to);
			_printStatement(level + 1, "body", statement->forLoop.body);
			break;
		case IF_STATEMENT:
			_line(level, label, "If (line %u)", line);
			_printExpression(level + 1, "condition", statement->ifStatement.condition);
			_printStatement(level + 1, "then", statement->ifStatement.thenBranch);
			_printStatement(level + 1, "else", statement->ifStatement.elseBranch);
			break;
		case LOG_STATEMENT:
			_line(level, label, "Log (line %u)", line);
			for (LogPart * part = statement->log; part != NULL; part = part->next) {
				if (part->type == TEXT_LOG_PART) {
					if (part->text[0] != '\0') {
						_line(level + 1, "text", "\"%s\"", part->text);
					}
				}
				else {
					_printExpression(level + 1, "interpolation", part->expression);
				}
			}
			break;
		case REPEAT_STATEMENT:
			_line(level, label, "Repeat (line %u)", line);
			_printExpression(level + 1, "times", statement->loop.condition);
			_printStatement(level + 1, "body", statement->loop.body);
			break;
		case USES_STATEMENT:
			_line(level, label, "Uses %s (line %u)", statement->assignment.strategy, line);
			_printExpression(level + 1, "target", statement->assignment.target);
			break;
		case WHILE_STATEMENT:
			_line(level, label, "While (line %u)", line);
			_printExpression(level + 1, "condition", statement->loop.condition);
			_printStatement(level + 1, "body", statement->loop.body);
			break;
		case DRAW_STATEMENT:
		case GIVE_STATEMENT:
		case MOVE_STATEMENT:
		case PLACE_STATEMENT:
		case PLAY_STATEMENT:
		case SHUFFLE_STATEMENT:
		case TAKE_STATEMENT: {
			const char * names[] = {
				[DRAW_STATEMENT] = "Draw",
				[GIVE_STATEMENT] = "Give",
				[MOVE_STATEMENT] = "Move",
				[PLACE_STATEMENT] = "Place",
				[PLAY_STATEMENT] = "Play",
				[SHUFFLE_STATEMENT] = "Shuffle",
				[TAKE_STATEMENT] = "Take"
			};
			_line(level, label, "%s (line %u)", names[statement->type], line);
			_printExpression(level + 1, "subject", statement->action.subject);
			_printExpression(level + 1, "amount", statement->action.amount);
			_printExpression(level + 1, "from", statement->action.source);
			_printExpression(level + 1, statement->type == PLACE_STATEMENT ? "on" : "to", statement->action.target);
			break;
		}
	}
}

static void _printCard(const unsigned int level, Card * card) {
	_line(level, NULL, "Card");
	if (card->type == NAMED_CARD) {
		for (CardField * field = card->fields; field != NULL; field = field->next) {
			_printExpression(level + 1, field->name, field->value);
		}
	}
	else {
		_printExpressionList(level + 1, "value", card->values);
	}
}

static void _printPolicy(const unsigned int level, Policy * policy) {
	_line(level, NULL, "Policy %s", policy->decision);
	for (Criterion * criterion = policy->criteria; criterion != NULL; criterion = criterion->next) {
		switch (criterion->type) {
			case FIRST_CRITERION:
				_line(level + 1, "prefer", "first");
				break;
			case INPUT_CRITERION:
				_line(level + 1, "prefer", "input");
				break;
			case MAX_CRITERION:
				_printExpression(level + 1, "prefer max", criterion->expression);
				break;
			case MIN_CRITERION:
				_printExpression(level + 1, "prefer min", criterion->expression);
				break;
			case RANDOM_CRITERION:
				_line(level + 1, "prefer", "random");
				break;
		}
	}
	_printExpression(level + 1, "where", policy->where);
}

static void _printGameItem(const unsigned int level, GameItem * gameItem) {
	const unsigned int line = gameItem->line;
	switch (gameItem->type) {
		case BOARD_ITEM:
			_line(level, NULL, "Board cells %d (line %u)", gameItem->cells, line);
			break;
		case CARD_TYPE_ITEM:
			_line(level, NULL, "CardType %s (line %u)", gameItem->cardType.name, line);
			for (Field * field = gameItem->cardType.fields; field != NULL; field = field->next) {
				_line(level + 1, "field", "%s%s %s", _baseTypeName(field->type), field->type->isArray ? "[]" : "", field->name);
			}
			break;
		case DECISION_ITEM: {
			TypeSpec * type = gameItem->decision.type;
			_line(level, NULL, "Decision %s: %s%s (line %u)", gameItem->decision.name, _baseTypeName(type), type->isArray ? "[]" : "", line);
			break;
		}
		case DECK_ITEM: {
			DeckDeclaration * deck = gameItem->deck;
			_line(level, NULL, "Deck %s of %s%s (line %u)", deck->name, deck->cardType, deck->perPlayer ? " per player" : "", line);
			if (deck->bodyType == CARDS_DECK_BODY) {
				for (Card * card = deck->cards; card != NULL; card = card->next) {
					_printCard(level + 1, card);
				}
			}
			else if (deck->bodyType == GENERATORS_DECK_BODY) {
				for (Generator * generator = deck->generators; generator != NULL; generator = generator->next) {
					_begin(level + 1, "generator");
					printf("%s: ", generator->field);
					_printValueSet(generator->values);
					printf("\n");
				}
			}
			break;
		}
		case DIE_ITEM:
			_begin(level, NULL);
			printf("Die %s faces ", gameItem->die.name);
			_printValueSet(gameItem->die.faces);
			printf(" (line %u)\n", line);
			break;
		case END_ITEM:
			_line(level, NULL, "End after %d turns (line %u)", gameItem->turns, line);
			break;
		case METRIC_ITEM:
			_line(level, NULL, "Metric %s (line %u)", gameItem->metric.name, line);
			_printExpression(level + 1, "value", gameItem->metric.value);
			break;
		case PIECE_ITEM:
			_line(level, NULL, "Piece %s%s (line %u)", gameItem->piece.name, gameItem->piece.perPlayer ? " per player" : "", line);
			break;
		case PLAYERS_ITEM:
			_begin(level, NULL);
			printf("Players ");
			_printValueSet(gameItem->players);
			printf(" (line %u)\n", line);
			break;
		case PREPARE_ITEM:
			_line(level, NULL, "Prepare (line %u)", line);
			_printStatements(level + 1, gameItem->block->statements);
			break;
		case STRATEGY_ITEM:
			_line(level, NULL, "Strategy %s (line %u)", gameItem->strategy.name, line);
			for (Policy * policy = gameItem->strategy.policies; policy != NULL; policy = policy->next) {
				_printPolicy(level + 1, policy);
			}
			break;
		case TURN_ITEM:
			_line(level, NULL, "Turn (line %u)", line);
			_printStatements(level + 1, gameItem->block->statements);
			break;
		case TURN_ORDER_ITEM:
			_line(level, NULL, "TurnOrder %s (line %u)", gameItem->clockwise ? "clockwise" : "counterclockwise", line);
			break;
		case VARIABLE_ITEM: {
			VariableDeclaration * variable = gameItem->variable;
			_line(level, NULL, "Variable %s%s %s (line %u)", _baseTypeName(variable->type), variable->type->isArray ? "[]" : "", variable->name, line);
			_printExpression(level + 1, "initializer", variable->initializer);
			break;
		}
		case WIN_ITEM:
			_line(level, NULL, "WinWhen (line %u)", line);
			_printExpression(level + 1, "condition", gameItem->condition);
			break;
	}
}

static void _printReportItem(const unsigned int level, ReportItem * reportItem) {
	switch (reportItem->type) {
		case AGGREGATE_REPORT_ITEM: {
			const char * names[] = {
				[AVG_REPORT_AGGREGATOR] = "avg",
				[MAX_REPORT_AGGREGATOR] = "max",
				[MIN_REPORT_AGGREGATOR] = "min"
			};
			_line(level, NULL, "%s %s", names[reportItem->aggregator], reportItem->metric == NULL ? "turns" : reportItem->metric);
			break;
		}
		case WINRATE_BY_PLAYER_REPORT_ITEM:
			_line(level, NULL, "winrate by player");
			break;
		case WINRATE_BY_STRATEGY_REPORT_ITEM:
			_line(level, NULL, "winrate by strategy");
			break;
		case WINRATE_REPORT_ITEM:
			_line(level, NULL, "winrate");
			break;
	}
}

static void _printTopLevel(const unsigned int level, TopLevel * topLevel) {
	switch (topLevel->type) {
		case GAME_TOP_LEVEL:
			_line(level, NULL, "Game \"%s\" (line %d)", topLevel->game->name, topLevel->line);
			for (GameItem * item = topLevel->game->items; item != NULL; item = item->next) {
				_printGameItem(level + 1, item);
			}
			break;
		case REPORT_TOP_LEVEL:
			_line(level, NULL, "Report (line %d)", topLevel->line);
			for (ReportItem * item = topLevel->report; item != NULL; item = item->next) {
				_printReportItem(level + 1, item);
			}
			break;
		case SIMULATION_TOP_LEVEL: {
			Simulation * simulation = topLevel->simulation;
			_begin(level, NULL);
			printf("Simulate %d games of \"%s\" with %d players", simulation->games, simulation->gameName, simulation->players);
			if (simulation->hasSeed) {
				printf(" seed %d", simulation->seed);
			}
			printf("%s (line %d)\n", simulation->verbose ? " verbose" : "", topLevel->line);
			break;
		}
	}
}

/** PUBLIC FUNCTIONS */

void executeGenerator(CompilerState * compilerState) {
	logDebugging(_logger, "Generating final output...");
	Program * program = compilerState->abstractSyntaxtTree;
	_line(0, NULL, "Program");
	for (TopLevel * item = program->items; item != NULL; item = item->next) {
		_printTopLevel(1, item);
	}
	fflush(stdout);
}
