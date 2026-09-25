#ifndef ABSTRACT_SYNTAX_TREE_HEADER
#define ABSTRACT_SYNTAX_TREE_HEADER

#include "../../support/logging/Logger.h"
#include "../../support/type/ModuleDestructor.h"
#include <stdbool.h>
#include <stdlib.h>

/** Initialize module's internal state. */
ModuleDestructor initializeAbstractSyntaxTreeModule();

typedef enum AggregationType AggregationType;
typedef enum BaseType BaseType;
typedef enum CardType CardType;
typedef enum CriterionType CriterionType;
typedef enum DeckBodyType DeckBodyType;
typedef enum ExpressionType ExpressionType;
typedef enum GameItemType GameItemType;
typedef enum LiteralType LiteralType;
typedef enum LogPartType LogPartType;
typedef enum ReportAggregator ReportAggregator;
typedef enum ReportItemType ReportItemType;
typedef enum StatementType StatementType;
typedef enum TopLevelType TopLevelType;
typedef enum ValueSetType ValueSetType;

typedef struct Card Card;
typedef struct CardField CardField;
typedef struct Criterion Criterion;
typedef struct DeckDeclaration DeckDeclaration;
typedef struct Expression Expression;
typedef struct ExpressionList ExpressionList;
typedef struct Field Field;
typedef struct GameDeclaration GameDeclaration;
typedef struct GameItem GameItem;
typedef struct Generator Generator;
typedef struct Literal Literal;
typedef struct LogPart LogPart;
typedef struct Policy Policy;
typedef struct Program Program;
typedef struct ReportItem ReportItem;
typedef struct Simulation Simulation;
typedef struct Statement Statement;
typedef struct TopLevel TopLevel;
typedef struct TypeSpec TypeSpec;
typedef struct ValueSet ValueSet;
typedef struct VariableDeclaration VariableDeclaration;

enum BaseType {
	BOOLEAN_TYPE,
	DECK_TYPE,
	DIE_TYPE,
	INTEGER_TYPE,
	NAMED_TYPE,
	PIECE_TYPE,
	PLAYER_TYPE,
	STRATEGY_TYPE,
	STRING_TYPE
};

struct TypeSpec {
	BaseType baseType;
	char * name;
	bool isArray;
};

enum LiteralType {
	BOOLEAN_LITERAL,
	INTEGER_LITERAL,
	STRING_LITERAL
};

struct Literal {
	union {
		bool boolean;
		int integer;
		char * string;
	};
	LiteralType type;
	Literal * next;
};

enum ValueSetType {
	RANGE_VALUE_SET,
	SET_VALUE_SET
};

struct ValueSet {
	int min;
	int max;
	Literal * elements;
	ValueSetType type;
};

enum ExpressionType {
	ADDITION_EXPRESSION,
	AND_EXPRESSION,
	DIVISION_EXPRESSION,
	EQUAL_EXPRESSION,
	GREATER_EQUAL_EXPRESSION,
	GREATER_EXPRESSION,
	LESS_EQUAL_EXPRESSION,
	LESS_EXPRESSION,
	MODULE_EXPRESSION,
	MULTIPLICATION_EXPRESSION,
	NOT_EQUAL_EXPRESSION,
	OR_EXPRESSION,
	SUBTRACTION_EXPRESSION,

	NEGATION_EXPRESSION,
	NOT_EXPRESSION,

	CALL_EXPRESSION,
	INDEX_EXPRESSION,
	MEMBER_EXPRESSION,

	ASK_EXPRESSION,
	AGGREGATION_EXPRESSION,
	BOARD_EXPRESSION,
	BOOLEAN_EXPRESSION,
	CURRENT_EXPRESSION,
	IDENTIFIER_EXPRESSION,
	INTEGER_EXPRESSION,
	NONE_EXPRESSION,
	OPTION_EXPRESSION,
	PLAYERS_EXPRESSION,
	ROLL_EXPRESSION,
	STRING_EXPRESSION,
	TURNS_EXPRESSION
};

enum AggregationType {
	COUNT_AGGREGATION,
	MAX_AGGREGATION,
	MIN_AGGREGATION,
	SELECT_AGGREGATION,
	SUM_AGGREGATION
};

struct Expression {
	union {
		struct {
			Expression * left;
			Expression * right;
		};

		Expression * operand;

		struct {
			Expression * object;
			union {
				char * member;
				Expression * index;
				ExpressionList * arguments;
			};
		} postfix;

		bool boolean;
		int integer;
		char * string;
		char * identifier;

		char * die;

		struct {
			Expression * player;
			char * decision;
			ExpressionList * arguments;
		} ask;

		struct {
			AggregationType type;
			char * variable;
			Expression * collection;
			Expression * where;
			Expression * by;
		} aggregation;
	};
	ExpressionType type;
};

struct ExpressionList {
	Expression * expression;
	ExpressionList * next;
};

struct VariableDeclaration {
	TypeSpec * type;
	char * name;
	Expression * initializer;
};

enum LogPartType {
	EXPRESSION_LOG_PART,
	TEXT_LOG_PART
};

struct LogPart {
	union {
		char * text;
		Expression * expression;
	};
	LogPartType type;
	LogPart * next;
};

enum StatementType {
	ASSIGNMENT_STATEMENT,
	BLOCK_STATEMENT,
	DECLARATION_STATEMENT,
	FOR_EACH_STATEMENT,
	FOR_RANGE_STATEMENT,
	IF_STATEMENT,
	LOG_STATEMENT,
	REPEAT_STATEMENT,
	USES_STATEMENT,
	WHILE_STATEMENT,

	DRAW_STATEMENT,
	GIVE_STATEMENT,
	MOVE_STATEMENT,
	PLACE_STATEMENT,
	PLAY_STATEMENT,
	SHUFFLE_STATEMENT,
	TAKE_STATEMENT
};

struct Statement {
	union {
		struct {
			Expression * target;
			Expression * value;
			char * strategy;
		} assignment;

		Statement * statements;

		VariableDeclaration * declaration;

		struct {
			char * variable;
			Expression * collection;
			Expression * from;
			Expression * to;
			Statement * body;
		} forLoop;

		struct {
			Expression * condition;
			Statement * thenBranch;
			Statement * elseBranch;
		} ifStatement;

		LogPart * log;

		struct {
			Expression * condition;
			Statement * body;
		} loop;

		struct {
			Expression * subject;
			Expression * amount;
			Expression * source;
			Expression * target;
		} action;
	};
	StatementType type;
	int line;
	Statement * next;
};

struct Field {
	TypeSpec * type;
	char * name;
	Field * next;
};

struct CardField {
	char * name;
	Expression * value;
	CardField * next;
};

enum CardType {
	NAMED_CARD,
	POSITIONAL_CARD
};

struct Card {
	union {
		CardField * fields;
		ExpressionList * values;
	};
	CardType type;
	Card * next;
};

struct Generator {
	char * field;
	ValueSet * values;
	Generator * next;
};

enum DeckBodyType {
	CARDS_DECK_BODY,
	EMPTY_DECK_BODY,
	GENERATORS_DECK_BODY
};

struct DeckDeclaration {
	char * name;
	char * cardType;
	bool perPlayer;
	union {
		Card * cards;
		Generator * generators;
	};
	DeckBodyType bodyType;
};

enum CriterionType {
	FIRST_CRITERION,
	INPUT_CRITERION,
	MAX_CRITERION,
	MIN_CRITERION,
	RANDOM_CRITERION
};

struct Criterion {
	Expression * expression;
	CriterionType type;
	Criterion * next;
};

struct Policy {
	char * decision;
	Criterion * criteria;
	Expression * where;
	Policy * next;
};

enum GameItemType {
	BOARD_ITEM,
	CARD_TYPE_ITEM,
	DECISION_ITEM,
	DECK_ITEM,
	DIE_ITEM,
	END_ITEM,
	METRIC_ITEM,
	PIECE_ITEM,
	PLAYERS_ITEM,
	PREPARE_ITEM,
	STRATEGY_ITEM,
	TURN_ITEM,
	TURN_ORDER_ITEM,
	VARIABLE_ITEM,
	WIN_ITEM
};

struct GameItem {
	union {
		ValueSet * players;

		int cells;

		struct {
			char * name;
			ValueSet * faces;
		} die;

		struct {
			char * name;
			bool perPlayer;
		} piece;

		struct {
			char * name;
			Field * fields;
		} cardType;

		DeckDeclaration * deck;

		VariableDeclaration * variable;

		struct {
			char * name;
			Expression * value;
		} metric;

		struct {
			char * name;
			TypeSpec * type;
		} decision;

		struct {
			char * name;
			Policy * policies;
		} strategy;

		Statement * block;

		bool clockwise;

		Expression * condition;

		int turns;
	};
	GameItemType type;
	int line;
	GameItem * next;
};

struct GameDeclaration {
	char * name;
	GameItem * items;
};

struct Simulation {
	int games;
	char * gameName;
	int players;
	bool hasSeed;
	int seed;
	bool verbose;
};

enum ReportItemType {
	AGGREGATE_REPORT_ITEM,
	WINRATE_BY_PLAYER_REPORT_ITEM,
	WINRATE_BY_STRATEGY_REPORT_ITEM,
	WINRATE_REPORT_ITEM
};

enum ReportAggregator {
	AVG_REPORT_AGGREGATOR,
	MAX_REPORT_AGGREGATOR,
	MIN_REPORT_AGGREGATOR
};

struct ReportItem {
	ReportAggregator aggregator;
	char * metric;
	ReportItemType type;
	ReportItem * next;
};

enum TopLevelType {
	GAME_TOP_LEVEL,
	REPORT_TOP_LEVEL,
	SIMULATION_TOP_LEVEL
};

struct TopLevel {
	union {
		GameDeclaration * game;
		ReportItem * report;
		Simulation * simulation;
	};
	TopLevelType type;
	int line;
	TopLevel * next;
};

struct Program {
	TopLevel * items;
};

/**
 * Node recursive super-duper-trambolik-destructors.
 */

void destroyCard(Card * card);
void destroyCardField(CardField * cardField);
void destroyCriterion(Criterion * criterion);
void destroyDeckDeclaration(DeckDeclaration * deckDeclaration);
void destroyExpression(Expression * expression);
void destroyExpressionList(ExpressionList * expressionList);
void destroyField(Field * field);
void destroyGameDeclaration(GameDeclaration * gameDeclaration);
void destroyGameItem(GameItem * gameItem);
void destroyGenerator(Generator * generator);
void destroyLiteral(Literal * literal);
void destroyLogPart(LogPart * logPart);
void destroyPolicy(Policy * policy);
void destroyProgram(Program * program);
void destroyReportItem(ReportItem * reportItem);
void destroySimulation(Simulation * simulation);
void destroyStatement(Statement * statement);
void destroyTopLevel(TopLevel * topLevel);
void destroyTypeSpec(TypeSpec * typeSpec);
void destroyValueSet(ValueSet * valueSet);
void destroyVariableDeclaration(VariableDeclaration * variableDeclaration);

#endif
