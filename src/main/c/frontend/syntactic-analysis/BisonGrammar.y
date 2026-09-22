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
}

/**
 * Terminals: literales.
 *
 * IDENTIFIER y STRING todavia NO declaran tipo, y es a proposito. Este hito
 * porta la gramatica con acciones vacias: si los terminales acarrearan el
 * "char *" que reserva Flex, ninguna accion tomaria posesion del puntero y
 * cada identificador del programa seria una fuga (bison lo avisa con 28
 * "unused value", y AddressSanitizer haria fallar los tests).
 *
 * Cuando existan las acciones que construyen el AST se agregan aca:
 *
 *   %token <string> IDENTIFIER STRING
 *   %destructor { free($$); } <string>
 *
 * junto con el cambio de TokenLexemeAction a Identifier/StringLexemeAction en
 * FlexPatterns.l, que ya estan escritas y esperando.
 */
%token <integer> INTEGER
%token IDENTIFIER
%token STRING

/** Terminals: palabras clave (declaraciones). */
%token <token> GAME GAMES PLAYERS BOARD CELLS DIE FACES TO PIECE PER PLAYER
%token <token> DECK OF CARD CARDTYPE SETUP TURN ORDER CLOCKWISE COUNTERCLOCKWISE
%token <token> DECISION STRATEGY PREFER FIRST RANDOM

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
%token <token> LEVEL VERBOSE

/** Terminals: condiciones de fin. */
%token <token> WIN WHEN END AFTER TURNS

/** Terminals: simulacion y reportes. */
%token <token> SEED SIMULATE WITH REPORT WINRATE AVG

/** Terminals: variables predefinidas y literales del dominio. */
%token <token> CURRENT TURN_NUMBER OPTION NONE TRUE FALSE

/** Terminals: operadores. */
%token <token> AND OR NOT ARROW
%token <token> EQ NE LE GE LT GT ADD SUB MUL DIV MOD

/** Terminals: puntuacion. */
%token <token> ASSIGN SEMI COMMA COLON DOT
%token <token> LBRACE RBRACE LPAREN RPAREN LBRACKET RBRACKET

/** Terminals: infraestructura (no forman parte del alfabeto del lenguaje). */
%token <token> OPEN_COMMENT
%token <token> CLOSE_COMMENT
%token <token> IGNORED
%token <token> UNKNOWN

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
program: topLevelList
	;

topLevelList: topLevel
	| topLevelList topLevel
	;

topLevel: SEED INTEGER SEMI
	| gameDeclaration
	| SIMULATE INTEGER gameWord OF STRING WITH INTEGER PLAYERS SEMI
	| REPORT LBRACE reportItemList RBRACE
	| LOG LEVEL logLevelValue SEMI
	;

gameWord: GAME
	| GAMES
	;

logLevelValue: VERBOSE
	| NONE
	;

reportItemList: reportItem
	| reportItemList reportItem
	;

reportItem: WINRATE BY PLAYER SEMI
	| WINRATE BY STRATEGY SEMI
	| AVG TURNS SEMI
	;

/** Componentes del juego (§4.2). */

gameDeclaration: GAME STRING LBRACE gameItemList RBRACE
	;

gameItemList: gameItem
	| gameItemList gameItem
	;

gameItem: PLAYERS INTEGER TO INTEGER SEMI
	| BOARD IDENTIFIER CELLS INTEGER SEMI
	| DIE IDENTIFIER FACES signedInteger TO signedInteger SEMI
	| DIE IDENTIFIER FACES LBRACE integerList RBRACE SEMI
	| PIECE IDENTIFIER perPlayerOpt SEMI
	| CARDTYPE IDENTIFIER LBRACE fieldList RBRACE
	| DECK IDENTIFIER OF IDENTIFIER perPlayerOpt SEMI
	| DECK IDENTIFIER OF IDENTIFIER LBRACE cardList RBRACE
	| DECISION IDENTIFIER LPAREN parameterList RPAREN ARROW typeSpec SEMI
	| STRATEGY IDENTIFIER LBRACE policyList RBRACE
	| SETUP block
	| TURN block
	| TURN ORDER turnOrderValue SEMI
	| WIN WHEN expression SEMI
	| END AFTER INTEGER TURNS SEMI
	;

turnOrderValue: CLOCKWISE
	| COUNTERCLOCKWISE
	;

perPlayerOpt: %empty
	| PER PLAYER
	;

/**
 * Los negativos con signo solo se admiten en "die ... faces", que es el unico
 * lugar donde el PDF los muestra. En el resto de las declaraciones se exige
 * INTEGER pelado, lo que regala dos rechazos: "seed -1;" y "cells -5;".
 */
integerList: signedInteger
	| integerList COMMA signedInteger
	;

signedInteger: INTEGER
	| SUB INTEGER
	;

fieldList: field
	| fieldList field
	;

field: typeSpec IDENTIFIER SEMI
	;

cardList: card
	| cardList card
	;

card: CARD LBRACE cardFieldList RBRACE
	;

cardFieldList: cardField
	| cardFieldList cardField
	;

cardField: IDENTIFIER COLON expression SEMI
	;

/** Decisiones y estrategias (§4.6). */

parameterList: parameter
	| parameterList COMMA parameter
	;

parameter: IDENTIFIER COLON typeSpec
	;

policyList: policy
	| policyList policy
	;

policy: IDENTIFIER COLON PREFER policyBody SEMI
	;

policyBody: MAX expression
	| MIN expression
	| RANDOM
	| FIRST
	;

typeSpec: baseType
	| baseType LBRACKET RBRACKET
	;

baseType: INTEGER_T
	| BOOLEAN_T
	| STRING_T
	| PLAYER
	| BOARD
	| PIECE
	| DIE
	| DECK
	| STRATEGY
	| IDENTIFIER
	;

/** Control de flujo (§4.4). */

block: LBRACE statementList RBRACE
	| LBRACE RBRACE
	;

statementList: statement
	| statementList statement
	;

statement: identStatement
	| globalStatement
	| keywordDeclaration
	| ifStatement
	| WHILE LPAREN expression RPAREN block
	| REPEAT expression block
	| FOR LPAREN IDENTIFIER IN expression RPAREN block
	| actionStatement
	| block
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
	| IDENTIFIER LBRACKET RBRACKET IDENTIFIER initializerOpt SEMI
	| IDENTIFIER lvalueEnd
	| IDENTIFIER lvalueTail lvalueEnd
	;

lvalueTail: DOT memberName
	| LBRACKET expression RBRACKET
	| lvalueTail DOT memberName
	| lvalueTail LBRACKET expression RBRACKET
	;

lvalueEnd: ASSIGN expression SEMI
	| USES IDENTIFIER SEMI
	;

/**
 * Las variables predefinidas tambien son lvalues ("players[0] uses X;"), pero
 * no son IDENTIFIER, asi que necesitan su propia produccion.
 */
globalStatement: globalName lvalueTail lvalueEnd
	;

globalName: CURRENT
	| PLAYERS
	| TURN_NUMBER
	| OPTION
	;

keywordDeclaration: keywordType IDENTIFIER initializerOpt SEMI
	| keywordType LBRACKET RBRACKET IDENTIFIER initializerOpt SEMI
	;

keywordType: INTEGER_T
	| BOOLEAN_T
	| STRING_T
	| PLAYER
	| BOARD
	| PIECE
	| DIE
	| DECK
	| STRATEGY
	;

initializerOpt: %empty
	| ASSIGN expression
	;

/**
 * Las llaves obligatorias eliminan el dangling-else sin precedencias: ELSE no
 * pertenece a FIRST(statement), por lo tanto tampoco a FOLLOW(ifStatement).
 */
ifStatement: IF LPAREN expression RPAREN block elseOpt
	;

elseOpt: %empty
	| ELSE block
	| ELSE ifStatement
	;

/** Acciones del dominio (§4.5). */

actionStatement: PLACE postfix ON expression SEMI
	| MOVE postfix FORWARD expression SEMI
	| SHUFFLE postfix SEMI
	| DRAW expression FROM postfix TO postfix SEMI
	| PLAY expression FROM postfix TO postfix SEMI
	| GIVE expression TO postfix SEMI
	| TAKE expression FROM postfix SEMI
	| LOG STRING logArgumentsOpt SEMI
	;

logArgumentsOpt: %empty
	| logArgumentsOpt COMMA expression
	;

/** Expresiones y agregaciones (§4.3). */

expression: expression ADD expression
	| expression SUB expression
	| expression MUL expression
	| expression DIV expression
	| expression MOD expression
	| expression LT expression
	| expression GT expression
	| expression LE expression
	| expression GE expression
	| expression EQ expression
	| expression NE expression
	| expression AND expression
	| expression OR expression
	| NOT expression
	| SUB expression %prec UMINUS
	| postfix
	;

postfix: primary
	| postfix DOT memberName
	| postfix LBRACKET expression RBRACKET
	| postfix LPAREN argumentListOpt RPAREN
	;

/**
 * Una keyword entra en memberName si y solo si la tabla de miembros del §4.1
 * define un miembro con ese nombre exacto: die.max, die.min, die.faces,
 * player.strategy, deck.cardtype. PLAYER no entra: no existe ".player".
 */
memberName: IDENTIFIER
	| MAX
	| MIN
	| STRATEGY
	| CARDTYPE
	| FACES
	;

argumentListOpt: %empty
	| argumentList
	;

argumentList: expression
	| argumentList COMMA expression
	;

primary: INTEGER
	| STRING
	| TRUE
	| FALSE
	| NONE
	| IDENTIFIER
	| CURRENT
	| PLAYERS
	| TURN_NUMBER
	| OPTION
	| LPAREN expression RPAREN
	| ROLL IDENTIFIER
	| ASK expression FOR IDENTIFIER LPAREN argumentListOpt RPAREN
	| aggregation
	;

aggregation: COUNT LPAREN IDENTIFIER IN expression whereOpt RPAREN
	| SELECT LPAREN IDENTIFIER IN expression whereOpt RPAREN
	| SUM LPAREN IDENTIFIER IN expression whereOpt BY expression RPAREN
	| MAX LPAREN IDENTIFIER IN expression whereOpt BY expression RPAREN
	| MIN LPAREN IDENTIFIER IN expression whereOpt BY expression RPAREN
	;

whereOpt: %empty
	| WHERE expression
	;

%%
