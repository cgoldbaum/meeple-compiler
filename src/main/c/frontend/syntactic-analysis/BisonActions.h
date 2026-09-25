#ifndef BISON_ACTIONS_HEADER
#define BISON_ACTIONS_HEADER

#include "../../support/logging/Logger.h"
#include "../../support/type/CompilerState.h"
#include "../../support/type/ModuleDestructor.h"
#include "../../support/type/TokenLabel.h"
#include "AbstractSyntaxTree.h"
#include "BisonParser.h"
#include <stdlib.h>
#include <string.h>

/** Initialize module's internal state. */
ModuleDestructor initializeBisonActionsModule(CompilerState * compilerState);

/**
 * Bison semantic actions.
 */

Card * CardListSemanticAction(Card * list, Card * item);
CardField * CardFieldListSemanticAction(CardField * list, CardField * item);
Criterion * CriterionListSemanticAction(Criterion * list, Criterion * item);
ExpressionList * ExpressionListSemanticAction(ExpressionList * list, Expression * expression);
Field * FieldListSemanticAction(Field * list, Field * item);
GameItem * GameItemListSemanticAction(GameItem * list, GameItem * item);
Generator * GeneratorListSemanticAction(Generator * list, Generator * item);
Literal * LiteralListSemanticAction(Literal * list, Literal * item);
Policy * PolicyListSemanticAction(Policy * list, Policy * item);
ReportItem * ReportItemListSemanticAction(ReportItem * list, ReportItem * item);
Statement * StatementListSemanticAction(Statement * list, Statement * item);
TopLevel * TopLevelListSemanticAction(TopLevel * list, TopLevel * item);

Program * ProgramSemanticAction(TopLevel * items);
TopLevel * GameTopLevelSemanticAction(GameDeclaration * game, const int line);
TopLevel * ReportTopLevelSemanticAction(ReportItem * report, const int line);
TopLevel * SimulationTopLevelSemanticAction(Simulation * simulation, const int line);
Simulation * SimulationSemanticAction(const int games, char * gameName, const int players, const int seed, const bool verbose);
ReportItem * AggregateReportItemSemanticAction(const ReportAggregator aggregator, char * metric);
ReportItem * WinrateReportItemSemanticAction(const ReportItemType type);

TypeSpec * ArrayTypeSpecSemanticAction(TypeSpec * typeSpec);
TypeSpec * TypeSpecSemanticAction(const BaseType baseType, char * name, const bool isArray);
Literal * BooleanLiteralSemanticAction(const bool value);
Literal * IntegerLiteralSemanticAction(const int value);
Literal * StringLiteralSemanticAction(char * value);
ValueSet * RangeValueSetSemanticAction(const int min, const int max);
ValueSet * SetValueSetSemanticAction(Literal * elements);

GameDeclaration * GameDeclarationSemanticAction(char * name, GameItem * items);
GameItem * BlockGameItemSemanticAction(const GameItemType type, Statement * block, const int line);
GameItem * BoardGameItemSemanticAction(const int cells, const int line);
GameItem * CardTypeGameItemSemanticAction(char * name, Field * fields, const int line);
GameItem * DecisionGameItemSemanticAction(char * name, TypeSpec * type, const int line);
GameItem * DeckGameItemSemanticAction(char * name, char * cardType, const bool perPlayer, DeckDeclaration * body, const int line);
GameItem * DieGameItemSemanticAction(char * name, ValueSet * faces, const int line);
GameItem * EndGameItemSemanticAction(const int turns, const int line);
GameItem * MetricGameItemSemanticAction(char * name, Expression * value, const int line);
GameItem * PieceGameItemSemanticAction(char * name, const bool perPlayer, const int line);
GameItem * PlayersGameItemSemanticAction(ValueSet * players, const int line);
GameItem * StrategyGameItemSemanticAction(char * name, Policy * policies, const int line);
GameItem * TurnOrderGameItemSemanticAction(const bool clockwise, const int line);
GameItem * VariableGameItemSemanticAction(VariableDeclaration * variable, const int line);
GameItem * WinGameItemSemanticAction(Expression * condition, const int line);
Field * FieldSemanticAction(TypeSpec * type, char * name);
DeckDeclaration * CardsDeckBodySemanticAction(Card * cards);
DeckDeclaration * GeneratorsDeckBodySemanticAction(Generator * generators);
Card * NamedCardSemanticAction(CardField * fields);
Card * PositionalCardSemanticAction(ExpressionList * values);
CardField * CardFieldSemanticAction(char * name, Expression * value);
Generator * GeneratorSemanticAction(char * field, ValueSet * values);
Policy * PolicySemanticAction(char * decision, Criterion * criteria, Expression * where);
Criterion * CriterionSemanticAction(const CriterionType type, Expression * expression);

VariableDeclaration * VariableDeclarationSemanticAction(TypeSpec * type, char * name, Expression * initializer);
Statement * ActionStatementSemanticAction(const StatementType type, Expression * subject, Expression * amount, Expression * source, Expression * target, const int line);
Statement * AssignmentStatementSemanticAction(Expression * value);
Statement * BlockStatementSemanticAction(Statement * statements, const int line);
Statement * DeclarationStatementSemanticAction(VariableDeclaration * declaration, const int line);
Statement * ForEachStatementSemanticAction(char * variable, Expression * collection, Statement * body, const int line);
Statement * ForRangeStatementSemanticAction(char * variable, Expression * from, Expression * to, Statement * body, const int line);
Statement * IfStatementSemanticAction(Expression * condition, Statement * thenBranch, Statement * elseBranch, const int line);
Statement * LogStatementSemanticAction(LogPart * parts, const int line);
Statement * LoopStatementSemanticAction(const StatementType type, Expression * condition, Statement * body, const int line);
Statement * LvalueStatementSemanticAction(Expression * base, Expression * tail, Statement * end, const int line);
Statement * UsesStatementSemanticAction(char * strategy);
LogPart * InterpolationLogPartSemanticAction(char * text, Expression * expression, LogPart * rest);
LogPart * TextLogPartSemanticAction(char * text);

Expression * AggregationExpressionSemanticAction(const AggregationType type, char * variable, Expression * collection, Expression * where, Expression * by);
Expression * AskExpressionSemanticAction(Expression * player, char * decision, ExpressionList * arguments);
Expression * BinaryExpressionSemanticAction(Expression * left, Expression * right, const ExpressionType type);
Expression * BooleanExpressionSemanticAction(const bool value);
Expression * CallExpressionSemanticAction(Expression * object, ExpressionList * arguments);
Expression * IdentifierExpressionSemanticAction(char * identifier);
Expression * IndexExpressionSemanticAction(Expression * object, Expression * index);
Expression * IntegerExpressionSemanticAction(const int value);
Expression * MemberExpressionSemanticAction(Expression * object, char * member);
Expression * PredefinedExpressionSemanticAction(const ExpressionType type);
Expression * RollExpressionSemanticAction(char * die);
Expression * StringExpressionSemanticAction(char * value);
Expression * UnaryExpressionSemanticAction(Expression * operand, const ExpressionType type);
char * MemberNameSemanticAction(const char * keyword);

#endif
