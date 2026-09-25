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

/* Las acciones de interpolacion reusan la accion generica, definida mas abajo. */
CompilationStatus StringLexemeAction(TokenLabel label);

/**
 * Logs a lexical-analyzer action over a token in DEBUGGING level.
 */
static void _logTokenAction(const char * actionName, Token * token) {
	char * _lexeme = escape(token->lexeme);
	logDebugging(_logger, WARNING_COLOR "%s" DEFAULT_COLOR ": Token(context=%d, label=%d, length=%d, lexeme=%s\"%s\"%s, line=%d, semanticValue=%p)",
		actionName,
		token->context,
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
		if (0 < context) {
			logError(_logger, "The final context is not closed (context=%d).", context);
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
 * Emite un STRING guardando el contenido *sin* las comillas. El lexema que
 * matcheo Flex es "\"...\"", de largo token->length, asi que el contenido
 * arranca en lexeme+1 y mide token->length-2.
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
 * del proyecto base (ArithmeticOperator, Parenthesis, ...), que con 103 tokens
 * no escala.
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
