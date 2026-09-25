#include "AbstractSyntaxTree.h"

/* MODULE INTERNAL STATE */

static Logger * _logger = NULL;

/** Shutdown module's internal state. */
void _shutdownAbstractSyntaxTreeModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: AbstractSyntaxTree...");
		destroyLogger(_logger);
		_logger = NULL;
	}
}

ModuleDestructor initializeAbstractSyntaxTreeModule() {
	_logger = createLogger("AbstractSyntaxTree");
	return _shutdownAbstractSyntaxTreeModule;
}

/* PUBLIC FUNCTIONS */

void destroyCard(Card * card) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	while (card != NULL) {
		Card * next = card->next;
		switch (card->type) {
			case NAMED_CARD:
				destroyCardField(card->fields);
				break;
			case POSITIONAL_CARD:
				destroyExpressionList(card->values);
				break;
		}
		free(card);
		card = next;
	}
}

void destroyCardField(CardField * cardField) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	while (cardField != NULL) {
		CardField * next = cardField->next;
		free(cardField->name);
		destroyExpression(cardField->value);
		free(cardField);
		cardField = next;
	}
}

void destroyCriterion(Criterion * criterion) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	while (criterion != NULL) {
		Criterion * next = criterion->next;
		destroyExpression(criterion->expression);
		free(criterion);
		criterion = next;
	}
}

void destroyDeckDeclaration(DeckDeclaration * deckDeclaration) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (deckDeclaration != NULL) {
		free(deckDeclaration->name);
		free(deckDeclaration->cardType);
		switch (deckDeclaration->bodyType) {
			case CARDS_DECK_BODY:
				destroyCard(deckDeclaration->cards);
				break;
			case EMPTY_DECK_BODY:
				break;
			case GENERATORS_DECK_BODY:
				destroyGenerator(deckDeclaration->generators);
				break;
		}
		free(deckDeclaration);
	}
}

void destroyExpression(Expression * expression) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (expression != NULL) {
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
				destroyExpression(expression->left);
				destroyExpression(expression->right);
				break;
			case NEGATION_EXPRESSION:
			case NOT_EXPRESSION:
				destroyExpression(expression->operand);
				break;
			case CALL_EXPRESSION:
				destroyExpression(expression->postfix.object);
				destroyExpressionList(expression->postfix.arguments);
				break;
			case INDEX_EXPRESSION:
				destroyExpression(expression->postfix.object);
				destroyExpression(expression->postfix.index);
				break;
			case MEMBER_EXPRESSION:
				destroyExpression(expression->postfix.object);
				free(expression->postfix.member);
				break;
			case ASK_EXPRESSION:
				destroyExpression(expression->ask.player);
				free(expression->ask.decision);
				destroyExpressionList(expression->ask.arguments);
				break;
			case AGGREGATION_EXPRESSION:
				free(expression->aggregation.variable);
				destroyExpression(expression->aggregation.collection);
				destroyExpression(expression->aggregation.where);
				destroyExpression(expression->aggregation.by);
				break;
			case IDENTIFIER_EXPRESSION:
				free(expression->identifier);
				break;
			case ROLL_EXPRESSION:
				free(expression->die);
				break;
			case STRING_EXPRESSION:
				free(expression->string);
				break;
			case BOARD_EXPRESSION:
			case BOOLEAN_EXPRESSION:
			case CURRENT_EXPRESSION:
			case INTEGER_EXPRESSION:
			case NONE_EXPRESSION:
			case OPTION_EXPRESSION:
			case PLAYERS_EXPRESSION:
			case TURNS_EXPRESSION:
				break;
		}
		free(expression);
	}
}

void destroyExpressionList(ExpressionList * expressionList) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	while (expressionList != NULL) {
		ExpressionList * next = expressionList->next;
		destroyExpression(expressionList->expression);
		free(expressionList);
		expressionList = next;
	}
}

void destroyField(Field * field) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	while (field != NULL) {
		Field * next = field->next;
		destroyTypeSpec(field->type);
		free(field->name);
		free(field);
		field = next;
	}
}

void destroyGameDeclaration(GameDeclaration * gameDeclaration) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (gameDeclaration != NULL) {
		free(gameDeclaration->name);
		destroyGameItem(gameDeclaration->items);
		free(gameDeclaration);
	}
}

void destroyGameItem(GameItem * gameItem) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	while (gameItem != NULL) {
		GameItem * next = gameItem->next;
		switch (gameItem->type) {
			case BOARD_ITEM:
			case END_ITEM:
			case TURN_ORDER_ITEM:
				break;
			case CARD_TYPE_ITEM:
				free(gameItem->cardType.name);
				destroyField(gameItem->cardType.fields);
				break;
			case DECISION_ITEM:
				free(gameItem->decision.name);
				destroyTypeSpec(gameItem->decision.type);
				break;
			case DECK_ITEM:
				destroyDeckDeclaration(gameItem->deck);
				break;
			case DIE_ITEM:
				free(gameItem->die.name);
				destroyValueSet(gameItem->die.faces);
				break;
			case METRIC_ITEM:
				free(gameItem->metric.name);
				destroyExpression(gameItem->metric.value);
				break;
			case PIECE_ITEM:
				free(gameItem->piece.name);
				break;
			case PLAYERS_ITEM:
				destroyValueSet(gameItem->players);
				break;
			case PREPARE_ITEM:
			case TURN_ITEM:
				destroyStatement(gameItem->block);
				break;
			case STRATEGY_ITEM:
				free(gameItem->strategy.name);
				destroyPolicy(gameItem->strategy.policies);
				break;
			case VARIABLE_ITEM:
				destroyVariableDeclaration(gameItem->variable);
				break;
			case WIN_ITEM:
				destroyExpression(gameItem->condition);
				break;
		}
		free(gameItem);
		gameItem = next;
	}
}

void destroyGenerator(Generator * generator) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	while (generator != NULL) {
		Generator * next = generator->next;
		free(generator->field);
		destroyValueSet(generator->values);
		free(generator);
		generator = next;
	}
}

void destroyLiteral(Literal * literal) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	while (literal != NULL) {
		Literal * next = literal->next;
		if (literal->type == STRING_LITERAL) {
			free(literal->string);
		}
		free(literal);
		literal = next;
	}
}

void destroyLogPart(LogPart * logPart) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	while (logPart != NULL) {
		LogPart * next = logPart->next;
		switch (logPart->type) {
			case EXPRESSION_LOG_PART:
				destroyExpression(logPart->expression);
				break;
			case TEXT_LOG_PART:
				free(logPart->text);
				break;
		}
		free(logPart);
		logPart = next;
	}
}

void destroyPolicy(Policy * policy) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	while (policy != NULL) {
		Policy * next = policy->next;
		free(policy->decision);
		destroyCriterion(policy->criteria);
		destroyExpression(policy->where);
		free(policy);
		policy = next;
	}
}

void destroyProgram(Program * program) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (program != NULL) {
		destroyTopLevel(program->items);
		free(program);
	}
}

void destroyReportItem(ReportItem * reportItem) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	while (reportItem != NULL) {
		ReportItem * next = reportItem->next;
		free(reportItem->metric);
		free(reportItem);
		reportItem = next;
	}
}

void destroySimulation(Simulation * simulation) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (simulation != NULL) {
		free(simulation->gameName);
		free(simulation);
	}
}

void destroyStatement(Statement * statement) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	while (statement != NULL) {
		Statement * next = statement->next;
		switch (statement->type) {
			case ASSIGNMENT_STATEMENT:
			case USES_STATEMENT:
				destroyExpression(statement->assignment.target);
				destroyExpression(statement->assignment.value);
				free(statement->assignment.strategy);
				break;
			case BLOCK_STATEMENT:
				destroyStatement(statement->statements);
				break;
			case DECLARATION_STATEMENT:
				destroyVariableDeclaration(statement->declaration);
				break;
			case FOR_EACH_STATEMENT:
			case FOR_RANGE_STATEMENT:
				free(statement->forLoop.variable);
				destroyExpression(statement->forLoop.collection);
				destroyExpression(statement->forLoop.from);
				destroyExpression(statement->forLoop.to);
				destroyStatement(statement->forLoop.body);
				break;
			case IF_STATEMENT:
				destroyExpression(statement->ifStatement.condition);
				destroyStatement(statement->ifStatement.thenBranch);
				destroyStatement(statement->ifStatement.elseBranch);
				break;
			case LOG_STATEMENT:
				destroyLogPart(statement->log);
				break;
			case REPEAT_STATEMENT:
			case WHILE_STATEMENT:
				destroyExpression(statement->loop.condition);
				destroyStatement(statement->loop.body);
				break;
			case DRAW_STATEMENT:
			case GIVE_STATEMENT:
			case MOVE_STATEMENT:
			case PLACE_STATEMENT:
			case PLAY_STATEMENT:
			case SHUFFLE_STATEMENT:
			case TAKE_STATEMENT:
				destroyExpression(statement->action.subject);
				destroyExpression(statement->action.amount);
				destroyExpression(statement->action.source);
				destroyExpression(statement->action.target);
				break;
		}
		free(statement);
		statement = next;
	}
}

void destroyTopLevel(TopLevel * topLevel) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	while (topLevel != NULL) {
		TopLevel * next = topLevel->next;
		switch (topLevel->type) {
			case GAME_TOP_LEVEL:
				destroyGameDeclaration(topLevel->game);
				break;
			case REPORT_TOP_LEVEL:
				destroyReportItem(topLevel->report);
				break;
			case SIMULATION_TOP_LEVEL:
				destroySimulation(topLevel->simulation);
				break;
		}
		free(topLevel);
		topLevel = next;
	}
}

void destroyTypeSpec(TypeSpec * typeSpec) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (typeSpec != NULL) {
		free(typeSpec->name);
		free(typeSpec);
	}
}

void destroyValueSet(ValueSet * valueSet) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (valueSet != NULL) {
		destroyLiteral(valueSet->elements);
		free(valueSet);
	}
}

void destroyVariableDeclaration(VariableDeclaration * variableDeclaration) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (variableDeclaration != NULL) {
		destroyTypeSpec(variableDeclaration->type);
		free(variableDeclaration->name);
		destroyExpression(variableDeclaration->initializer);
		free(variableDeclaration);
	}
}
