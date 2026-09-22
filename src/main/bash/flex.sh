#! /bin/bash

set -euxo pipefail

BASE_PATH="$(dirname "$0")/../../.."
cd "$BASE_PATH"

GREEN='\033[0;32m'
RED='\033[0;31m'
OFF='\033[0m'

# Flex 2.6.4 no tiene "-Werror" (solo "-w/--nowarn") y devuelve rc=0 aunque
# emita warnings. El mas peligroso es "rule cannot be matched", que delata una
# palabra clave tapada por IDENTIFIER: el build pasa y el lenguaje no parsea.
# Por eso se captura stderr y se aborta si no quedo vacio.
#
# El "|| FLEX_STATUS=$?" es necesario: con "set -e", si flex falla el script
# terminaria ahi mismo y el error, que quedo en el archivo, nunca se mostraria.
FLEX_ERR="$(mktemp)"
trap 'rm -f "$FLEX_ERR"' EXIT

FLEX_STATUS=0
flex \
	--bison-bridge \
	--bison-locations \
	--header-file="src/main/c/frontend/lexical-analysis/FlexScanner.h" \
	--never-interactive \
	--nounistd \
	--noyywrap \
	--outfile="src/main/c/frontend/lexical-analysis/FlexScanner.c" \
	--reentrant \
	--stack \
	--yylineno \
	"src/main/c/frontend/lexical-analysis/FlexPatterns.l" 2>"$FLEX_ERR" || FLEX_STATUS=$?

cat "$FLEX_ERR" >&2
if [ "$FLEX_STATUS" -ne 0 ]; then
	echo -e "${RED}Flex fallo (rc=$FLEX_STATUS); se aborta el build.${OFF}" >&2
	exit "$FLEX_STATUS"
fi
if [ -s "$FLEX_ERR" ]; then
	echo -e "${RED}Flex emitio warnings; se aborta el build.${OFF}" >&2
	exit 1
fi

echo -e "${GREEN}Flex done.${OFF}"
