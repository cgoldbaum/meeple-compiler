[![✗](https://github.com/cgoldbaum/meeple-compiler/actions/workflows/pipeline.yaml/badge.svg?branch=development)](https://github.com/cgoldbaum/meeple-compiler/actions/workflows/pipeline.yaml)

# Meeple

**Meeple** es un DSL para diseñar, simular y balancear juegos de mesa por turnos. El compilador está desarrollado con Flex y Bison sobre el proyecto base [Flex-Bison-Compiler](https://github.com/Alpha-Theta-Gamma-Mu/Flex-Bison-Compiler) (rama `development`).

**Entrega del Stage II (frontend).** El compilador lexea y parsea programas Meeple, construye el AST, corre una pasada semántica acotada (casos de rechazo 8, 9 y 10) y lo imprime por consola.

La **especificación** es el documento del Stage I, [doc/Especificacion-Meeple.pdf](doc/Especificacion-Meeple.pdf); las referencias con § apuntan a sus secciones. La **devolución** es la corrección de ese documento que hizo la cátedra.

## Compilar y correr

Solo hace falta [Docker](https://www.docker.com/). Desde la raíz del repositorio, para compilar y correr todos los tests:

```bash
docker compose run --rm compiler bash -c "bash src/main/bash/build.sh && bash src/main/bash/test.sh"
```

Para un solo programa (con el build ya hecho), mostrando solo el AST y los errores:

```bash
docker compose run --rm -e LOGGING_LEVEL=ERROR compiler bash src/main/bash/run.sh src/test/c/accept/21-ejemplo-la-oca
```

Sin `-e LOGGING_LEVEL=ERROR` se ven también los logs de depuración.

| Carpeta de tests | Contenido |
| :-- | :-- |
| `src/test/c/accept/` | Programas que se deben aceptar (34) |
| `src/test/c/reject/` | Programas que se deben rechazar (29) |
| `src/test/c/pending/` | Casos de rechazo que recién detecta el Stage III (4). `test.sh` los lista, pero no los corre |

## Gramática

Σ (105 símbolos) está definido en [FlexPatterns.l](src/main/c/frontend/lexical-analysis/FlexPatterns.l). N (67 no terminales), Π (220 producciones) y S = `program` están en [BisonGrammar.y](src/main/c/frontend/syntactic-analysis/BisonGrammar.y). La gramática es LALR(1) y no tiene conflictos: `bison.sh` corre con `-Werror=conflicts-sr` y `-Werror=conflicts-rr`.

## Cambios de sintaxis respecto de la especificación

Estos cambios los pidió la devolución del Stage I:

| Tema | Especificación | Ahora | Tests |
| :-- | :-- | :-- | :-- |
| Jugadores | `players 2 to 4;` | También un conjunto no vacío: `players {2, 4, 6};` | `accept/25` |
| `for` | `for (p in players)` | También sobre un rango: `for (i in 1 to n)` | `accept/27` |
| Mazos | Lista de `card { campo: valor; }` | Generadores por producto cartesiano (`valor: 1 to 12; palo: {"Oro", "Copa"};`) y cartas posicionales (`card {1, 1}`) | `accept/23`, `accept/24` |
| Tablero | `board Tablero cells 63;` | `board cells 63;` y `board.cell(0)` | `accept/02` |
| Semilla y traza | `seed 42;` y `log level ...` sueltos | Cláusulas de `simulate`: `... with 4 players seed 42 verbose;` | `accept/29` |
| Nombres | `setup` y `turn_number` | `prepare` y `turns` | `accept/06` |
| `log` | `log "{1} avanzo {2}", current, pasos;` | Interpolación: `log "{current.name} avanzo {pasos}";` | `accept/15` |
| `decision` | `decision jugar(opciones: Carta[]) -> Carta;` | `decision jugar: Carta;` | `accept/16` |
| `prefer` | Una política | Criterios encadenados con coma, filtro `where` y `input`; `random`, `first` e `input` solo al final | `accept/30`, `accept/33` |
| Variables del juego | No existían | `integer cartasJugadas = 0;` dentro del `game` | `accept/31` |
| `metric` y `report` | `winrate` y `avg turns` | `metric nombre = expr;` y `avg`/`min`/`max` sobre `turns` o una `metric` | `accept/32` |

`decision` y `strategy` siguen separados (punto 33 de la devolución). `decision` es el contrato que comparten todas las estrategias: fija una sola vez el tipo de `option` y permite detectar al compilar los casos de rechazo 8 y 9.

## Casos de rechazo de la especificación (§5.2)

| # | Caso | Lo detecta | Test |
| :-: | :-- | :-- | :-- |
| 1 | Programa malformado | Sintaxis | `reject/01`, `reject/02` |
| 2 | Ficha o jugador no declarado | Stage III (tabla de símbolos) | `pending/02` |
| 3 | Rango de jugadores inválido | Acción de Bison | `reject/20` |
| 4 | Tipos incompatibles | Stage III (tipos) | `pending/04` |
| 5 | Campo que el `cardtype` no declara | Stage III (tabla de símbolos) | `pending/05` |
| 6 | `draw` entre `cardtype` distintos | Stage III (tipos) | `pending/06` |
| 7 | `log` con marcadores de más | Ya no existe: sus equivalentes (interpolación vacía o sin cerrar) son de sintaxis | `reject/10`, `reject/11` |
| 8 | `strategy` incompleta | Pasada semántica | `reject/24` |
| 9 | `ask` a un `decision` no declarado | Pasada semántica | `reject/25` |
| 10 | `simulate` de un juego inexistente o fuera de rango | Pasada semántica | `reject/26` a `reject/28` |
| 11 | Juego sin `win when` | Ya no es error (punto 8 de la devolución) | `accept/26` |

Los casos 2, 4, 5 y 6 se aceptan por ahora (falsos positivos, como anticipa el FAQ del Stage II). La pasada semántica ([SemanticAnalyzer.c](src/main/c/frontend/semantic-analysis/SemanticAnalyzer.c)) corre sobre el AST ya construido, porque un `simulate` o un `ask` pueden aparecer antes de lo que nombran. Reporta todos los errores, no solo el primero. El resto de `reject/` son errores de sintaxis que no están en la especificación, como sintaxis vieja, negativos donde no van o enteros fuera de rango.

## Supuestos del equipo

Donde la especificación no dice nada:

1. `if`, `while` y `for` llevan paréntesis, y todo cuerpo va entre llaves.
2. No hay sentencias-expresión sueltas: `roll D6;` no es válido, pero `integer pasos = roll D6;` sí.
3. `place` existe (`place p.token on board.cell(0);`), aunque la tabla de §4.5 no lo lista.
4. Las variables del `game` no pueden ser `piece`, `die`, `deck` ni `strategy`, que tienen su propia declaración.
5. Un `log` no puede contener `{` ni `}` literales.

Los ejemplos 6.1 y 6.2 de la especificación están adaptados a la sintaxis nueva en `accept/21` y `accept/22`.
