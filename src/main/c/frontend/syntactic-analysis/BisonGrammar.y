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

/**
 * Partes de una cadena interpolada del log (§13.7 del plan): "texto{",
 * "}texto{" y "}texto". Mismo criterio que STRING: sin tipo hasta el AST.
 */
%token STRING_HEAD STRING_MIDDLE STRING_TAIL

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

topLevel: gameDeclaration
	| simulation
	| REPORT LBRACE reportItemList RBRACE
	;

/**
 * La semilla y la verbosidad son clausulas de simulate (§13.4 del plan): ya no
 * existen "seed n;" ni "log level ...;" sueltos. El orden es fijo.
 */
simulation: SIMULATE INTEGER gameWord OF STRING WITH INTEGER PLAYERS seedOpt verboseOpt SEMI
	;

gameWord: GAME
	| GAMES
	;

seedOpt: %empty
	| SEED INTEGER
	;

verboseOpt: %empty
	| VERBOSE
	;

reportItemList: reportItem
	| reportItemList reportItem
	;

/**
 * Lo que se puede reportar (§13.10 del plan): las tres formas de winrate, y un
 * agregador sobre una metrica. "turns" es la metrica predefinida; el resto son
 * los "metric" declarados en el juego.
 */
reportItem: WINRATE SEMI
	| WINRATE BY PLAYER SEMI
	| WINRATE BY STRATEGY SEMI
	| reportAggregator reportMetric SEMI
	;

reportAggregator: AVG
	| MIN
	| MAX
	;

reportMetric: TURNS
	| IDENTIFIER
	;

/** Componentes del juego (§4.2). */

gameDeclaration: GAME STRING LBRACE gameItemList RBRACE
	;

gameItemList: gameItem
	| gameItemList gameItem
	;

gameItem: PLAYERS playerCount SEMI
	| BOARD CELLS INTEGER SEMI
	| DIE IDENTIFIER FACES integerSet SEMI
	| PIECE IDENTIFIER perPlayerOpt SEMI
	| CARDTYPE IDENTIFIER LBRACE fieldList RBRACE
	| DECK IDENTIFIER OF IDENTIFIER perPlayerOpt SEMI
	| DECK IDENTIFIER OF IDENTIFIER LBRACE deckBody RBRACE
	| gameVariable
	| METRIC IDENTIFIER ASSIGN expression SEMI
	| DECISION IDENTIFIER COLON typeSpec SEMI
	| STRATEGY IDENTIFIER LBRACE policyList RBRACE
	| PREPARE block
	| TURN block
	| TURN ORDER turnOrderValue SEMI
	| WIN WHEN expression SEMI
	| END AFTER INTEGER TURNS SEMI
	;

/**
 * Variables del juego (§13.9 del plan). Los tipos se limitan a los que no
 * abren otra declaracion del juego: con "piece", "die" o "deck", la entrada
 * "piece p;" seria a la vez una variable y una ficha (conflicto R/R).
 */
gameVariable: gameVariableType IDENTIFIER initializerOpt SEMI
	| gameVariableType LBRACKET RBRACKET IDENTIFIER initializerOpt SEMI
	| IDENTIFIER IDENTIFIER initializerOpt SEMI
	| IDENTIFIER LBRACKET RBRACKET IDENTIFIER initializerOpt SEMI
	;

gameVariableType: INTEGER_T
	| BOOLEAN_T
	| STRING_T
	| PLAYER
	;

turnOrderValue: CLOCKWISE
	| COUNTERCLOCKWISE
	;

perPlayerOpt: %empty
	| PER PLAYER
	;

/**
 * Conjuntos de valores (§13.1 del plan): "A to B" abrevia {A, A+1, ..., B}, y
 * donde se acepta un rango tambien se acepta un conjunto. Nunca vacio.
 *
 * Los negativos solo se admiten en "die ... faces" y en los generadores de
 * mazo. En el resto de las declaraciones se exige INTEGER pelado, lo que
 * regala dos rechazos: "seed -1" y "cells -5".
 */
playerCount: INTEGER TO INTEGER
	| LBRACE naturalList RBRACE
	;

naturalList: INTEGER
	| naturalList COMMA INTEGER
	;

integerSet: signedInteger TO signedInteger
	| LBRACE integerList RBRACE
	;

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

/**
 * El cuerpo de un mazo es una lista de cartas O una lista de generadores, sin
 * mezclar (§13.2 del plan). Los generadores arman el producto cartesiano de
 * sus conjuntos de valores.
 */
deckBody: cardList
	| generatorList
	;

cardList: card
	| cardList card
	;

/**
 * Carta con nombres ("card { ataque: 1; }") o posicional ("card {1, 1}"). LALR
 * decide con el token que sigue al primer IDENTIFIER: ":" abre la forma con
 * nombres.
 */
card: CARD LBRACE cardFieldList RBRACE
	| CARD LBRACE argumentList RBRACE
	;

cardFieldList: cardField
	| cardFieldList cardField
	;

cardField: IDENTIFIER COLON expression SEMI
	;

generatorList: generator
	| generatorList generator
	;

generator: IDENTIFIER COLON valueSet SEMI
	;

valueSet: signedInteger TO signedInteger
	| LBRACE literalList RBRACE
	;

literalList: literal
	| literalList COMMA literal
	;

literal: signedInteger
	| STRING
	| TRUE
	| FALSE
	;

/** Decisiones y estrategias (§4.6). */

policyList: policy
	| policyList policy
	;

/**
 * Politicas (§13.8 del plan): criterios encadenados con coma, donde cada uno
 * desempata al anterior, y un filtro "where" opcional. Los criterios que no
 * ordenan (random, first, input) solo pueden ir al final: despues de ellos no
 * queda nada que desempatar. Lo garantiza la gramatica.
 */
policy: IDENTIFIER COLON PREFER criteria whereOpt SEMI
	;

criteria: rankingList
	| rankingList COMMA finalCriterion
	| finalCriterion
	;

rankingList: ranking
	| rankingList COMMA ranking
	;

ranking: MAX expression
	| MIN expression
	;

finalCriterion: RANDOM
	| FIRST
	| INPUT
	;

typeSpec: baseType
	| baseType LBRACKET RBRACKET
	;

baseType: INTEGER_T
	| BOOLEAN_T
	| STRING_T
	| PLAYER
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
	| FOR LPAREN IDENTIFIER IN expression TO expression RPAREN block
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
	| OPTION
	;

keywordDeclaration: keywordType IDENTIFIER initializerOpt SEMI
	| keywordType LBRACKET RBRACKET IDENTIFIER initializerOpt SEMI
	;

keywordType: INTEGER_T
	| BOOLEAN_T
	| STRING_T
	| PLAYER
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
	| LOG interpolatedString SEMI
	;

/**
 * Cadena del log, con interpolaciones "{expr}" (§13.7 del plan). El lexer ya
 * la parte en STRING_HEAD, STRING_MIDDLE y STRING_TAIL. Una interpolacion
 * vacia ("{}") o sin cerrar es un error de sintaxis.
 */
interpolatedString: STRING
	| STRING_HEAD expression interpolationRest
	;

interpolationRest: STRING_TAIL
	| STRING_MIDDLE expression interpolationRest
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
	| TURNS
	| OPTION
	| BOARD
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
