#include "BisonActions.h"

/* MODULE INTERNAL STATE */

static CompilerState * _compilerState = NULL;
static Logger * _logger = NULL;

/** Shutdown module's internal state. */
void _shutdownBisonActionsModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: BisonActions...");
		destroyLogger(_logger);
		_logger = NULL;
	}
	_compilerState = NULL;
}

ModuleDestructor initializeBisonActionsModule(CompilerState * compilerState) {
	_compilerState = compilerState;
	_logger = createLogger("BisonActions");
	return _shutdownBisonActionsModule;
}

/* IMPORTED FUNCTIONS */

/* PRIVATE FUNCTIONS */

static void _logSyntacticAnalyzerAction(const char * functionName);

/**
 * Logs a syntactic-analyzer action in DEBUGGING level.
 */
static void _logSyntacticAnalyzerAction(const char * functionName) {
	logDebugging(_logger, "%s", functionName);
}

#define LIST_SEMANTIC_ACTION(Type)                                      \
	Type * Type##ListSemanticAction(Type * list, Type * item) {         \
		_logSyntacticAnalyzerAction(__FUNCTION__);                      \
		Type * last = list;                                             \
		while (last->next != NULL) {                                    \
			last = last->next;                                          \
		}                                                               \
		last->next = item;                                              \
		return list;                                                    \
	}

/* PUBLIC FUNCTIONS */

LIST_SEMANTIC_ACTION(Card)
LIST_SEMANTIC_ACTION(CardField)
LIST_SEMANTIC_ACTION(Criterion)
LIST_SEMANTIC_ACTION(Field)
LIST_SEMANTIC_ACTION(GameItem)
LIST_SEMANTIC_ACTION(Generator)
LIST_SEMANTIC_ACTION(Literal)
LIST_SEMANTIC_ACTION(Policy)
LIST_SEMANTIC_ACTION(ReportItem)
LIST_SEMANTIC_ACTION(Statement)
LIST_SEMANTIC_ACTION(TopLevel)

ExpressionList * ExpressionListSemanticAction(ExpressionList * list, Expression * expression) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	ExpressionList * item = calloc(1, sizeof(ExpressionList));
	item->expression = expression;
	if (list == NULL) {
		return item;
	}
	ExpressionList * last = list;
	while (last->next != NULL) {
		last = last->next;
	}
	last->next = item;
	return list;
}

Program * ProgramSemanticAction(TopLevel * items) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Program * program = calloc(1, sizeof(Program));
	program->items = items;
	_compilerState->abstractSyntaxtTree = program;
	return program;
}

TopLevel * GameTopLevelSemanticAction(GameDeclaration * game, const int line) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	TopLevel * topLevel = calloc(1, sizeof(TopLevel));
	topLevel->game = game;
	topLevel->type = GAME_TOP_LEVEL;
	topLevel->line = line;
	return topLevel;
}

TopLevel * ReportTopLevelSemanticAction(ReportItem * report, const int line) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	TopLevel * topLevel = calloc(1, sizeof(TopLevel));
	topLevel->report = report;
	topLevel->type = REPORT_TOP_LEVEL;
	topLevel->line = line;
	return topLevel;
}

TopLevel * SimulationTopLevelSemanticAction(Simulation * simulation, const int line) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	TopLevel * topLevel = calloc(1, sizeof(TopLevel));
	topLevel->simulation = simulation;
	topLevel->type = SIMULATION_TOP_LEVEL;
	topLevel->line = line;
	return topLevel;
}

Simulation * SimulationSemanticAction(const int games, char * gameName, const int players, const int seed, const bool verbose) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Simulation * simulation = calloc(1, sizeof(Simulation));
	simulation->games = games;
	simulation->gameName = gameName;
	simulation->players = players;
	simulation->hasSeed = 0 <= seed;
	simulation->seed = simulation->hasSeed ? seed : 0;
	simulation->verbose = verbose;
	return simulation;
}

ReportItem * AggregateReportItemSemanticAction(const ReportAggregator aggregator, char * metric) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	ReportItem * reportItem = calloc(1, sizeof(ReportItem));
	reportItem->aggregator = aggregator;
	reportItem->metric = metric;
	reportItem->type = AGGREGATE_REPORT_ITEM;
	return reportItem;
}

ReportItem * WinrateReportItemSemanticAction(const ReportItemType type) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	ReportItem * reportItem = calloc(1, sizeof(ReportItem));
	reportItem->type = type;
	return reportItem;
}

TypeSpec * ArrayTypeSpecSemanticAction(TypeSpec * typeSpec) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	typeSpec->isArray = true;
	return typeSpec;
}

TypeSpec * TypeSpecSemanticAction(const BaseType baseType, char * name, const bool isArray) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	TypeSpec * typeSpec = calloc(1, sizeof(TypeSpec));
	typeSpec->baseType = baseType;
	typeSpec->name = name;
	typeSpec->isArray = isArray;
	return typeSpec;
}

Literal * BooleanLiteralSemanticAction(const bool value) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Literal * literal = calloc(1, sizeof(Literal));
	literal->boolean = value;
	literal->type = BOOLEAN_LITERAL;
	return literal;
}

Literal * IntegerLiteralSemanticAction(const int value) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Literal * literal = calloc(1, sizeof(Literal));
	literal->integer = value;
	literal->type = INTEGER_LITERAL;
	return literal;
}

Literal * StringLiteralSemanticAction(char * value) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Literal * literal = calloc(1, sizeof(Literal));
	literal->string = value;
	literal->type = STRING_LITERAL;
	return literal;
}

ValueSet * RangeValueSetSemanticAction(const int min, const int max) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	ValueSet * valueSet = calloc(1, sizeof(ValueSet));
	valueSet->min = min;
	valueSet->max = max;
	valueSet->type = RANGE_VALUE_SET;
	return valueSet;
}

ValueSet * SetValueSetSemanticAction(Literal * elements) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	ValueSet * valueSet = calloc(1, sizeof(ValueSet));
	valueSet->elements = elements;
	valueSet->type = SET_VALUE_SET;
	return valueSet;
}

GameDeclaration * GameDeclarationSemanticAction(char * name, GameItem * items) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	GameDeclaration * gameDeclaration = calloc(1, sizeof(GameDeclaration));
	gameDeclaration->name = name;
	gameDeclaration->items = items;
	return gameDeclaration;
}

static GameItem * _createGameItem(const GameItemType type, const int line) {
	GameItem * gameItem = calloc(1, sizeof(GameItem));
	gameItem->type = type;
	gameItem->line = line;
	return gameItem;
}

GameItem * BlockGameItemSemanticAction(const GameItemType type, Statement * block, const int line) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	GameItem * gameItem = _createGameItem(type, line);
	gameItem->block = block;
	return gameItem;
}

GameItem * BoardGameItemSemanticAction(const int cells, const int line) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	GameItem * gameItem = _createGameItem(BOARD_ITEM, line);
	gameItem->cells = cells;
	return gameItem;
}

GameItem * CardTypeGameItemSemanticAction(char * name, Field * fields, const int line) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	GameItem * gameItem = _createGameItem(CARD_TYPE_ITEM, line);
	gameItem->cardType.name = name;
	gameItem->cardType.fields = fields;
	return gameItem;
}

GameItem * DecisionGameItemSemanticAction(char * name, TypeSpec * type, const int line) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	GameItem * gameItem = _createGameItem(DECISION_ITEM, line);
	gameItem->decision.name = name;
	gameItem->decision.type = type;
	return gameItem;
}

GameItem * DeckGameItemSemanticAction(char * name, char * cardType, const bool perPlayer, DeckDeclaration * body, const int line) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	DeckDeclaration * deck = body;
	if (deck == NULL) {
		deck = calloc(1, sizeof(DeckDeclaration));
		deck->bodyType = EMPTY_DECK_BODY;
	}
	deck->name = name;
	deck->cardType = cardType;
	deck->perPlayer = perPlayer;
	GameItem * gameItem = _createGameItem(DECK_ITEM, line);
	gameItem->deck = deck;
	return gameItem;
}

GameItem * DieGameItemSemanticAction(char * name, ValueSet * faces, const int line) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	GameItem * gameItem = _createGameItem(DIE_ITEM, line);
	gameItem->die.name = name;
	gameItem->die.faces = faces;
	return gameItem;
}

GameItem * EndGameItemSemanticAction(const int turns, const int line) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	GameItem * gameItem = _createGameItem(END_ITEM, line);
	gameItem->turns = turns;
	return gameItem;
}

GameItem * MetricGameItemSemanticAction(char * name, Expression * value, const int line) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	GameItem * gameItem = _createGameItem(METRIC_ITEM, line);
	gameItem->metric.name = name;
	gameItem->metric.value = value;
	return gameItem;
}

GameItem * PieceGameItemSemanticAction(char * name, const bool perPlayer, const int line) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	GameItem * gameItem = _createGameItem(PIECE_ITEM, line);
	gameItem->piece.name = name;
	gameItem->piece.perPlayer = perPlayer;
	return gameItem;
}

GameItem * PlayersGameItemSemanticAction(ValueSet * players, const int line) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	GameItem * gameItem = _createGameItem(PLAYERS_ITEM, line);
	gameItem->players = players;
	return gameItem;
}

GameItem * StrategyGameItemSemanticAction(char * name, Policy * policies, const int line) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	GameItem * gameItem = _createGameItem(STRATEGY_ITEM, line);
	gameItem->strategy.name = name;
	gameItem->strategy.policies = policies;
	return gameItem;
}

GameItem * TurnOrderGameItemSemanticAction(const bool clockwise, const int line) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	GameItem * gameItem = _createGameItem(TURN_ORDER_ITEM, line);
	gameItem->clockwise = clockwise;
	return gameItem;
}

GameItem * VariableGameItemSemanticAction(VariableDeclaration * variable, const int line) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	GameItem * gameItem = _createGameItem(VARIABLE_ITEM, line);
	gameItem->variable = variable;
	return gameItem;
}

GameItem * WinGameItemSemanticAction(Expression * condition, const int line) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	GameItem * gameItem = _createGameItem(WIN_ITEM, line);
	gameItem->condition = condition;
	return gameItem;
}

Field * FieldSemanticAction(TypeSpec * type, char * name) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Field * field = calloc(1, sizeof(Field));
	field->type = type;
	field->name = name;
	return field;
}

DeckDeclaration * CardsDeckBodySemanticAction(Card * cards) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	DeckDeclaration * deck = calloc(1, sizeof(DeckDeclaration));
	deck->cards = cards;
	deck->bodyType = CARDS_DECK_BODY;
	return deck;
}

DeckDeclaration * GeneratorsDeckBodySemanticAction(Generator * generators) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	DeckDeclaration * deck = calloc(1, sizeof(DeckDeclaration));
	deck->generators = generators;
	deck->bodyType = GENERATORS_DECK_BODY;
	return deck;
}

Card * NamedCardSemanticAction(CardField * fields) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Card * card = calloc(1, sizeof(Card));
	card->fields = fields;
	card->type = NAMED_CARD;
	return card;
}

Card * PositionalCardSemanticAction(ExpressionList * values) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Card * card = calloc(1, sizeof(Card));
	card->values = values;
	card->type = POSITIONAL_CARD;
	return card;
}

CardField * CardFieldSemanticAction(char * name, Expression * value) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	CardField * cardField = calloc(1, sizeof(CardField));
	cardField->name = name;
	cardField->value = value;
	return cardField;
}

Generator * GeneratorSemanticAction(char * field, ValueSet * values) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Generator * generator = calloc(1, sizeof(Generator));
	generator->field = field;
	generator->values = values;
	return generator;
}

Policy * PolicySemanticAction(char * decision, Criterion * criteria, Expression * where) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Policy * policy = calloc(1, sizeof(Policy));
	policy->decision = decision;
	policy->criteria = criteria;
	policy->where = where;
	return policy;
}

Criterion * CriterionSemanticAction(const CriterionType type, Expression * expression) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Criterion * criterion = calloc(1, sizeof(Criterion));
	criterion->expression = expression;
	criterion->type = type;
	return criterion;
}

VariableDeclaration * VariableDeclarationSemanticAction(TypeSpec * type, char * name, Expression * initializer) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	VariableDeclaration * variableDeclaration = calloc(1, sizeof(VariableDeclaration));
	variableDeclaration->type = type;
	variableDeclaration->name = name;
	variableDeclaration->initializer = initializer;
	return variableDeclaration;
}

static Statement * _createStatement(const StatementType type, const int line) {
	Statement * statement = calloc(1, sizeof(Statement));
	statement->type = type;
	statement->line = line;
	return statement;
}

Statement * ActionStatementSemanticAction(const StatementType type, Expression * subject, Expression * amount, Expression * source, Expression * target, const int line) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Statement * statement = _createStatement(type, line);
	statement->action.subject = subject;
	statement->action.amount = amount;
	statement->action.source = source;
	statement->action.target = target;
	return statement;
}

Statement * AssignmentStatementSemanticAction(Expression * value) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Statement * statement = _createStatement(ASSIGNMENT_STATEMENT, 0);
	statement->assignment.value = value;
	return statement;
}

Statement * BlockStatementSemanticAction(Statement * statements, const int line) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Statement * statement = _createStatement(BLOCK_STATEMENT, line);
	statement->statements = statements;
	return statement;
}

Statement * DeclarationStatementSemanticAction(VariableDeclaration * declaration, const int line) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Statement * statement = _createStatement(DECLARATION_STATEMENT, line);
	statement->declaration = declaration;
	return statement;
}

Statement * ForEachStatementSemanticAction(char * variable, Expression * collection, Statement * body, const int line) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Statement * statement = _createStatement(FOR_EACH_STATEMENT, line);
	statement->forLoop.variable = variable;
	statement->forLoop.collection = collection;
	statement->forLoop.body = body;
	return statement;
}

Statement * ForRangeStatementSemanticAction(char * variable, Expression * from, Expression * to, Statement * body, const int line) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Statement * statement = _createStatement(FOR_RANGE_STATEMENT, line);
	statement->forLoop.variable = variable;
	statement->forLoop.from = from;
	statement->forLoop.to = to;
	statement->forLoop.body = body;
	return statement;
}

Statement * IfStatementSemanticAction(Expression * condition, Statement * thenBranch, Statement * elseBranch, const int line) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Statement * statement = _createStatement(IF_STATEMENT, line);
	statement->ifStatement.condition = condition;
	statement->ifStatement.thenBranch = thenBranch;
	statement->ifStatement.elseBranch = elseBranch;
	return statement;
}

Statement * LogStatementSemanticAction(LogPart * parts, const int line) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Statement * statement = _createStatement(LOG_STATEMENT, line);
	statement->log = parts;
	return statement;
}

Statement * LoopStatementSemanticAction(const StatementType type, Expression * condition, Statement * body, const int line) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Statement * statement = _createStatement(type, line);
	statement->loop.condition = condition;
	statement->loop.body = body;
	return statement;
}

Statement * LvalueStatementSemanticAction(Expression * base, Expression * tail, Statement * end, const int line) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expression * target = base;
	if (tail != NULL) {
		Expression * innermost = tail;
		while (innermost->postfix.object != NULL) {
			innermost = innermost->postfix.object;
		}
		innermost->postfix.object = base;
		target = tail;
	}
	end->assignment.target = target;
	end->line = line;
	return end;
}

Statement * UsesStatementSemanticAction(char * strategy) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Statement * statement = _createStatement(USES_STATEMENT, 0);
	statement->assignment.strategy = strategy;
	return statement;
}

LogPart * InterpolationLogPartSemanticAction(char * text, Expression * expression, LogPart * rest) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	LogPart * expressionPart = calloc(1, sizeof(LogPart));
	expressionPart->expression = expression;
	expressionPart->type = EXPRESSION_LOG_PART;
	expressionPart->next = rest;
	LogPart * textPart = TextLogPartSemanticAction(text);
	textPart->next = expressionPart;
	return textPart;
}

LogPart * TextLogPartSemanticAction(char * text) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	LogPart * logPart = calloc(1, sizeof(LogPart));
	logPart->text = text;
	logPart->type = TEXT_LOG_PART;
	return logPart;
}

static Expression * _createExpression(const ExpressionType type) {
	Expression * expression = calloc(1, sizeof(Expression));
	expression->type = type;
	return expression;
}

Expression * AggregationExpressionSemanticAction(const AggregationType type, char * variable, Expression * collection, Expression * where, Expression * by) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expression * expression = _createExpression(AGGREGATION_EXPRESSION);
	expression->aggregation.type = type;
	expression->aggregation.variable = variable;
	expression->aggregation.collection = collection;
	expression->aggregation.where = where;
	expression->aggregation.by = by;
	return expression;
}

Expression * AskExpressionSemanticAction(Expression * player, char * decision, ExpressionList * arguments) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expression * expression = _createExpression(ASK_EXPRESSION);
	expression->ask.player = player;
	expression->ask.decision = decision;
	expression->ask.arguments = arguments;
	return expression;
}

Expression * BinaryExpressionSemanticAction(Expression * left, Expression * right, const ExpressionType type) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expression * expression = _createExpression(type);
	expression->left = left;
	expression->right = right;
	return expression;
}

Expression * BooleanExpressionSemanticAction(const bool value) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expression * expression = _createExpression(BOOLEAN_EXPRESSION);
	expression->boolean = value;
	return expression;
}

Expression * CallExpressionSemanticAction(Expression * object, ExpressionList * arguments) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expression * expression = _createExpression(CALL_EXPRESSION);
	expression->postfix.object = object;
	expression->postfix.arguments = arguments;
	return expression;
}

Expression * IdentifierExpressionSemanticAction(char * identifier) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expression * expression = _createExpression(IDENTIFIER_EXPRESSION);
	expression->identifier = identifier;
	return expression;
}

Expression * IndexExpressionSemanticAction(Expression * object, Expression * index) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expression * expression = _createExpression(INDEX_EXPRESSION);
	expression->postfix.object = object;
	expression->postfix.index = index;
	return expression;
}

Expression * IntegerExpressionSemanticAction(const int value) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expression * expression = _createExpression(INTEGER_EXPRESSION);
	expression->integer = value;
	return expression;
}

Expression * MemberExpressionSemanticAction(Expression * object, char * member) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expression * expression = _createExpression(MEMBER_EXPRESSION);
	expression->postfix.object = object;
	expression->postfix.member = member;
	return expression;
}

Expression * PredefinedExpressionSemanticAction(const ExpressionType type) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	return _createExpression(type);
}

Expression * RollExpressionSemanticAction(char * die) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expression * expression = _createExpression(ROLL_EXPRESSION);
	expression->die = die;
	return expression;
}

Expression * StringExpressionSemanticAction(char * value) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expression * expression = _createExpression(STRING_EXPRESSION);
	expression->string = value;
	return expression;
}

Expression * UnaryExpressionSemanticAction(Expression * operand, const ExpressionType type) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expression * expression = _createExpression(type);
	expression->operand = operand;
	return expression;
}

char * MemberNameSemanticAction(const char * keyword) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	return strdup(keyword);
}
