#ifndef GENERATOR_HEADER
#define GENERATOR_HEADER

#include "../../frontend/semantic-analysis/Type.h"
#include "../../frontend/syntactic-analysis/AbstractSyntaxTree.h"
#include "../../support/language/String.h"
#include "../../support/logging/Logger.h"
#include "../../support/type/CompilationStatus.h"
#include "../../support/type/CompilerState.h"
#include "../../support/type/ModuleDestructor.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Initialize module's internal state. */
ModuleDestructor initializeGeneratorModule();

/**
 * Genera el programa en C a partir del AST anotado por el analisis semantico,
 * y lo escribe en compilerState->outputPath. Sin ruta, lo genera igual y lo
 * descarta. El C generado se compila junto con el runtime (MeepleRuntime.c).
 */
CompilationStatus executeGenerator(CompilerState * compilerState);

#endif
