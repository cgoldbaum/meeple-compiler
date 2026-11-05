#! /bin/bash

# Prueba el runtime sin el compilador. Los programas de src/test/runtime/ son
# lo que tiene que emitir el generador, escrito a mano: se compilan junto con
# el runtime, con los flags que va a usar el C generado mas ASan y UBSan, y se
# compara lo que imprimen.
#
# Cada caso es un archivo NOMBRE.out o NOMBRE.CASO.out con el stdout esperado
# del programa NOMBRE.c. Si existe NOMBRE[.CASO].stdin, se le pasa por la
# entrada estandar. Si existe NOMBRE[.CASO].err, se espera exactamente ese
# stderr y el codigo 1 (un error de ejecucion); si no, stderr vacio y codigo 0.
# Un stderr vacio tambien prueba que ASan y UBSan no encontraron nada.

set -u
shopt -s nullglob

BASE_PATH="$(dirname "$0")/../../.."
cd "$BASE_PATH"

GREEN='\033[0;32m'
OFF='\033[0m'
RED='\033[0;31m'
STATUS=0

RUNTIME="src/main/runtime"
TESTS="src/test/runtime"
OUTPUT=".build/runtime"
FLAGS=(-std=gnu99 -O1 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all)

rm --force --recursive "$OUTPUT"
mkdir --parents "$OUTPUT"

echo "Runtime should compile..."
echo ""

for program in "$TESTS"/*.c; do
	NAME="$(basename "$program" .c)"
	if gcc "${FLAGS[@]}" -I "$RUNTIME" "$program" "$RUNTIME/MeepleRuntime.c" -o "$OUTPUT/$NAME" 2> "$OUTPUT/$NAME.gcc"; then
		echo -e "    \"$NAME.c\", ${GREEN}and it does${OFF}"
	else
		STATUS=1
		echo -e "    \"$NAME.c\", ${RED}but it does not${OFF}"
		sed 's/^/        /' "$OUTPUT/$NAME.gcc"
	fi
	# "[.]" hace que el primero tambien sea un patron, y nullglob lo descarte si no existe.
	CASES=("$TESTS/$NAME"[.]out "$TESTS/$NAME".*.out)
	if [ "${#CASES[@]}" == "0" ]; then
		STATUS=1
		echo -e "        ${RED}no tiene ningun caso (\"$NAME.out\" o \"$NAME.CASO.out\")${OFF}"
	fi
done
echo ""

echo "Programs should print the expected output..."
echo ""

for expected in "$TESTS"/*.out; do
	CASE="$(basename "$expected" .out)"
	NAME="${CASE%%.*}"
	if [ ! -x "$OUTPUT/$NAME" ]; then
		STATUS=1
		echo -e "    \"$CASE\", ${RED}but \"$NAME.c\" does not compile${OFF}"
		continue
	fi

	INPUT="$TESTS/$CASE.stdin"
	if [ ! -f "$INPUT" ]; then
		INPUT="/dev/null"
	fi
	ERRORS="$TESTS/$CASE.err"
	EXPECTED_RESULT=0
	if [ -f "$ERRORS" ]; then
		EXPECTED_RESULT=1
	fi

	timeout 60 "$OUTPUT/$NAME" < "$INPUT" > "$OUTPUT/$CASE.stdout" 2> "$OUTPUT/$CASE.stderr"
	RESULT="$?"

	PROBLEMS=()
	if [ "$RESULT" != "$EXPECTED_RESULT" ]; then
		PROBLEMS+=("status $RESULT, expected $EXPECTED_RESULT")
	fi
	if ! cmp --silent "$expected" "$OUTPUT/$CASE.stdout"; then
		PROBLEMS+=("stdout differs")
	fi
	if [ -f "$ERRORS" ]; then
		if ! cmp --silent "$ERRORS" "$OUTPUT/$CASE.stderr"; then
			PROBLEMS+=("stderr differs")
		fi
	elif [ -s "$OUTPUT/$CASE.stderr" ]; then
		PROBLEMS+=("stderr is not empty")
	fi

	if [ "${#PROBLEMS[@]}" == "0" ]; then
		echo -e "    \"$CASE\", ${GREEN}and it does${OFF} (status $RESULT)"
	else
		STATUS=1
		echo -e "    \"$CASE\", ${RED}but it does not${OFF}: $(printf '%s\n' "${PROBLEMS[@]}" | paste --serial --delimiters=';' | sed 's/;/, /g')"
		diff --unified "$expected" "$OUTPUT/$CASE.stdout" | head --lines=40 | sed 's/^/        /'
		if [ -f "$ERRORS" ]; then
			diff --unified "$ERRORS" "$OUTPUT/$CASE.stderr" | head --lines=20 | sed 's/^/        /'
		else
			head --lines=20 "$OUTPUT/$CASE.stderr" | sed 's/^/        /'
		fi
	fi
done
echo ""

echo "All done."
exit $STATUS
