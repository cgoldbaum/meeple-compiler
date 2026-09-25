%{

#include "../../support/type/TokenLabel.h"
#include "AbstractSyntaxTree.h"
#include "BisonActions.h"
#include <stdio.h>

/**
 * The error reporting function for Bison parser.
 *
 * @see https://www.gnu.org/software/bison/manual/html_node/Error-Reporting-Function.html
 * @see https://www.gnu.org/software/bison/manual/html_node/Tracking-Locations.html
 */
void yyerror(const YYLTYPE * location, const char * message) {
	fprintf(stderr, "[ERROR][SyntacticAnalyzer] Line %d: %s.\n",
		location != NULL ? location->first_line : 0, message);
}

%}

// You touch this, and you die.
%define api.pure full
%define api.push-pull push
%define api.value.union.name SemanticValue
%define parse.error detailed
%locations

%union {
	/** Terminals. */

	signed int integer;
	char * string;
	TokenLabel token;

	BaseType baseType;
	bool boolean;
	Card * card;
	CardField * cardField;
	Criterion * criterion;
	DeckDeclaration * deckDeclaration;
	Expression * expression;
	ExpressionList * expressionList;
	Field * field;
	GameDeclaration * gameDeclaration;
	GameItem * gameItem;
	Generator * generator;
	Literal * literal;
	LogPart * logPart;
	Policy * policy;
	Program * program;
	ReportAggregator reportAggregator;
	ReportItem * reportItem;
	Simulation * simulation;
	Statement * statement;
	TopLevel * topLevel;
	TypeSpec * typeSpec;
	ValueSet * valueSet;
	VariableDeclaration * variableDeclaration;
}

%destructor { free($$); } <string>
%destructor { destroyCard($$); } <card>
%destructor { destroyCardField($$); } <cardField>
%destructor { destroyCriterion($$); } <criterion>
%destructor { destroyDeckDeclaration($$); } <deckDeclaration>
%destructor { destroyExpression($$); } <expression>
%destructor { destroyExpressionList($$); } <expressionList>
%destructor { destroyField($$); } <field>
%destructor { destroyGameDeclaration($$); } <gameDeclaration>
%destructor { destroyGameItem($$); } <gameItem>
%destructor { destroyGenerator($$); } <generator>
%destructor { destroyLiteral($$); } <literal>
%destructor { destroyLogPart($$); } <logPart>
%destructor { destroyPolicy($$); } <policy>
%destructor { destroyReportItem($$); } <reportItem>
%destructor { destroySimulation($$); } <simulation>
%destructor { destroyStatement($$); } <statement>
%destructor { destroyTopLevel($$); } <topLevel>
%destructor { destroyTypeSpec($$); } <typeSpec>
%destructor { destroyValueSet($$); } <valueSet>
%destructor { destroyVariableDeclaration($$); } <variableDeclaration>

/** Terminals: literales. */
%token <integer> INTEGER
%token <string> IDENTIFIER
%token <string> STRING

/**
 * Partes de una cadena interpolada del log (§13.7 del plan): "texto{",
 * "}texto{" y "}texto".
 */
%token <string> STRING_HEAD STRING_MIDDLE STRING_TAIL

/** Terminals: palabras clave (declaraciones). */
%token <token> GAME GAMES PLAYERS BOARD CELLS DIE FACES TO PIECE PER PLAYER
%token <token> DECK OF CARD CARDTYPE PREPARE TURN ORDER CLOCKWISE COUNTERCLOCKWISE
%token <token> DECISION STRATEGY PREFER FIRST RANDOM INPUT

/** Terminals: tipos clasicos. */
%token <token> INTEGER_T BOOLEAN_T STRING_T

/** Terminals: agregaciones. */
%token <token> MAX MIN SUM COUNT SELECT WHERE BY IN

/** Terminals: decisiones. */
%token <token> ASK FOR USES

/** Terminals: control de flujo. */
%token <token> IF ELSE WHILE REPEAT

/** Terminals: acciones del dominio. */
%token <token> ROLL PLACE ON MOVE FORWARD SHUFFLE DRAW FROM PLAY GIVE TAKE LOG

/** Terminals: condiciones de fin. */
%token <token> WIN WHEN END AFTER TURNS

/** Terminals: simulacion y reportes. */
%token <token> SEED SIMULATE VERBOSE WITH REPORT WINRATE AVG METRIC

/** Terminals: variables predefinidas y literales del dominio. */
%token <token> CURRENT OPTION NONE TRUE FALSE

/** Terminals: operadores. */
%token <token> AND OR NOT
%token <token> EQ NE LE GE LT GT ADD SUB MUL DIV MOD

/** Terminals: puntuacion. */
%token <token> ASSIGN SEMI COMMA COLON DOT
%token <token> LBRACE RBRACE LPAREN RPAREN LBRACKET RBRACKET

/** Terminals: infraestructura (no forman parte del alfabeto del lenguaje). */
%token <token> OPEN_COMMENT
%token <token> CLOSE_COMMENT
%token <token> IGNORED
%token <token> UNKNOWN

%type <baseType> gameVariableType keywordType
%type <boolean> perPlayerOpt turnOrderValue verboseOpt
%type <card> card cardList
%type <cardField> cardField cardFieldList
%type <criterion> criteria finalCriterion ranking rankingList
%type <deckDeclaration> deckBody
%type <expression> aggregation expression globalName initializerOpt lvalueTail postfix primary whereOpt
%type <expressionList> argumentList argumentListOpt
%type <field> field fieldList
%type <gameDeclaration> gameDeclaration
%type <gameItem> gameItem gameItemList
%type <generator> generator generatorList
%type <integer> seedOpt signedInteger
%type <literal> integerList literal literalList naturalList
%type <logPart> interpolatedString interpolationRest
%type <policy> policy policyList
%type <program> program
%type <reportAggregator> reportAggregator
%type <reportItem> reportItem reportItemList
%type <simulation> simulation
%type <statement> actionStatement block elseOpt globalStatement identStatement ifStatement keywordDeclaration lvalueEnd statement statementList
%type <string> memberName reportMetric
%type <topLevel> topLevel topLevelList
%type <typeSpec> baseType typeSpec
%type <valueSet> integerSet playerCount valueSet
%type <variableDeclaration> gameVariable

/**
 * Precedence and associativity.
 *
 * Se usa %precedence (y no %right) para NOT y UMINUS: son unarios, no tiene
 * sentido hablar de asociatividad y %right emite "useless associativity".
 *
 * @see https://www.gnu.org/software/bison/manual/html_node/Precedence.html
 */
%left OR
%left AND
%precedence NOT
%left EQ NE
%left LT GT LE GE
%left ADD SUB
%left MUL DIV MOD
%precedence UMINUS

%start program

%%

// IMPORTANT: To use λ in the following grammar, use the %empty symbol.

/**
 * La raiz es una lista plana que conserva el orden del codigo fuente: el §4.8
 * del PDF dice que "report" opera sobre la simulacion *anterior*, asi que el
 * orden es semantico y no se puede agrupar por tipo.
 */
program: topLevelList													{ $$ = ProgramSemanticAction($1); }
	;

topLevelList: topLevel													{ $$ = $1; }
	| topLevelList topLevel												{ $$ = TopLevelListSemanticAction($1, $2); }
	;

topLevel: gameDeclaration												{ $$ = GameTopLevelSemanticAction($1, @1.first_line); }
	| simulation														{ $$ = SimulationTopLevelSemanticAction($1, @1.first_line); }
	| REPORT LBRACE reportItemList RBRACE								{ $$ = ReportTopLevelSemanticAction($3, @1.first_line); }
	;

/**
 * La semilla y la verbosidad son clausulas de simulate (§13.4 del plan): ya no
 * existen "seed n;" ni "log level ...;" sueltos. El orden es fijo.
 */
simulation: SIMULATE INTEGER gameWord OF STRING WITH INTEGER PLAYERS seedOpt verboseOpt SEMI
																		{ $$ = SimulationSemanticAction($2, $5, $7, $9, $10); }
	;

gameWord: GAME
	| GAMES
	;

seedOpt: %empty															{ $$ = -1; }
	| SEED INTEGER														{ $$ = $2; }
	;

verboseOpt: %empty														{ $$ = false; }
	| VERBOSE															{ $$ = true; }
	;

reportItemList: reportItem												{ $$ = $1; }
	| reportItemList reportItem											{ $$ = ReportItemListSemanticAction($1, $2); }
	;

/**
 * Lo que se puede reportar (§13.10 del plan): las tres formas de winrate, y un
 * agregador sobre una metrica. "turns" es la metrica predefinida; el resto son
 * los "metric" declarados en el juego.
 */
reportItem: WINRATE SEMI												{ $$ = WinrateReportItemSemanticAction(WINRATE_REPORT_ITEM); }
	| WINRATE BY PLAYER SEMI											{ $$ = WinrateReportItemSemanticAction(WINRATE_BY_PLAYER_REPORT_ITEM); }
	| WINRATE BY STRATEGY SEMI											{ $$ = WinrateReportItemSemanticAction(WINRATE_BY_STRATEGY_REPORT_ITEM); }
	| reportAggregator reportMetric SEMI								{ $$ = AggregateReportItemSemanticAction($1, $2); }
	;

reportAggregator: AVG													{ $$ = AVG_REPORT_AGGREGATOR; }
	| MIN																{ $$ = MIN_REPORT_AGGREGATOR; }
	| MAX																{ $$ = MAX_REPORT_AGGREGATOR; }
	;

reportMetric: TURNS														{ $$ = NULL; }
	| IDENTIFIER														{ $$ = $1; }
	;

/** Componentes del juego (§4.2). */

gameDeclaration: GAME STRING LBRACE gameItemList RBRACE				{ $$ = GameDeclarationSemanticAction($2, $4); }
	;

gameItemList: gameItem													{ $$ = $1; }
	| gameItemList gameItem												{ $$ = GameItemListSemanticAction($1, $2); }
	;

gameItem: PLAYERS playerCount SEMI										{ $$ = PlayersGameItemSemanticAction($2, @1.first_line); }
	| BOARD CELLS INTEGER SEMI											{ $$ = BoardGameItemSemanticAction($3, @1.first_line); }
	| DIE IDENTIFIER FACES integerSet SEMI								{ $$ = DieGameItemSemanticAction($2, $4, @1.first_line); }
	| PIECE IDENTIFIER perPlayerOpt SEMI								{ $$ = PieceGameItemSemanticAction($2, $3, @1.first_line); }
	| CARDTYPE IDENTIFIER LBRACE fieldList RBRACE						{ $$ = CardTypeGameItemSemanticAction($2, $4, @1.first_line); }
	| DECK IDENTIFIER OF IDENTIFIER perPlayerOpt SEMI					{ $$ = DeckGameItemSemanticAction($2, $4, $5, NULL, @1.first_line); }
	| DECK IDENTIFIER OF IDENTIFIER LBRACE deckBody RBRACE				{ $$ = DeckGameItemSemanticAction($2, $4, false, $6, @1.first_line); }
	| gameVariable														{ $$ = VariableGameItemSemanticAction($1, @1.first_line); }
	| METRIC IDENTIFIER ASSIGN expression SEMI							{ $$ = MetricGameItemSemanticAction($2, $4, @1.first_line); }
	| DECISION IDENTIFIER COLON typeSpec SEMI							{ $$ = DecisionGameItemSemanticAction($2, $4, @1.first_line); }
	| STRATEGY IDENTIFIER LBRACE policyList RBRACE						{ $$ = StrategyGameItemSemanticAction($2, $4, @1.first_line); }
	| PREPARE block														{ $$ = BlockGameItemSemanticAction(PREPARE_ITEM, $2, @1.first_line); }
	| TURN block														{ $$ = BlockGameItemSemanticAction(TURN_ITEM, $2, @1.first_line); }
	| TURN ORDER turnOrderValue SEMI									{ $$ = TurnOrderGameItemSemanticAction($3, @1.first_line); }
	| WIN WHEN expression SEMI											{ $$ = WinGameItemSemanticAction($3, @1.first_line); }
	| END AFTER INTEGER TURNS SEMI										{ $$ = EndGameItemSemanticAction($3, @1.first_line); }
	;

/**
 * Variables del juego (§13.9 del plan). Los tipos se limitan a los que no
 * abren otra declaracion del juego: con "piece", "die" o "deck", la entrada
 * "piece p;" seria a la vez una variable y una ficha (conflicto R/R).
 */
gameVariable: gameVariableType IDENTIFIER initializerOpt SEMI
																		{ $$ = VariableDeclarationSemanticAction(TypeSpecSemanticAction($1, NULL, false), $2, $3); }
	| gameVariableType LBRACKET RBRACKET IDENTIFIER initializerOpt SEMI
																		{ $$ = VariableDeclarationSemanticAction(TypeSpecSemanticAction($1, NULL, true), $4, $5); }
	| IDENTIFIER IDENTIFIER initializerOpt SEMI							{ $$ = VariableDeclarationSemanticAction(TypeSpecSemanticAction(NAMED_TYPE, $1, false), $2, $3); }
	| IDENTIFIER LBRACKET RBRACKET IDENTIFIER initializerOpt SEMI		{ $$ = VariableDeclarationSemanticAction(TypeSpecSemanticAction(NAMED_TYPE, $1, true), $4, $5); }
	;

gameVariableType: INTEGER_T												{ $$ = INTEGER_TYPE; }
	| BOOLEAN_T															{ $$ = BOOLEAN_TYPE; }
	| STRING_T															{ $$ = STRING_TYPE; }
	| PLAYER															{ $$ = PLAYER_TYPE; }
	;

turnOrderValue: CLOCKWISE												{ $$ = true; }
	| COUNTERCLOCKWISE													{ $$ = false; }
	;

perPlayerOpt: %empty													{ $$ = false; }
	| PER PLAYER														{ $$ = true; }
	;

/**
 * Conjuntos de valores (§13.1 del plan): "A to B" abrevia {A, A+1, ..., B}, y
 * donde se acepta un rango tambien se acepta un conjunto. Nunca vacio.
 *
 * Los negativos solo se admiten en "die ... faces" y en los generadores de
 * mazo. En el resto de las declaraciones se exige INTEGER pelado, lo que
 * regala dos rechazos: "seed -1" y "cells -5".
 */
playerCount: INTEGER TO INTEGER											{
		if ($1 > $3) {
			yyerror(&@1, "rango invalido: min > max");
			YYERROR;
		}
		$$ = RangeValueSetSemanticAction($1, $3);
	}
	| LBRACE naturalList RBRACE											{ $$ = SetValueSetSemanticAction($2); }
	;

naturalList: INTEGER													{ $$ = IntegerLiteralSemanticAction($1); }
	| naturalList COMMA INTEGER											{ $$ = LiteralListSemanticAction($1, IntegerLiteralSemanticAction($3)); }
	;

integerSet: signedInteger TO signedInteger								{
		if ($1 > $3) {
			yyerror(&@1, "rango invalido: min > max");
			YYERROR;
		}
		$$ = RangeValueSetSemanticAction($1, $3);
	}
	| LBRACE integerList RBRACE											{ $$ = SetValueSetSemanticAction($2); }
	;

integerList: signedInteger												{ $$ = IntegerLiteralSemanticAction($1); }
	| integerList COMMA signedInteger									{ $$ = LiteralListSemanticAction($1, IntegerLiteralSemanticAction($3)); }
	;

signedInteger: INTEGER													{ $$ = $1; }
	| SUB INTEGER														{ $$ = -$2; }
	;

fieldList: field														{ $$ = $1; }
	| fieldList field													{ $$ = FieldListSemanticAction($1, $2); }
	;

field: typeSpec IDENTIFIER SEMI											{ $$ = FieldSemanticAction($1, $2); }
	;

/**
 * El cuerpo de un mazo es una lista de cartas O una lista de generadores, sin
 * mezclar (§13.2 del plan). Los generadores arman el producto cartesiano de
 * sus conjuntos de valores.
 */
deckBody: cardList														{ $$ = CardsDeckBodySemanticAction($1); }
	| generatorList														{ $$ = GeneratorsDeckBodySemanticAction($1); }
	;

cardList: card															{ $$ = $1; }
	| cardList card														{ $$ = CardListSemanticAction($1, $2); }
	;

/**
 * Carta con nombres ("card { ataque: 1; }") o posicional ("card {1, 1}"). LALR
 * decide con el token que sigue al primer IDENTIFIER: ":" abre la forma con
 * nombres.
 */
card: CARD LBRACE cardFieldList RBRACE									{ $$ = NamedCardSemanticAction($3); }
	| CARD LBRACE argumentList RBRACE									{ $$ = PositionalCardSemanticAction($3); }
	;

cardFieldList: cardField												{ $$ = $1; }
	| cardFieldList cardField											{ $$ = CardFieldListSemanticAction($1, $2); }
	;

cardField: IDENTIFIER COLON expression SEMI								{ $$ = CardFieldSemanticAction($1, $3); }
	;

generatorList: generator												{ $$ = $1; }
	| generatorList generator											{ $$ = GeneratorListSemanticAction($1, $2); }
	;

generator: IDENTIFIER COLON valueSet SEMI								{ $$ = GeneratorSemanticAction($1, $3); }
	;

valueSet: signedInteger TO signedInteger								{
		if ($1 > $3) {
			yyerror(&@1, "rango invalido: min > max");
			YYERROR;
		}
		$$ = RangeValueSetSemanticAction($1, $3);
	}
	| LBRACE literalList RBRACE											{ $$ = SetValueSetSemanticAction($2); }
	;

literalList: literal													{ $$ = $1; }
	| literalList COMMA literal											{ $$ = LiteralListSemanticAction($1, $3); }
	;

literal: signedInteger													{ $$ = IntegerLiteralSemanticAction($1); }
	| STRING															{ $$ = StringLiteralSemanticAction($1); }
	| TRUE																{ $$ = BooleanLiteralSemanticAction(true); }
	| FALSE																{ $$ = BooleanLiteralSemanticAction(false); }
	;

/** Decisiones y estrategias (§4.6). */

policyList: policy														{ $$ = $1; }
	| policyList policy													{ $$ = PolicyListSemanticAction($1, $2); }
	;

/**
 * Politicas (§13.8 del plan): criterios encadenados con coma, donde cada uno
 * desempata al anterior, y un filtro "where" opcional. Los criterios que no
 * ordenan (random, first, input) solo pueden ir al final: despues de ellos no
 * queda nada que desempatar. Lo garantiza la gramatica.
 */
policy: IDENTIFIER COLON PREFER criteria whereOpt SEMI					{ $$ = PolicySemanticAction($1, $4, $5); }
	;

criteria: rankingList													{ $$ = $1; }
	| rankingList COMMA finalCriterion									{ $$ = CriterionListSemanticAction($1, $3); }
	| finalCriterion													{ $$ = $1; }
	;

rankingList: ranking													{ $$ = $1; }
	| rankingList COMMA ranking											{ $$ = CriterionListSemanticAction($1, $3); }
	;

ranking: MAX expression													{ $$ = CriterionSemanticAction(MAX_CRITERION, $2); }
	| MIN expression													{ $$ = CriterionSemanticAction(MIN_CRITERION, $2); }
	;

finalCriterion: RANDOM													{ $$ = CriterionSemanticAction(RANDOM_CRITERION, NULL); }
	| FIRST																{ $$ = CriterionSemanticAction(FIRST_CRITERION, NULL); }
	| INPUT																{ $$ = CriterionSemanticAction(INPUT_CRITERION, NULL); }
	;

typeSpec: baseType														{ $$ = $1; }
	| baseType LBRACKET RBRACKET										{ $$ = ArrayTypeSpecSemanticAction($1); }
	;

baseType: INTEGER_T														{ $$ = TypeSpecSemanticAction(INTEGER_TYPE, NULL, false); }
	| BOOLEAN_T															{ $$ = TypeSpecSemanticAction(BOOLEAN_TYPE, NULL, false); }
	| STRING_T															{ $$ = TypeSpecSemanticAction(STRING_TYPE, NULL, false); }
	| PLAYER															{ $$ = TypeSpecSemanticAction(PLAYER_TYPE, NULL, false); }
	| PIECE																{ $$ = TypeSpecSemanticAction(PIECE_TYPE, NULL, false); }
	| DIE																{ $$ = TypeSpecSemanticAction(DIE_TYPE, NULL, false); }
	| DECK																{ $$ = TypeSpecSemanticAction(DECK_TYPE, NULL, false); }
	| STRATEGY															{ $$ = TypeSpecSemanticAction(STRATEGY_TYPE, NULL, false); }
	| IDENTIFIER														{ $$ = TypeSpecSemanticAction(NAMED_TYPE, $1, false); }
	;

/** Control de flujo (§4.4). */

block: LBRACE statementList RBRACE										{ $$ = BlockStatementSemanticAction($2, @1.first_line); }
	| LBRACE RBRACE														{ $$ = BlockStatementSemanticAction(NULL, @1.first_line); }
	;

statementList: statement												{ $$ = $1; }
	| statementList statement											{ $$ = StatementListSemanticAction($1, $2); }
	;

statement: identStatement												{ $$ = $1; }
	| globalStatement													{ $$ = $1; }
	| keywordDeclaration												{ $$ = $1; }
	| ifStatement														{ $$ = $1; }
	| WHILE LPAREN expression RPAREN block								{ $$ = LoopStatementSemanticAction(WHILE_STATEMENT, $3, $5, @1.first_line); }
	| REPEAT expression block											{ $$ = LoopStatementSemanticAction(REPEAT_STATEMENT, $2, $3, @1.first_line); }
	| FOR LPAREN IDENTIFIER IN expression RPAREN block					{ $$ = ForEachStatementSemanticAction($3, $5, $7, @1.first_line); }
	| FOR LPAREN IDENTIFIER IN expression TO expression RPAREN block	{ $$ = ForRangeStatementSemanticAction($3, $5, $7, $9, @1.first_line); }
	| actionStatement													{ $$ = $1; }
	| block																{ $$ = $1; }
	;

/**
 * Factorizacion a izquierda. Un IDENTIFIER inicial puede abrir una declaracion
 * de tipo cardtype ("Carta elegida = ...;") o un lvalue ("Mazo[0].valor = 9;").
 * La escritura natural da un shift/reduce sobre "[" que rompe el segundo caso.
 * Asi ningun camino exige reducir en el IDENTIFIER: ambos desplazan "[" y el
 * token siguiente ("]" vs. una expresion) decide.
 *
 * lvalueTail es NO vacio a proposito: con %empty el conflicto vuelve.
 */
identStatement: IDENTIFIER IDENTIFIER initializerOpt SEMI
																		{ $$ = DeclarationStatementSemanticAction(VariableDeclarationSemanticAction(TypeSpecSemanticAction(NAMED_TYPE, $1, false), $2, $3), @1.first_line); }
	| IDENTIFIER LBRACKET RBRACKET IDENTIFIER initializerOpt SEMI
																		{ $$ = DeclarationStatementSemanticAction(VariableDeclarationSemanticAction(TypeSpecSemanticAction(NAMED_TYPE, $1, true), $4, $5), @1.first_line); }
	| IDENTIFIER lvalueEnd												{ $$ = LvalueStatementSemanticAction(IdentifierExpressionSemanticAction($1), NULL, $2, @1.first_line); }
	| IDENTIFIER lvalueTail lvalueEnd									{ $$ = LvalueStatementSemanticAction(IdentifierExpressionSemanticAction($1), $2, $3, @1.first_line); }
	;

lvalueTail: DOT memberName												{ $$ = MemberExpressionSemanticAction(NULL, $2); }
	| LBRACKET expression RBRACKET										{ $$ = IndexExpressionSemanticAction(NULL, $2); }
	| lvalueTail DOT memberName											{ $$ = MemberExpressionSemanticAction($1, $3); }
	| lvalueTail LBRACKET expression RBRACKET							{ $$ = IndexExpressionSemanticAction($1, $3); }
	;

lvalueEnd: ASSIGN expression SEMI										{ $$ = AssignmentStatementSemanticAction($2); }
	| USES IDENTIFIER SEMI												{ $$ = UsesStatementSemanticAction($2); }
	;

/**
 * Las variables predefinidas tambien son lvalues ("players[0] uses X;"), pero
 * no son IDENTIFIER, asi que necesitan su propia produccion.
 */
globalStatement: globalName lvalueTail lvalueEnd						{ $$ = LvalueStatementSemanticAction($1, $2, $3, @1.first_line); }
	;

globalName: CURRENT														{ $$ = PredefinedExpressionSemanticAction(CURRENT_EXPRESSION); }
	| PLAYERS															{ $$ = PredefinedExpressionSemanticAction(PLAYERS_EXPRESSION); }
	| OPTION															{ $$ = PredefinedExpressionSemanticAction(OPTION_EXPRESSION); }
	;

keywordDeclaration: keywordType IDENTIFIER initializerOpt SEMI
																		{ $$ = DeclarationStatementSemanticAction(VariableDeclarationSemanticAction(TypeSpecSemanticAction($1, NULL, false), $2, $3), @1.first_line); }
	| keywordType LBRACKET RBRACKET IDENTIFIER initializerOpt SEMI
																		{ $$ = DeclarationStatementSemanticAction(VariableDeclarationSemanticAction(TypeSpecSemanticAction($1, NULL, true), $4, $5), @1.first_line); }
	;

keywordType: INTEGER_T													{ $$ = INTEGER_TYPE; }
	| BOOLEAN_T															{ $$ = BOOLEAN_TYPE; }
	| STRING_T															{ $$ = STRING_TYPE; }
	| PLAYER															{ $$ = PLAYER_TYPE; }
	| PIECE																{ $$ = PIECE_TYPE; }
	| DIE																{ $$ = DIE_TYPE; }
	| DECK																{ $$ = DECK_TYPE; }
	| STRATEGY															{ $$ = STRATEGY_TYPE; }
	;

initializerOpt: %empty													{ $$ = NULL; }
	| ASSIGN expression													{ $$ = $2; }
	;

/**
 * Las llaves obligatorias eliminan el dangling-else sin precedencias: ELSE no
 * pertenece a FIRST(statement), por lo tanto tampoco a FOLLOW(ifStatement).
 */
ifStatement: IF LPAREN expression RPAREN block elseOpt					{ $$ = IfStatementSemanticAction($3, $5, $6, @1.first_line); }
	;

elseOpt: %empty															{ $$ = NULL; }
	| ELSE block														{ $$ = $2; }
	| ELSE ifStatement													{ $$ = $2; }
	;

/** Acciones del dominio (§4.5). */
actionStatement: PLACE postfix ON expression SEMI						{ $$ = ActionStatementSemanticAction(PLACE_STATEMENT, $2, NULL, NULL, $4, @1.first_line); }
	| MOVE postfix FORWARD expression SEMI								{ $$ = ActionStatementSemanticAction(MOVE_STATEMENT, $2, $4, NULL, NULL, @1.first_line); }
	| SHUFFLE postfix SEMI												{ $$ = ActionStatementSemanticAction(SHUFFLE_STATEMENT, $2, NULL, NULL, NULL, @1.first_line); }
	| DRAW expression FROM postfix TO postfix SEMI						{ $$ = ActionStatementSemanticAction(DRAW_STATEMENT, NULL, $2, $4, $6, @1.first_line); }
	| PLAY expression FROM postfix TO postfix SEMI						{ $$ = ActionStatementSemanticAction(PLAY_STATEMENT, $2, NULL, $4, $6, @1.first_line); }
	| GIVE expression TO postfix SEMI									{ $$ = ActionStatementSemanticAction(GIVE_STATEMENT, NULL, $2, NULL, $4, @1.first_line); }
	| TAKE expression FROM postfix SEMI									{ $$ = ActionStatementSemanticAction(TAKE_STATEMENT, NULL, $2, $4, NULL, @1.first_line); }
	| LOG interpolatedString SEMI										{ $$ = LogStatementSemanticAction($2, @1.first_line); }
	;

/**
 * Cadena del log, con interpolaciones "{expr}" (§13.7 del plan). El lexer ya
 * la parte en STRING_HEAD, STRING_MIDDLE y STRING_TAIL. Una interpolacion
 * vacia ("{}") o sin cerrar es un error de sintaxis.
 */
interpolatedString: STRING												{ $$ = TextLogPartSemanticAction($1); }
	| STRING_HEAD expression interpolationRest							{ $$ = InterpolationLogPartSemanticAction($1, $2, $3); }
	;

interpolationRest: STRING_TAIL											{ $$ = TextLogPartSemanticAction($1); }
	| STRING_MIDDLE expression interpolationRest						{ $$ = InterpolationLogPartSemanticAction($1, $2, $3); }
	;

/** Expresiones y agregaciones (§4.3). */

expression: expression ADD expression									{ $$ = BinaryExpressionSemanticAction($1, $3, ADDITION_EXPRESSION); }
	| expression SUB expression											{ $$ = BinaryExpressionSemanticAction($1, $3, SUBTRACTION_EXPRESSION); }
	| expression MUL expression											{ $$ = BinaryExpressionSemanticAction($1, $3, MULTIPLICATION_EXPRESSION); }
	| expression DIV expression											{ $$ = BinaryExpressionSemanticAction($1, $3, DIVISION_EXPRESSION); }
	| expression MOD expression											{ $$ = BinaryExpressionSemanticAction($1, $3, MODULE_EXPRESSION); }
	| expression LT expression											{ $$ = BinaryExpressionSemanticAction($1, $3, LESS_EXPRESSION); }
	| expression GT expression											{ $$ = BinaryExpressionSemanticAction($1, $3, GREATER_EXPRESSION); }
	| expression LE expression											{ $$ = BinaryExpressionSemanticAction($1, $3, LESS_EQUAL_EXPRESSION); }
	| expression GE expression											{ $$ = BinaryExpressionSemanticAction($1, $3, GREATER_EQUAL_EXPRESSION); }
	| expression EQ expression											{ $$ = BinaryExpressionSemanticAction($1, $3, EQUAL_EXPRESSION); }
	| expression NE expression											{ $$ = BinaryExpressionSemanticAction($1, $3, NOT_EQUAL_EXPRESSION); }
	| expression AND expression											{ $$ = BinaryExpressionSemanticAction($1, $3, AND_EXPRESSION); }
	| expression OR expression											{ $$ = BinaryExpressionSemanticAction($1, $3, OR_EXPRESSION); }
	| NOT expression													{ $$ = UnaryExpressionSemanticAction($2, NOT_EXPRESSION); }
	| SUB expression %prec UMINUS										{ $$ = UnaryExpressionSemanticAction($2, NEGATION_EXPRESSION); }
	| postfix															{ $$ = $1; }
	;

postfix: primary														{ $$ = $1; }
	| postfix DOT memberName											{ $$ = MemberExpressionSemanticAction($1, $3); }
	| postfix LBRACKET expression RBRACKET								{ $$ = IndexExpressionSemanticAction($1, $3); }
	| postfix LPAREN argumentListOpt RPAREN								{ $$ = CallExpressionSemanticAction($1, $3); }
	;

/**
 * Una keyword entra en memberName si y solo si la tabla de miembros del §4.1
 * define un miembro con ese nombre exacto: die.max, die.min, die.faces,
 * player.strategy, deck.cardtype. PLAYER no entra: no existe ".player".
 */
memberName: IDENTIFIER													{ $$ = $1; }
	| MAX																{ $$ = MemberNameSemanticAction("max"); }
	| MIN																{ $$ = MemberNameSemanticAction("min"); }
	| STRATEGY															{ $$ = MemberNameSemanticAction("strategy"); }
	| CARDTYPE															{ $$ = MemberNameSemanticAction("cardtype"); }
	| FACES																{ $$ = MemberNameSemanticAction("faces"); }
	;

argumentListOpt: %empty													{ $$ = NULL; }
	| argumentList														{ $$ = $1; }
	;

argumentList: expression												{ $$ = ExpressionListSemanticAction(NULL, $1); }
	| argumentList COMMA expression										{ $$ = ExpressionListSemanticAction($1, $3); }
	;

primary: INTEGER														{ $$ = IntegerExpressionSemanticAction($1); }
	| STRING															{ $$ = StringExpressionSemanticAction($1); }
	| TRUE																{ $$ = BooleanExpressionSemanticAction(true); }
	| FALSE																{ $$ = BooleanExpressionSemanticAction(false); }
	| NONE																{ $$ = PredefinedExpressionSemanticAction(NONE_EXPRESSION); }
	| IDENTIFIER														{ $$ = IdentifierExpressionSemanticAction($1); }
	| CURRENT															{ $$ = PredefinedExpressionSemanticAction(CURRENT_EXPRESSION); }
	| PLAYERS															{ $$ = PredefinedExpressionSemanticAction(PLAYERS_EXPRESSION); }
	| TURNS																{ $$ = PredefinedExpressionSemanticAction(TURNS_EXPRESSION); }
	| OPTION															{ $$ = PredefinedExpressionSemanticAction(OPTION_EXPRESSION); }
	| BOARD																{ $$ = PredefinedExpressionSemanticAction(BOARD_EXPRESSION); }
	| LPAREN expression RPAREN											{ $$ = $2; }
	| ROLL IDENTIFIER													{ $$ = RollExpressionSemanticAction($2); }
	| ASK expression FOR IDENTIFIER LPAREN argumentListOpt RPAREN		{ $$ = AskExpressionSemanticAction($2, $4, $6); }
	| aggregation														{ $$ = $1; }
	;

aggregation: COUNT LPAREN IDENTIFIER IN expression whereOpt RPAREN		{ $$ = AggregationExpressionSemanticAction(COUNT_AGGREGATION, $3, $5, $6, NULL); }
	| SELECT LPAREN IDENTIFIER IN expression whereOpt RPAREN			{ $$ = AggregationExpressionSemanticAction(SELECT_AGGREGATION, $3, $5, $6, NULL); }
	| SUM LPAREN IDENTIFIER IN expression whereOpt BY expression RPAREN	{ $$ = AggregationExpressionSemanticAction(SUM_AGGREGATION, $3, $5, $6, $8); }
	| MAX LPAREN IDENTIFIER IN expression whereOpt BY expression RPAREN	{ $$ = AggregationExpressionSemanticAction(MAX_AGGREGATION, $3, $5, $6, $8); }
	| MIN LPAREN IDENTIFIER IN expression whereOpt BY expression RPAREN	{ $$ = AggregationExpressionSemanticAction(MIN_AGGREGATION, $3, $5, $6, $8); }
	;

whereOpt: %empty														{ $$ = NULL; }
	| WHERE expression													{ $$ = $2; }
	;

%%
