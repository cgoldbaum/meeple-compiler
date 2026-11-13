[![✗](https://github.com/cgoldbaum/meeple-compiler/actions/workflows/pipeline.yaml/badge.svg?branch=development)](https://github.com/cgoldbaum/meeple-compiler/actions/workflows/pipeline.yaml)

# Meeple

**Meeple** es un DSL para describir juegos de mesa por turnos y comparar estrategias de juego mediante simulación. Un programa declara los componentes de un juego (jugadores, tablero, dados, fichas, cartas), sus reglas, las decisiones que toman los jugadores y las estrategias con que las resuelven. El compilador lo traduce a un programa en C que corre miles de partidas e imprime un reporte de métricas.

El compilador está hecho con Flex y Bison sobre el proyecto base [Flex-Bison-Compiler](https://github.com/Alpha-Theta-Gamma-Mu/Flex-Bison-Compiler). Esta es la **entrega del Stage III**: frontend, análisis semántico, generación de código y *runtime*.

| Documento | Contenido |
| :-- | :-- |
| [doc/Especificacion-Meeple.pdf](doc/Especificacion-Meeple.pdf) | Especificación del lenguaje (Stage I). Las referencias con § apuntan a sus secciones |
| [doc/Informe-Meeple.pdf](doc/Informe-Meeple.pdf) | Informe final: diseño, implementación, dificultades y extensiones |

## Compilar y correr

Solo hace falta [Docker](https://www.docker.com/). Todos los comandos se ejecutan desde la raíz del repositorio y son iguales en Linux, macOS y Windows.

Construir el compilador y correr todos los tests:

```bash
docker compose run --rm compiler bash -c "bash src/main/bash/build.sh && bash src/main/bash/test.sh && bash src/main/bash/e2e.sh"
```

Simular un programa Meeple (con el compilador ya construido). El script lo compila a C, compila ese C junto con el *runtime* y ejecuta las simulaciones:

```bash
docker compose run --rm compiler bash src/main/bash/simulate.sh src/test/c/accept/21-ejemplo-la-oca
```

```text
simulate 1000 games of "La Oca" with 4 players seed 42
winrate by player
  Jugador 1: 32.00%
  Jugador 2: 27.20%
  Jugador 3: 22.20%
  Jugador 4: 18.60%
avg turns: 62.66
```

El C generado, una copia del *runtime* y el ejecutable quedan en `.build/simulation/`. Para generar solo el C, sin compilarlo:

```bash
docker compose run --rm compiler bash src/main/bash/run.sh programa.meeple salida.c
```

Si el programa es incorrecto, el compilador termina con código 1 y escribe cada error en la salida de error, con su línea:

```text
[ERROR][SemanticAnalyzer] Linea 8: draw entre mazos de cardtype distinto: deck of A y deck of B.
```

Por defecto el compilador no imprime nada si el programa es correcto. Con `-e LOGGING_LEVEL=ALL` (después de `run`) se ven los *tokens* y las reducciones.

## Tests

| Carpeta | Contenido | Script |
| :-- | :-- | :-- |
| `src/test/c/accept/` | 49 programas que el compilador acepta | `test.sh` (espera código 0) |
| `src/test/c/reject/` | 86 programas que el compilador rechaza: sintaxis y semántica | `test.sh` (espera código distinto de 0) |
| `src/test/c/runtime-error/` | 8 programas que compilan pero fallan al ejecutarse | `e2e.sh` |
| `src/test/c/output/` | Salida esperada de 19 casos (`.out`), entrada (`.stdin`) y error esperado (`.err`) | `e2e.sh` |

`e2e.sh` compila el C generado de `accept/` y `runtime-error/` con `-std=gnu99 -Wall -Wextra -Werror`, AddressSanitizer y UndefinedBehaviorSanitizer, y compara la salida de cada programa con su `output/NOMBRE[.CASO].out` (o el mensaje y el código 1 con su `.err`). El CI corre los tres scripts en cada *push*.

## Un ejemplo

Dos jugadores con dos fichas cada uno deciden en cada turno qué ficha mover (`accept/41-decision-sobre-fichas`):

```text
game "Dos fichas" {
    players 2 to 2;
    board cells 20;
    die D6 faces 1 to 6;
    piece a per player;
    piece b per player;
    decision mover: piece;
    strategy Rezagada { mover: prefer min option.cell; }
    strategy Puntera  { mover: prefer max option.cell, random; }
    prepare { players[0] uses Rezagada; players[1] uses Puntera; }
    turn {
        piece f = ask current for mover(current.pieces);
        move f forward roll D6;
    }
    win when current.a.cell == 19 and current.b.cell == 19;
    end after 200 turns;
}
simulate 1000 games of "Dos fichas" with 2 players seed 7;
report { winrate by strategy; avg turns; }
```

```text
simulate 1000 games of "Dos fichas" with 2 players seed 7
winrate by strategy
  Rezagada: 100.00%
  Puntera: 0.00%
avg turns: 22.59
```

Los ejemplos 6.1 y 6.2 de la especificación están en `accept/21-ejemplo-la-oca` y `accept/22-ejemplo-duelo-de-cartas`, con la sintaxis final. En el 6.2 nadie gana nunca: el `score` no pasa de 8 y la condición pide 15, por cómo está escrito el ejemplo.

## Semántica

Lo que la especificación no fijaba y cómo quedó:

* **Una partida.** Los jugadores se llaman `Jugador 1` a `Jugador K` (índices 0 a K−1) y empiezan con `score` 0 y sin estrategia. Las fichas empiezan en la celda 0, y los mazos con las cartas en el orden declarado, sin mezclar. Se ejecuta `prepare`. En cada turno `turns` aumenta en 1 (vale 1 en el primero), juega `current`, y si `win when` se cumple, `current` gana. Después el turno pasa al siguiente según `turn order` (`clockwise`, el valor por defecto, recorre los índices en orden ascendente).
* **Fin.** Sin `end after`, la partida termina sin ganador a los 1000 turnos. Un juego sin `turn` o sin `win when` es válido.
* **Semilla.** Sin `seed`, la semilla es 0. Con el mismo compilador de C, la misma semilla da la misma simulación en cualquier máquina: el generador es propio, no `rand()`.
* **Valores.** Jugadores, fichas, cartas y mazos son referencias: asignar copia la referencia y `==` compara identidad. Asignar un arreglo lo copia. Las cadenas se comparan por contenido. `none` vale solo para las referencias.
* **Acciones.** `move` nunca saca una ficha del tablero: se detiene en la primera o en la última celda. `draw` de un mazo con menos cartas que las pedidas mueve las que hay. `play` de una carta que no está en el origen no hace nada. El tope de un mazo es `Mazo[0]`.
* **`and` y `or`** cortocircuitan, como en C.
* **Decisiones.** `ask P for d(C)` toma la política de la estrategia de `P` para `d` (sin `uses`, `prefer first`), filtra las opciones con `where`, aplica los criterios en orden (cada uno desempata al anterior) y termina con `first`, `random` o `input`. Si no queda ninguna opción, devuelve `none` si la decisión es sobre una referencia, y es un error de ejecución si es sobre `integer`, `string` o `boolean`.
* **`prefer input`.** Muestra la partida, el turno, el jugador y las opciones numeradas desde 1, y lee la elección por la entrada estándar. Si la entrada no es válida, vuelve a preguntar.
* **`log` y `verbose`.** Un `log` evalúa siempre sus interpolaciones, pero solo imprime con `verbose`. Con `verbose` también se imprime el comienzo y el resultado de cada partida. El `report` es el mismo con y sin `verbose`.
* **Reporte.** `winrate` es el porcentaje de partidas con ganador. `winrate by strategy` divide las victorias de cada estrategia por la cantidad de jugadores que la usaron (con la estrategia que tenían al terminar la partida), así que la línea de base de una estrategia neutral es 1/K. `avg`, `min` y `max` se calculan sobre `turns` o sobre una `metric`, que se evalúa al terminar cada partida con `current` igual al último que jugó. Porcentajes y promedios van con dos decimales.

## Casos de rechazo de la especificación (§5.2)

| # | Caso | Test |
| :-: | :-- | :-- |
| 1 | Programa malformado | `reject/01`, `reject/02` |
| 2 | Ficha o jugador no declarado | `reject/31` |
| 3 | Rango de jugadores inválido | `reject/20` |
| 4 | Tipos incompatibles | `reject/32`, `reject/33` |
| 5 | Campo que el `cardtype` no declara | `reject/34` |
| 6 | `draw` entre mazos de `cardtype` distintos | `reject/35` |
| 7 | `log` con marcadores de más | Ya no aplica: el `log` usa interpolación, y sus errores equivalentes son de sintaxis (`reject/10`, `reject/11`) |
| 8 | `strategy` que no resuelve todas las decisiones | `reject/24`, `reject/29` |
| 9 | `ask` a una decisión no declarada | `reject/25`, `reject/82` |
| 10 | `simulate` de un juego inexistente o con una cantidad de jugadores que no admite | `reject/26` a `reject/28` |
| 11 | Juego sin `win when` | Ya no es un error (`accept/26`): la partida termina por la cota de turnos |

El resto de `reject/` son validaciones que agregamos; el nombre de cada archivo describe el caso.

## Errores de ejecución

Un error de ejecución imprime `Error de ejecucion en la linea N: <mensaje>.` por la salida de error y termina el programa con código 1, sin imprimir el reporte.

| Situación | Mensaje | Test |
| :-- | :-- | :-- |
| Acceso a un miembro de `none` | `acceso a un miembro de none` | `runtime-error/error-none` |
| `ask` a `none` | `ask a none` | `runtime-error/error-ask-none` |
| Índice fuera de rango | `el indice 2 esta fuera de rango: hay 2 elementos` | `runtime-error/error-indice` |
| División o módulo por cero | `division por cero`, `modulo por cero` | `runtime-error/error-division`, `error-modulo` |
| Celda fuera del tablero | `board.cell(5) esta fuera del tablero: las celdas van de 0 a 4` | `runtime-error/error-celda` |
| Decisión `integer`, `string` o `boolean` sin opciones | `la decision "d" no tiene opciones entre las que elegir` | `runtime-error/error-sin-opciones` |
| `max` o `min` de una colección vacía de valores | `max de una coleccion sin elementos` | `runtime-error/error-max-vacio` |
| `prefer input` sin entrada | `la decision "d" espera una opcion por la entrada estandar, y la entrada se termino` | `accept/46-prefer-input` |

## Cambios de sintaxis respecto de la especificación

Los pidió la devolución del Stage I:

| Tema | Especificación | Ahora | Tests |
| :-- | :-- | :-- | :-- |
| Jugadores | `players 2 to 4;` | También un conjunto no vacío: `players {2, 4, 6};` | `accept/25` |
| `for` | `for (p in players)` | También sobre un rango, con los extremos incluidos: `for (i in 1 to n)` | `accept/27` |
| Mazos | Lista de `card { campo: valor; }` | Producto cartesiano de generadores (`valor: 1 to 12; palo: {"Oro", "Copa"};`) y cartas posicionales (`card {1, 1}`) | `accept/23`, `accept/24` |
| Tablero | `board Tablero cells 63;` | `board cells 63;` y `board.cell(0)` | `accept/02` |
| Semilla y traza | `seed 42;` y `log level ...` sueltos | Cláusulas de `simulate`: `... with 4 players seed 42 verbose;` | `accept/29` |
| Nombres | `setup` y `turn_number` | `prepare` y `turns` | `accept/06` |
| `log` | `log "{1} avanzo {2}", current, pasos;` | Interpolación: `log "{current.name} avanzo {pasos}";` | `accept/15` |
| `decision` | `decision jugar(opciones: Carta[]) -> Carta;` | `decision jugar: Carta;` | `accept/16` |
| `prefer` | Un único criterio | Criterios encadenados con coma, filtro `where` y criterio `input`; `random`, `first` e `input` solo al final | `accept/30`, `accept/33` |
| Variables del juego | No existían | `integer cartasJugadas = 0;` dentro del `game`, visibles desde las estrategias | `accept/31` |
| `metric` y `report` | `winrate` y `avg turns` | `metric nombre = expr;` y `avg`/`min`/`max` sobre `turns` o una `metric` | `accept/32` |

`decision` y `strategy` siguen separados (punto 33 de la devolución); el informe explica por qué.

## Supuestos y limitaciones

1. `if`, `while` y `for` llevan paréntesis, y todo cuerpo va entre llaves.
2. No hay sentencias-expresión sueltas: `roll D6;` no es válido; `integer pasos = roll D6;` sí.
3. `place` existe (`place p.token on board.cell(0);`), aunque la tabla de §4.5 no lo lista.
4. Las variables del `game` no pueden ser `piece`, `die`, `deck` ni `strategy`, que tienen su propia declaración. Las variables locales tampoco pueden ser `deck` ni `die`.
5. Los campos de un `cardtype` son `integer`, `boolean` o `string`.
6. Una cadena no puede contener `"`, saltos de línea ni llaves: en un `log`, las llaves delimitan las interpolaciones.
7. No hay *shadowing*: una variable local no puede tener el nombre de algo visible.
8. No se detectan el desborde de enteros ni un `while` que no termina.
9. El C generado y el *runtime* ([src/main/runtime/](src/main/runtime/)) son C99 con extensiones de GNU (*statement expressions*, `__typeof__`), así que necesitan GCC o Clang. Solo usan la biblioteca estándar.
10. El orden de evaluación de los operandos es el del compilador de C: en `roll D - roll D` no está definido cuál se tira primero.
