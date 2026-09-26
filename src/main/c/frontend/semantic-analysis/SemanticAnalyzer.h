#ifndef SEMANTIC_ANALYZER_HEADER
#define SEMANTIC_ANALYZER_HEADER

#include "../../support/logging/Logger.h"
#include "../../support/type/CompilationStatus.h"
#include "../../support/type/CompilerState.h"
#include "../../support/type/ModuleDestructor.h"
#include "../syntactic-analysis/AbstractSyntaxTree.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/** Initialize module's internal state. */
ModuleDestructor initializeSemanticAnalyzerModule();

/**
 * Recorre el AST ya construido y aplica los chequeos semanticos de los casos
 * de rechazo 8, 9 y 10 del PDF (§5.2):
 *
 *   8. Toda "strategy" resuelve todos los "decision" declarados en su game.
 *   9. Todo "ask" nombra un "decision" declarado en su game.
 *  10. Todo "simulate" nombra un game declarado, con una cantidad de jugadores
 *      que ese game admite.
 *
 * Ninguno de los tres se puede chequear en una accion de Bison: los tres miran
 * declaraciones que pueden aparecer *despues* del uso (una strategy se reduce
 * antes de saber si mas abajo hay otro decision, y un simulate puede preceder
 * al game que nombra), asi que necesitan el AST completo.
 *
 * Reporta todos los errores que encuentra, no solo el primero, y devuelve
 * SUCCEEDED solo si no encontro ninguno.
 */
CompilationStatus executeSemanticAnalysis(CompilerState * compilerState);

#endif
