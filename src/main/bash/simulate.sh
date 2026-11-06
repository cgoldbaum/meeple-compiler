#! /bin/bash

# Todo el camino para un programa Meeple: lo compila a C, compila ese C junto
# con el runtime y ejecuta las simulaciones. Los argumentos que siguen al
# programa se le pasan al ejecutable. Necesita el compilador ya construido
# (build.sh).
#
#   bash src/main/bash/simulate.sh programa.meeple
#
# El C generado, el runtime y el ejecutable quedan en .build/simulation/.

set -euo pipefail

BASE_PATH="$(dirname "$0")/../../.."
cd "$BASE_PATH"

RED='\033[0;31m'
OFF='\033[0m'

INPUT="$1"
shift 1
OUTPUT=".build/simulation"

rm --force --recursive "$OUTPUT"
mkdir --parents "$OUTPUT"

# El compilador escribe sus mensajes de depuracion por stdout y los errores por
# stderr: los primeros van a un archivo, los errores se ven en la consola.
if ! ".build/Flex-Bison-Compiler" "$OUTPUT/program.c" < "$INPUT" > "$OUTPUT/compiler.log"; then
	echo -e "${RED}El compilador rechazo \"$INPUT\".${OFF}" >&2
	exit 1
fi

cp "src/main/runtime/MeepleRuntime.h" "src/main/runtime/MeepleRuntime.c" "$OUTPUT/"

# El -std va explicito: gcc 15 compila con gnu23 por defecto. Las extensiones
# de GNU hacen falta por las agregaciones (statement expressions).
gcc -std=gnu99 -O2 -Wall -Wextra -o "$OUTPUT/program" "$OUTPUT/program.c" "$OUTPUT/MeepleRuntime.c"

exec "$OUTPUT/program" "$@"
