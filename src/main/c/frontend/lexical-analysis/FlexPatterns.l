%top{

#include "../../support/type/CompilationStatus.h"
#include "../../support/type/FlexContext.h"
#include "../../support/type/TokenLabel.h"
#include "../syntactic-analysis/AbstractSyntaxTree.h"
#include "../syntactic-analysis/BisonParser.h"

}

%{

/**
 * @see https://westes.github.io/flex/manual/Definitions-Section.html#Definitions-Section
 */

extern CompilationStatus EnterMultilineCommentLexemeAction(FlexContext context);
extern CompilationStatus EOFLexemeAction();
extern CompilationStatus IdentifierLexemeAction();
extern CompilationStatus IgnoredLexemeAction();
extern CompilationStatus IntegerLexemeAction();
extern CompilationStatus LeaveMultilineCommentLexemeAction();
extern CompilationStatus StringLexemeAction();
extern CompilationStatus TokenLexemeAction(TokenLabel label);
extern CompilationStatus UnknownLexemeAction();

%}

/**
 * Flex contexts (a.k.a. start conditions).
 *
 * @see https://westes.github.io/flex/manual/Start-Conditions.html
 */
%x MULTILINE_COMMENT

/**
 * Reusable patterns.
 *
 * @see https://westes.github.io/flex/manual/Matching.html
 * @see https://westes.github.io/flex/manual/Patterns.html
 */
whitespace			[[:space:]]+

%%

%{

/**
 * El alfabeto Σ de Meeple: 103 simbolos (77 palabras clave, 23 simbolos y 3
 * literales).
 *
 * Sobre el ORDEN de las reglas. Flex aplica *longest match* PRIMERO, y el
 * orden solo desempata entre matches del mismo largo. Por lo tanto:
 *
 *   - Poner "games" antes que "game" NO hace falta: son largos distintos.
 *   - Lo que SI importa es que toda palabra clave vaya antes que IDENTIFIER,
 *     porque ahi el empate es real ("game" y IDENTIFIER matchean 4 caracteres
 *     sobre la entrada "game") y gana la primera regla.
 *   - No hace falta un word boundary: sobre "gameplay", IDENTIFIER consume 8
 *     caracteres y "game" solo 4, asi que gana IDENTIFIER.
 *
 * Si alguna keyword queda tapada, flex avisa con "rule cannot be matched" y
 * flex.sh rompe el build (flex 2.6.4 no tiene -Werror y devuelve rc=0 solo).
 */

%}

	/* Comentarios: descartados por el analizador lexico (§4.1). */

"//"[^\n]*							{ return IgnoredLexemeAction(); }
"/*"								{ return EnterMultilineCommentLexemeAction(MULTILINE_COMMENT); }
<MULTILINE_COMMENT>"*/"				{ return LeaveMultilineCommentLexemeAction(); }
<MULTILINE_COMMENT>[^*]+			{ return IgnoredLexemeAction(); }
<MULTILINE_COMMENT>.				{ return IgnoredLexemeAction(); }

	/* Componentes del juego (§4.2). */

"games"								{ return TokenLexemeAction(GAMES); }
"game"								{ return TokenLexemeAction(GAME); }
"players"							{ return TokenLexemeAction(PLAYERS); }
"player"							{ return TokenLexemeAction(PLAYER); }
"board"								{ return TokenLexemeAction(BOARD); }
"cells"								{ return TokenLexemeAction(CELLS); }
"die"								{ return TokenLexemeAction(DIE); }
"faces"								{ return TokenLexemeAction(FACES); }
"to"								{ return TokenLexemeAction(TO); }
"piece"								{ return TokenLexemeAction(PIECE); }
"per"								{ return TokenLexemeAction(PER); }
"deck"								{ return TokenLexemeAction(DECK); }
"of"								{ return TokenLexemeAction(OF); }
"cardtype"							{ return TokenLexemeAction(CARDTYPE); }
"card"								{ return TokenLexemeAction(CARD); }
"setup"								{ return TokenLexemeAction(SETUP); }
"turn_number"						{ return TokenLexemeAction(TURN_NUMBER); }
"turns"								{ return TokenLexemeAction(TURNS); }
"turn"								{ return TokenLexemeAction(TURN); }
"order"								{ return TokenLexemeAction(ORDER); }
"clockwise"							{ return TokenLexemeAction(CLOCKWISE); }
"counterclockwise"					{ return TokenLexemeAction(COUNTERCLOCKWISE); }

	/* Decisiones y estrategias (§4.6). */

"decision"							{ return TokenLexemeAction(DECISION); }
"strategy"							{ return TokenLexemeAction(STRATEGY); }
"prefer"							{ return TokenLexemeAction(PREFER); }
"first"								{ return TokenLexemeAction(FIRST); }
"random"							{ return TokenLexemeAction(RANDOM); }
"ask"								{ return TokenLexemeAction(ASK); }
"for"								{ return TokenLexemeAction(FOR); }
"uses"								{ return TokenLexemeAction(USES); }

	/* Tipos clasicos (§4.1). */

"integer"							{ return TokenLexemeAction(INTEGER_T); }
"boolean"							{ return TokenLexemeAction(BOOLEAN_T); }
"string"							{ return TokenLexemeAction(STRING_T); }

	/* Agregaciones (§4.3). */

"max"								{ return TokenLexemeAction(MAX); }
"min"								{ return TokenLexemeAction(MIN); }
"sum"								{ return TokenLexemeAction(SUM); }
"count"								{ return TokenLexemeAction(COUNT); }
"select"							{ return TokenLexemeAction(SELECT); }
"where"								{ return TokenLexemeAction(WHERE); }
"by"								{ return TokenLexemeAction(BY); }
"in"								{ return TokenLexemeAction(IN); }

	/* Control de flujo (§4.4). */

"if"								{ return TokenLexemeAction(IF); }
"else"								{ return TokenLexemeAction(ELSE); }
"while"								{ return TokenLexemeAction(WHILE); }
"repeat"							{ return TokenLexemeAction(REPEAT); }

	/* Acciones del dominio (§4.5). */

"roll"								{ return TokenLexemeAction(ROLL); }
"place"								{ return TokenLexemeAction(PLACE); }
"on"								{ return TokenLexemeAction(ON); }
"move"								{ return TokenLexemeAction(MOVE); }
"forward"							{ return TokenLexemeAction(FORWARD); }
"shuffle"							{ return TokenLexemeAction(SHUFFLE); }
"draw"								{ return TokenLexemeAction(DRAW); }
"from"								{ return TokenLexemeAction(FROM); }
"play"								{ return TokenLexemeAction(PLAY); }
"give"								{ return TokenLexemeAction(GIVE); }
"take"								{ return TokenLexemeAction(TAKE); }
"log"								{ return TokenLexemeAction(LOG); }
"level"								{ return TokenLexemeAction(LEVEL); }
"verbose"							{ return TokenLexemeAction(VERBOSE); }

	/* Condiciones de fin (§4.7). */

"win"								{ return TokenLexemeAction(WIN); }
"when"								{ return TokenLexemeAction(WHEN); }
"end"								{ return TokenLexemeAction(END); }
"after"								{ return TokenLexemeAction(AFTER); }

	/* Simulacion y reportes (§4.8). */

"seed"								{ return TokenLexemeAction(SEED); }
"simulate"							{ return TokenLexemeAction(SIMULATE); }
"with"								{ return TokenLexemeAction(WITH); }
"report"							{ return TokenLexemeAction(REPORT); }
"winrate"							{ return TokenLexemeAction(WINRATE); }
"avg"								{ return TokenLexemeAction(AVG); }

	/* Variables predefinidas y literales del dominio (§4.1). */

"current"							{ return TokenLexemeAction(CURRENT); }
"option"							{ return TokenLexemeAction(OPTION); }
"none"								{ return TokenLexemeAction(NONE); }
"true"								{ return TokenLexemeAction(TRUE); }
"false"								{ return TokenLexemeAction(FALSE); }

	/* Operadores logicos (§4.3). */

"and"								{ return TokenLexemeAction(AND); }
"or"								{ return TokenLexemeAction(OR); }
"not"								{ return TokenLexemeAction(NOT); }

	/* Operadores de dos caracteres: los resuelve solo el longest match. */

"->"								{ return TokenLexemeAction(ARROW); }
"=="								{ return TokenLexemeAction(EQ); }
"!="								{ return TokenLexemeAction(NE); }
"<="								{ return TokenLexemeAction(LE); }
">="								{ return TokenLexemeAction(GE); }

	/* Operadores de un caracter. */

"<"									{ return TokenLexemeAction(LT); }
">"									{ return TokenLexemeAction(GT); }
"="									{ return TokenLexemeAction(ASSIGN); }
"+"									{ return TokenLexemeAction(ADD); }
"-"									{ return TokenLexemeAction(SUB); }
"*"									{ return TokenLexemeAction(MUL); }
"/"									{ return TokenLexemeAction(DIV); }
"%"									{ return TokenLexemeAction(MOD); }

	/* Puntuacion y agrupamiento. */

";"									{ return TokenLexemeAction(SEMI); }
","									{ return TokenLexemeAction(COMMA); }
":"									{ return TokenLexemeAction(COLON); }
"."									{ return TokenLexemeAction(DOT); }
"{"									{ return TokenLexemeAction(LBRACE); }
"}"									{ return TokenLexemeAction(RBRACE); }
"("									{ return TokenLexemeAction(LPAREN); }
")"									{ return TokenLexemeAction(RPAREN); }
"["									{ return TokenLexemeAction(LBRACKET); }
"]"									{ return TokenLexemeAction(RBRACKET); }

	/*
	 * Literales. El signo NO se lexea junto al numero: con "-?[0-9]+", la
	 * entrada "a - 5" dejaria de parsear. El menos se resuelve siempre en la
	 * gramatica (menos unario, o "signedInteger" en "die ... faces").
	 */

[[:digit:]]+						{ return IntegerLexemeAction(); }
\"[^\"]*\"							{ return TokenLexemeAction(STRING); }

	/* IDENTIFIER va al final de todo: es el unico que empata con las keywords. */

[[:alpha:]_][[:alnum:]_]*			{ return TokenLexemeAction(IDENTIFIER); }

{whitespace}						{ return IgnoredLexemeAction(); }
.									{ return UnknownLexemeAction(); }

<<EOF>>								{ return EOFLexemeAction(); }

%%

/**
 * @see https://westes.github.io/flex/manual/User-Code-Section.html#User-Code-Section
 */

#include "FlexActions.h"
#include "FlexExport.h"
