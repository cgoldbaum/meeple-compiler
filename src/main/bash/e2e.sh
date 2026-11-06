#! /bin/bash

# Pruebas de punta a punta: el compilador, el C que genera y el runtime.
#
#   1. Todo programa de src/test/c/accept/ y de src/test/c/runtime-error/ se
#      compila a C, y ese C compila con -Wall -Wextra -Werror, ASan y UBSan.
#   2. Cada src/test/c/output/NOMBRE[.CASO].out es el stdout esperado del
#      programa NOMBRE (de accept/ o de runtime-error/). Si existe
#      NOMBRE[.CASO].stdin, se le pasa por la entrada estandar. Si existe
#      NOMBRE[.CASO].err, se espera exactamente ese stderr y el codigo 1 (un
#      error de ejecucion); si no, stderr vacio y codigo 0. Un stderr vacio
#      tambien prueba que ASan y UBSan no encontraron nada.
#   3. Todo programa de runtime-error/ tiene al menos un caso con .err.
#
# Necesita el compilador ya construido (build.sh).

set -u
shopt -s globstar nullglob

BASE_PATH="$(dirname "$0")/../../.."
cd "$BASE_PATH"

GREEN='\033[0;32m'
OFF='\033[0m'
RED='\033[0;31m'
STATUS=0

COMPILER=".build/Flex-Bison-Compiler"
RUNTIME="src/main/runtime"
TESTS="src/test/c"
EXPECTED="$TESTS/output"
OUTPUT=".build/e2e"
FLAGS=(-std=gnu99 -O1 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all)

rm --force --recursive "$OUTPUT"
mkdir --parents "$OUTPUT"

if ! gcc "${FLAGS[@]}" -c "$RUNTIME/MeepleRuntime.c" -o "$OUTPUT/MeepleRuntime.o"; then
	echo -e "${RED}The runtime does not compile.${OFF}"
	exit 1
fi

echo "Generated programs should compile..."
echo ""

for path in "$TESTS"/accept/**/* "$TESTS"/runtime-error/**/*; do
	if [ ! -f "$path" ]; then
		continue
	fi
	NAME="$(basename "$path")"
	TEST="${path#"$TESTS"/}"
	if ! "$COMPILER" "$OUTPUT/$NAME.c" < "$path" > "$OUTPUT/$NAME.compiler" 2>&1; then
		STATUS=1
		echo -e "    \"$TEST\", ${RED}but the compiler rejects it${OFF}"
		grep --max-count=10 'ERROR' "$OUTPUT/$NAME.compiler" | sed 's/^/        /'
		continue
	fi
	if gcc "${FLAGS[@]}" -I "$RUNTIME" "$OUTPUT/$NAME.c" "$OUTPUT/MeepleRuntime.o" -o "$OUTPUT/$NAME" 2> "$OUTPUT/$NAME.gcc"; then
		echo -e "    \"$TEST\", ${GREEN}and it does${OFF}"
	else
		STATUS=1
		echo -e "    \"$TEST\", ${RED}but it does not${OFF}"
		head --lines=20 "$OUTPUT/$NAME.gcc" | sed 's/^/        /'
	fi
done
echo ""

echo "Runtime-error programs should have an expected error..."
echo ""

for path in "$TESTS"/runtime-error/**/*; do
	if [ ! -f "$path" ]; then
		continue
	fi
	NAME="$(basename "$path")"
	ERRORS=("$EXPECTED/$NAME"[.]err "$EXPECTED/$NAME".*.err)
	if [ "${#ERRORS[@]}" == "0" ]; then
		STATUS=1
		echo -e "    \"$NAME\", ${RED}but it has no \"$NAME.err\" in output/${OFF}"
	else
		echo -e "    \"$NAME\", ${GREEN}and it does${OFF}"
	fi
done
echo ""

echo "Programs should print the expected output..."
echo ""

for expected in "$EXPECTED"/*.out; do
	CASE="$(basename "$expected" .out)"
	NAME="${CASE%%.*}"
	if [ ! -x "$OUTPUT/$NAME" ]; then
		STATUS=1
		echo -e "    \"$CASE\", ${RED}but there is no compiled program \"$NAME\"${OFF}"
		continue
	fi

	INPUT="$EXPECTED/$CASE.stdin"
	if [ ! -f "$INPUT" ]; then
		INPUT="/dev/null"
	fi
	ERRORS="$EXPECTED/$CASE.err"
	EXPECTED_RESULT=0
	if [ -f "$ERRORS" ]; then
		EXPECTED_RESULT=1
	fi

	timeout 120 "$OUTPUT/$NAME" < "$INPUT" > "$OUTPUT/$CASE.stdout" 2> "$OUTPUT/$CASE.stderr"
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
