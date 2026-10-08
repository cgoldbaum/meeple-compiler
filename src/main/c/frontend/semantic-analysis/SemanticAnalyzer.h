#ifndef SEMANTIC_ANALYZER_HEADER
#define SEMANTIC_ANALYZER_HEADER

#include "../../support/logging/Logger.h"
#include "../../support/type/CompilationStatus.h"
#include "../../support/type/CompilerState.h"
#include "../../support/type/ModuleDestructor.h"
#include "../syntactic-analysis/AbstractSyntaxTree.h"
#include "SymbolTable.h"
#include "Type.h"
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/** Initialize module's internal state. */
ModuleDestructor initializeSemanticAnalyzerModule();

/**
 * Analisis semantico sobre el AST ya construido. Hace tres recorridos:
 *
 *   1. Los nombres de los game, para rechazar los repetidos.
 *   2. Cada game, en orden: declara cada item en la tabla de simbolos y chequea
 *      sus expresiones con lo declarado hasta ese punto (declarar antes de
 *      usar). Al cerrar el game chequea las strategy, que pueden resolver
 *      decisiones declaradas despues.
 *   3. Los simulate y los report, en orden.
 *
 * Al terminar, cada Expression tiene su tipo (semanticType) y cada nombre su
 * resolucion: es lo que lee el generador de codigo. La tabla de simbolos se
 * libera antes de volver.
 *
 * Reporta todos los errores que encuentra, no solo el primero, y devuelve
 * SUCCEEDED solo si no encontro ninguno. Una expresion mal tipada toma el tipo
 * error, que es compatible con todo, para no reportar errores en cascada.
 */
CompilationStatus executeSemanticAnalysis(CompilerState * compilerState);

#endif
