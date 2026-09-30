#include "FlexActions.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

/* MODULE INTERNAL STATE */

static bool _logIgnoredLexemes = true;
static LexicalAnalyzer * _lexicalAnalyzer = NULL;
static Logger * _logger = NULL;

/** Shutdown module's internal state. */
void _shutdownFlexActionsModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: FlexActions...");
		destroyLogger(_logger);
		_logger = NULL;
	}
	_lexicalAnalyzer = NULL;
}

ModuleDestructor initializeFlexActionsModule(LexicalAnalyzer * lexicalAnalyzer) {
	_lexicalAnalyzer = lexicalAnalyzer;
	_logger = createLogger("FlexActions");
	_logIgnoredLexemes = getBooleanOrDefault("LOG_IGNORED_LEXEMES", _logIgnoredLexemes);
	return _shutdownFlexActionsModule;
}

/* PRIVATE FUNCTIONS */

static void _logTokenAction(const char * actionName, Token * token);
static const char * _toContextString(const FlexContext context);

/*
 * Enter/LeaveInterpolationLexemeAction reusan StringLexemeAction, que esta
 * definida mas abajo y no se declara en FlexActions.h (FlexPatterns.l la
 * declara extern).
 */
CompilationStatus StringLexemeAction(TokenLabel label);

/**
 * Get the context string of the specified Flex context. Los numeros salen del
 * orden de declaracion de los contextos en FlexPatterns.l.
 */
static const char * _toContextString(const FlexContext context) {
	switch (context) {
		case 0: return "INITIAL";
		case 1: return "MULTILINE_COMMENT";
		case 2: return "INTERPOLATION";
		default:
			logError(_logger, "The specified Flex context is unknown: %d", context);
			return "<UNKNOWN CONTEXT>";
	}
}

/**
 * Logs a lexical-analyzer action over a token in DEBUGGING level.
 */
static void _logTokenAction(const char * actionName, Token * token) {
	char * _lexeme = escape(token->lexeme);
	logDebugging(_logger, WARNING_COLOR "%s" DEFAULT_COLOR ": Token(context=%s, label=%d, length=%d, lexeme=%s\"%s\"%s, line=%d, semanticValue=%p)",
		actionName,
		_toContextString(token->context),
		token->label,
		token->length,
		INFORMATION_COLOR, _lexeme, DEFAULT_COLOR,
		token->line,
		token->semanticValue);
	free(_lexeme);
	_lexeme = NULL;
}

/* PUBLIC FUNCTIONS */

/**
 * Emite el STRING_HEAD de una cadena interpolada y entra al contexto de la
 * interpolacion, donde se lexea la expresion que va entre llaves.
 */
CompilationStatus EnterInterpolationLexemeAction(TokenLabel label, FlexContext context) {
	CompilationStatus status = StringLexemeAction(label);
	enterLexicalAnalyzerContext(_lexicalAnalyzer, context);
	return status;
}

CompilationStatus EnterMultilineCommentLexemeAction(FlexContext context) {
	if (_logIgnoredLexemes) {
		Token * token = createToken(_lexicalAnalyzer, OPEN_COMMENT);
		_logTokenAction(__FUNCTION__, token);
		destroyToken(token);
	}
	enterLexicalAnalyzerContext(_lexicalAnalyzer, context);
	return IN_PROGRESS;
}

CompilationStatus EOFLexemeAction() {
	CompilationStatus status = IN_PROGRESS;
	Token * token = createToken(_lexicalAnalyzer, 0);
	_logTokenAction(__FUNCTION__, token);
	if (!popInputBuffer(_lexicalAnalyzer)) {
		status = pushToken(_lexicalAnalyzer, token);
		FlexContext context = currentLexicalAnalyzerContext(_lexicalAnalyzer);
		if (0 != context) {
			logError(_logger, "The final context is not closed (context=%s).", _toContextString(context));
			status = FAILED;
		}
	}
	destroyToken(token);
	return status;
}

/**
 * Emite un IDENTIFIER. El lexema se copia con strdup y el puntero pasa a ser
 * propiedad del parser (ver el %destructor de <string> en BisonGrammar.y).
 */
CompilationStatus IdentifierLexemeAction() {
	Token * token = createToken(_lexicalAnalyzer, IDENTIFIER);
	token->semanticValue->string = strdup(token->lexeme);
	if (token->semanticValue->string == NULL) {
		destroyToken(token);
		return OUT_OF_MEMORY;
	}
	_logTokenAction(__FUNCTION__, token);
	CompilationStatus status = pushToken(_lexicalAnalyzer, token);
	destroyToken(token);
	return status;
}

CompilationStatus IgnoredLexemeAction() {
	if (_logIgnoredLexemes) {
		Token * token = createToken(_lexicalAnalyzer, IGNORED);
		_logTokenAction(__FUNCTION__, token);
		destroyToken(token);
	}
	return IN_PROGRESS;
}

CompilationStatus IntegerLexemeAction() {
	Token * token = createToken(_lexicalAnalyzer, INTEGER);
	errno = 0;
	long value = strtol(token->lexeme, NULL, 10);
	if (errno == ERANGE || value > INT_MAX) {
		logError(_logger, "Integer literal is out of range: \"%s\".", token->lexeme);
		/*
		 * Se empuja como UNKNOWN para que el parser aborte y libere su pila con
		 * los %destructor. Si solo se devolviera FAILED, lo que el parser ya
		 * construyo se perderia: yypstate_delete no corre los destructores.
		 */
		token->label = UNKNOWN;
		pushToken(_lexicalAnalyzer, token);
		destroyToken(token);
		return FAILED;
	}
	token->semanticValue->integer = (int) value;
	_logTokenAction(__FUNCTION__, token);
	CompilationStatus status = pushToken(_lexicalAnalyzer, token);
	destroyToken(token);
	return status;
}

/**
 * Emite el STRING_TAIL de una cadena interpolada y vuelve al contexto en el que
 * se abrio la cadena.
 */
CompilationStatus LeaveInterpolationLexemeAction(TokenLabel label) {
	leaveLexicalAnalyzerContext(_lexicalAnalyzer);
	return StringLexemeAction(label);
}

CompilationStatus LeaveMultilineCommentLexemeAction() {
	leaveLexicalAnalyzerContext(_lexicalAnalyzer);
	if (_logIgnoredLexemes) {
		Token * token = createToken(_lexicalAnalyzer, CLOSE_COMMENT);
		_logTokenAction(__FUNCTION__, token);
		destroyToken(token);
	}
	return IN_PROGRESS;
}

/**
 * Emite un STRING, STRING_HEAD, STRING_MIDDLE o STRING_TAIL guardando el texto
 * *sin* sus delimitadores. En los cuatro casos el lexema que matcheo Flex tiene
 * exactamente un caracter delimitador de cada lado:
 *
 *   STRING         "texto"
 *   STRING_HEAD    "texto{
 *   STRING_MIDDLE  }texto{
 *   STRING_TAIL    }texto"
 *
 * asi que el contenido arranca en lexeme+1 y mide token->length-2.
 */
CompilationStatus StringLexemeAction(TokenLabel label) {
	Token * token = createToken(_lexicalAnalyzer, label);
	token->semanticValue->string = strndup(token->lexeme + 1, token->length - 2);
	if (token->semanticValue->string == NULL) {
		destroyToken(token);
		return OUT_OF_MEMORY;
	}
	_logTokenAction(__FUNCTION__, token);
	CompilationStatus status = pushToken(_lexicalAnalyzer, token);
	destroyToken(token);
	return status;
}

/**
 * Accion generica para toda palabra clave y todo simbolo, es decir, para los
 * tokens que no acarrean valor semantico. Reemplaza a la familia de funciones
 * del proyecto base (ArithmeticOperator, Parenthesis, ...), que con un
 * centenar de tokens no escala.
 */
CompilationStatus TokenLexemeAction(TokenLabel label) {
	Token * token = createToken(_lexicalAnalyzer, label);
	_logTokenAction(__FUNCTION__, token);
	CompilationStatus status = pushToken(_lexicalAnalyzer, token);
	destroyToken(token);
	return status;
}

CompilationStatus UnknownLexemeAction() {
	Token * token = createToken(_lexicalAnalyzer, UNKNOWN);
	_logTokenAction(__FUNCTION__, token);
	pushToken(_lexicalAnalyzer, token);
	destroyToken(token);
	return FAILED;
}
