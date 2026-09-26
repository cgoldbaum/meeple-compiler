[![✗](https://img.shields.io/badge/Release-v1.0.0-ffb600.svg?style=for-the-badge)](https://github.com/cgoldbaum/meeple-compiler/releases)

[![✗](https://github.com/cgoldbaum/meeple-compiler/actions/workflows/pipeline.yaml/badge.svg?branch=development)](https://github.com/cgoldbaum/meeple-compiler/actions/workflows/pipeline.yaml)

# Meeple

**Meeple** es un DSL para diseñar, simular y balancear juegos de mesa por turnos. El compilador está desarrollado con Flex y Bison sobre el proyecto base [Flex-Bison-Compiler](https://github.com/agustin-golmar/Flex-Bison-Compiler) (rama `development`).

Este README acompaña la entrega del **Stage II (frontend)**: el compilador lexea y parsea programas Meeple, construye el AST, le corre una pasada semántica acotada (los casos de rechazo 8, 9 y 10 del PDF) y lo imprime por consola. Todas las aclaraciones sobre la entrega están en este README.

* [Compilar y correr](#compilar-y-correr)
* [Gramática](#gramática)
* [Cambios de sintaxis respecto del PDF](#cambios-de-sintaxis-respecto-del-pdf)
* [Chequeos semánticos sobre el AST](#chequeos-semánticos-sobre-el-ast)
* [Casos de rechazo: Stage II y Stage III](#casos-de-rechazo-stage-ii-y-stage-iii)
* [Supuestos del equipo](#supuestos-del-equipo)
* [Los ejemplos 6.1 y 6.2 del PDF](#los-ejemplos-61-y-62-del-pdf)
* [Por qué `decision` y `strategy` siguen separados](#por-qué-decision-y-strategy-siguen-separados)
* [Referencia del proyecto base](#referencia-del-proyecto-base)

## Documentación

* [Especificación de Meeple](doc/Especificacion-Meeple.pdf) (Stage I, 30/08/2026). En este README.

## Compilar y correr

El único requisito es [Docker](https://www.docker.com/) (v28.3.2). Para compilar y correr toda la suite de tests, desde la raíz del repositorio:

```bash
docker compose run --rm compiler bash -c "bash src/main/bash/build.sh && bash src/main/bash/test.sh"
```

`test.sh` pasa cada programa de `src/test/c/accept/` y `src/test/c/reject/` por el compilador. Un caso de aceptación pasa si el compilador termina con estado 0; uno de rechazo, si termina con cualquier otro estado. Si todos pasan, el script termina con estado 0.

Para compilar un solo programa e imprimir su AST:

```bash
docker compose run --rm compiler bash -c "bash src/main/bash/build.sh && bash src/main/bash/run.sh src/test/c/accept/21-ejemplo-la-oca"
```

Los tests están en tres carpetas:

| Carpeta | Contenido |
| :-- | :-- |
| `src/test/c/accept/` | Programas que el compilador debe aceptar (34). |
| `src/test/c/reject/` | Programas que el compilador debe rechazar (29). |
| `src/test/c/pending/` | Casos de rechazo del PDF que recién puede detectar el Stage III (4). `test.sh` no los corre: ver [Casos de rechazo](#casos-de-rechazo-stage-ii-y-stage-iii). |

## Gramática

La gramática es G = ⟨Σ, N, Π, S⟩. Σ lo define el analizador léxico ([FlexPatterns.l](src/main/c/frontend/lexical-analysis/FlexPatterns.l)); N, Π y S, el sintáctico ([BisonGrammar.y](src/main/c/frontend/syntactic-analysis/BisonGrammar.y)).

### Σ: alfabeto (105 símbolos)

| Grupo | Símbolos |
| :-- | :-- |
| Palabras clave de componentes (21) | `game` `games` `players` `player` `board` `cells` `die` `faces` `to` `piece` `per` `deck` `of` `cardtype` `card` `prepare` `turns` `turn` `order` `clockwise` `counterclockwise` |
| Palabras clave de decisiones (9) | `decision` `strategy` `prefer` `first` `input` `random` `ask` `for` `uses` |
| Tipos clásicos (3) | `integer` `boolean` `string` |
| Agregaciones (8) | `max` `min` `sum` `count` `select` `where` `by` `in` |
| Control de flujo (4) | `if` `else` `while` `repeat` |
| Acciones del dominio (12) | `roll` `place` `on` `move` `forward` `shuffle` `draw` `from` `play` `give` `take` `log` |
| Condiciones de fin (4) | `win` `when` `end` `after` |
| Simulación y reportes (8) | `seed` `simulate` `verbose` `with` `report` `winrate` `avg` `metric` |
| Predefinidos (5) | `current` `option` `none` `true` `false` |
| Operadores lógicos (3) | `and` `or` `not` |
| Operadores y puntuación (22) | `==` `!=` `<=` `>=` `<` `>` `=` `+` `-` `*` `/` `%` `;` `,` `:` `.` `{` `}` `(` `)` `[` `]` |
| Literales (6) | `INTEGER`, `IDENTIFIER`, `STRING`, `STRING_HEAD`, `STRING_MIDDLE`, `STRING_TAIL` |

Los literales, donde *texto* es `[^"{}\n]*`:

| Token | Patrón | Ejemplo |
| :-- | :-- | :-- |
| `INTEGER` | `[0-9]+`, hasta 2147483647 | `63` |
| `IDENTIFIER` | `[A-Za-z_][A-Za-z0-9_]*` que no sea palabra clave | `Mazo` |
| `STRING` | `"`*texto*`"` | `"La Oca"` |
| `STRING_HEAD` | `"`*texto*`{` | `"avanzo {` |
| `STRING_MIDDLE` | `}`*texto*`{` | `} y quedo en {` |
| `STRING_TAIL` | `}`*texto*`"` | `} puntos"` |

Los tres últimos forman las cadenas interpoladas del `log`: `"a {x} b {y} c"` se lexea como `STRING_HEAD` (`"a {`), la expresión `x`, `STRING_MIDDLE` (`} b {`), la expresión `y` y `STRING_TAIL` (`} c"`).

Los espacios y los comentarios (`// ...` y `/* ... */`) se descartan en el análisis léxico y no forman parte de Σ. El signo de un entero no es parte del literal: `-5` son dos símbolos, `-` e `INTEGER`.

### N: no terminales (67)

`program` `topLevelList` `topLevel` `simulation` `gameWord` `seedOpt` `verboseOpt` `reportItemList` `reportItem` `reportAggregator` `reportMetric` `gameDeclaration` `gameItemList` `gameItem` `gameVariable` `gameVariableType` `turnOrderValue` `perPlayerOpt` `playerCount` `naturalList` `integerSet` `integerList` `signedInteger` `fieldList` `field` `deckBody` `cardList` `card` `cardFieldList` `cardField` `generatorList` `generator` `valueSet` `literalList` `literal` `policyList` `policy` `criteria` `rankingList` `ranking` `finalCriterion` `typeSpec` `baseType` `block` `statementList` `statement` `identStatement` `lvalueTail` `lvalueEnd` `globalStatement` `globalName` `keywordDeclaration` `keywordType` `initializerOpt` `ifStatement` `elseOpt` `actionStatement` `interpolatedString` `interpolationRest` `expression` `postfix` `memberName` `argumentListOpt` `argumentList` `primary` `aggregation` `whereOpt`

### S: símbolo inicial

S = `program`

### Π: producciones (220)

Las palabras clave y los símbolos van entre comillas; los literales, en mayúsculas; λ es la cadena vacía.

```text
program            → topLevelList

topLevelList       → topLevel
                   | topLevelList topLevel

topLevel           → gameDeclaration
                   | simulation
                   | "report" "{" reportItemList "}"

simulation         → "simulate" INTEGER gameWord "of" STRING "with" INTEGER "players" seedOpt verboseOpt ";"

gameWord           → "game"
                   | "games"

seedOpt            → λ
                   | "seed" INTEGER

verboseOpt         → λ
                   | "verbose"

reportItemList     → reportItem
                   | reportItemList reportItem

reportItem         → "winrate" ";"
                   | "winrate" "by" "player" ";"
                   | "winrate" "by" "strategy" ";"
                   | reportAggregator reportMetric ";"

reportAggregator   → "avg"
                   | "min"
                   | "max"

reportMetric       → "turns"
                   | IDENTIFIER

gameDeclaration    → "game" STRING "{" gameItemList "}"

gameItemList       → gameItem
                   | gameItemList gameItem

gameItem           → "players" playerCount ";"
                   | "board" "cells" INTEGER ";"
                   | "die" IDENTIFIER "faces" integerSet ";"
                   | "piece" IDENTIFIER perPlayerOpt ";"
                   | "cardtype" IDENTIFIER "{" fieldList "}"
                   | "deck" IDENTIFIER "of" IDENTIFIER perPlayerOpt ";"
                   | "deck" IDENTIFIER "of" IDENTIFIER "{" deckBody "}"
                   | gameVariable
                   | "metric" IDENTIFIER "=" expression ";"
                   | "decision" IDENTIFIER ":" typeSpec ";"
                   | "strategy" IDENTIFIER "{" policyList "}"
                   | "prepare" block
                   | "turn" block
                   | "turn" "order" turnOrderValue ";"
                   | "win" "when" expression ";"
                   | "end" "after" INTEGER "turns" ";"

gameVariable       → gameVariableType IDENTIFIER initializerOpt ";"
                   | gameVariableType "[" "]" IDENTIFIER initializerOpt ";"
                   | IDENTIFIER IDENTIFIER initializerOpt ";"
                   | IDENTIFIER "[" "]" IDENTIFIER initializerOpt ";"

gameVariableType   → "integer"
                   | "boolean"
                   | "string"
                   | "player"

turnOrderValue     → "clockwise"
                   | "counterclockwise"

perPlayerOpt       → λ
                   | "per" "player"

playerCount        → INTEGER "to" INTEGER
                   | "{" naturalList "}"

naturalList        → INTEGER
                   | naturalList "," INTEGER

integerSet         → signedInteger "to" signedInteger
                   | "{" integerList "}"

integerList        → signedInteger
                   | integerList "," signedInteger

signedInteger      → INTEGER
                   | "-" INTEGER

fieldList          → field
                   | fieldList field

field              → typeSpec IDENTIFIER ";"

deckBody           → cardList
                   | generatorList

cardList           → card
                   | cardList card

card               → "card" "{" cardFieldList "}"
                   | "card" "{" argumentList "}"

cardFieldList      → cardField
                   | cardFieldList cardField

cardField          → IDENTIFIER ":" expression ";"

generatorList      → generator
                   | generatorList generator

generator          → IDENTIFIER ":" valueSet ";"

valueSet           → signedInteger "to" signedInteger
                   | "{" literalList "}"

literalList        → literal
                   | literalList "," literal

literal            → signedInteger
                   | STRING
                   | "true"
                   | "false"

policyList         → policy
                   | policyList policy

policy             → IDENTIFIER ":" "prefer" criteria whereOpt ";"

criteria           → rankingList
                   | rankingList "," finalCriterion
                   | finalCriterion

rankingList        → ranking
                   | rankingList "," ranking

ranking            → "max" expression
                   | "min" expression

finalCriterion     → "random"
                   | "first"
                   | "input"

typeSpec           → baseType
                   | baseType "[" "]"

baseType           → "integer"
                   | "boolean"
                   | "string"
                   | "player"
                   | "piece"
                   | "die"
                   | "deck"
                   | "strategy"
                   | IDENTIFIER

block              → "{" statementList "}"
                   | "{" "}"

statementList      → statement
                   | statementList statement

statement          → identStatement
                   | globalStatement
                   | keywordDeclaration
                   | ifStatement
                   | "while" "(" expression ")" block
                   | "repeat" expression block
                   | "for" "(" IDENTIFIER "in" expression ")" block
                   | "for" "(" IDENTIFIER "in" expression "to" expression ")" block
                   | actionStatement
                   | block

identStatement     → IDENTIFIER IDENTIFIER initializerOpt ";"
                   | IDENTIFIER "[" "]" IDENTIFIER initializerOpt ";"
                   | IDENTIFIER lvalueEnd
                   | IDENTIFIER lvalueTail lvalueEnd

lvalueTail         → "." memberName
                   | "[" expression "]"
                   | lvalueTail "." memberName
                   | lvalueTail "[" expression "]"

lvalueEnd          → "=" expression ";"
                   | "uses" IDENTIFIER ";"

globalStatement    → globalName lvalueTail lvalueEnd

globalName         → "current"
                   | "players"
                   | "option"

keywordDeclaration → keywordType IDENTIFIER initializerOpt ";"
                   | keywordType "[" "]" IDENTIFIER initializerOpt ";"

keywordType        → "integer"
                   | "boolean"
                   | "string"
                   | "player"
                   | "piece"
                   | "die"
                   | "deck"
                   | "strategy"

initializerOpt     → λ
                   | "=" expression

ifStatement        → "if" "(" expression ")" block elseOpt

elseOpt            → λ
                   | "else" block
                   | "else" ifStatement

actionStatement    → "place" postfix "on" expression ";"
                   | "move" postfix "forward" expression ";"
                   | "shuffle" postfix ";"
                   | "draw" expression "from" postfix "to" postfix ";"
                   | "play" expression "from" postfix "to" postfix ";"
                   | "give" expression "to" postfix ";"
                   | "take" expression "from" postfix ";"
                   | "log" interpolatedString ";"

interpolatedString → STRING
                   | STRING_HEAD expression interpolationRest

interpolationRest  → STRING_TAIL
                   | STRING_MIDDLE expression interpolationRest

expression         → expression "+" expression
                   | expression "-" expression
                   | expression "*" expression
                   | expression "/" expression
                   | expression "%" expression
                   | expression "<" expression
                   | expression ">" expression
                   | expression "<=" expression
                   | expression ">=" expression
                   | expression "==" expression
                   | expression "!=" expression
                   | expression "and" expression
                   | expression "or" expression
                   | "not" expression
                   | "-" expression
                   | postfix

postfix            → primary
                   | postfix "." memberName
                   | postfix "[" expression "]"
                   | postfix "(" argumentListOpt ")"

memberName         → IDENTIFIER
                   | "max"
                   | "min"
                   | "strategy"
                   | "cardtype"
                   | "faces"

argumentListOpt    → λ
                   | argumentList

argumentList       → expression
                   | argumentList "," expression

primary            → INTEGER
                   | STRING
                   | "true"
                   | "false"
                   | "none"
                   | IDENTIFIER
                   | "current"
                   | "players"
                   | "turns"
                   | "option"
                   | "board"
                   | "(" expression ")"
                   | "roll" IDENTIFIER
                   | "ask" expression "for" IDENTIFIER "(" argumentListOpt ")"
                   | aggregation

aggregation        → "count" "(" IDENTIFIER "in" expression whereOpt ")"
                   | "select" "(" IDENTIFIER "in" expression whereOpt ")"
                   | "sum" "(" IDENTIFIER "in" expression whereOpt "by" expression ")"
                   | "max" "(" IDENTIFIER "in" expression whereOpt "by" expression ")"
                   | "min" "(" IDENTIFIER "in" expression whereOpt "by" expression ")"

whereOpt           → λ
                   | "where" expression
```

Las producciones de `expression` son ambiguas a propósito. Bison las desambigua con esta tabla de precedencias, de menor a mayor:

| Precedencia | Operadores | Asociatividad |
| :-: | :-- | :-- |
| 1 | `or` | izquierda |
| 2 | `and` | izquierda |
| 3 | `not` | unario |
| 4 | `==` `!=` | izquierda |
| 5 | `<` `>` `<=` `>=` | izquierda |
| 6 | `+` `-` | izquierda |
| 7 | `*` `/` `%` | izquierda |
| 8 | `-` unario | unario |

Aparte de eso, la gramática es LALR(1) sin conflictos. El build lo garantiza: `bison.sh` corre con `-Werror=conflicts-sr` y `-Werror=conflicts-rr`, así que un conflicto rompe la compilación.

## Cambios de sintaxis respecto del PDF

La devolución del Stage I pidió estos cambios, y ya están implementados. La columna "Tests" indica dónde verlos.

| Tema | En el PDF | Ahora | Tests |
| :-- | :-- | :-- | :-- |
| Cantidad de jugadores | Solo rango: `players 2 to 4;` | También un conjunto: `players {2, 4, 6};`. Nunca vacío. | `accept/25`, `reject/12`, `reject/20` |
| `for` | Solo sobre una colección: `for (p in players)` | También sobre un rango de enteros, extremos incluidos: `for (i in 1 to n)` | `accept/27` |
| Mazos | Solo una lista de `card { campo: valor; }` | Generadores que arman el producto cartesiano (`valor: 1 to 12; palo: {"Copa", "Oro", "Basto", "Espada"};` son 48 cartas) y cartas posicionales (`card {1, 1}`, con los valores en el orden de los campos del `cardtype`). Un mazo tiene cartas o generadores, no las dos cosas. | `accept/23`, `accept/24`, `reject/15`, `reject/22` |
| Tablero | `board Tablero cells 63;` y `Tablero.cell(0)` | `board cells 63;` y `board.cell(0)`. Hay un único tablero por juego, así que el nombre sobraba. | `accept/02`, `accept/06`, `reject/16` |
| Semilla y traza | `seed 42;` y `log level verbose \| none;` sueltos | Cláusulas opcionales de `simulate`, en ese orden: `simulate 1000 games of "La Oca" with 4 players seed 42 verbose;` | `accept/20`, `accept/29`, `reject/05`, `reject/13` |
| Nombres | `setup { ... }` y `turn_number` | `prepare { ... }` y `turns` | `accept/06`, `accept/15`, `reject/17` |
| `log` | Marcadores posicionales: `log "{1} avanzo {2}", current, pasos;` | Interpolación de expresiones: `log "{current.name} avanzo {pasos}";` | `accept/15`, `reject/10`, `reject/11`, `reject/18` |
| `decision` | `decision jugar(opciones: Carta[]) -> Carta;` | `decision jugar: Carta;`. El tipo es el de `option` y el del resultado del `ask`; las opciones siempre son un arreglo de ese tipo y se pasan en el `ask`. | `accept/16`, `accept/28`, `reject/19` |
| `prefer` | Una sola política: `prefer max <expr>`, `prefer min <expr>`, `prefer random` o `prefer first` | Criterios encadenados con coma, donde cada uno desempata al anterior; un filtro `where` opcional; y el criterio nuevo `input`. `random`, `first` e `input` solo pueden ir al final, porque después de ellos no queda nada que desempatar. Ejemplo: `jugar: prefer max option.ataque, min option.costo where option.costo <= current.score;` | `accept/30`, `accept/33`, `reject/14` |
| Variables del juego | No existían | Se declaran dentro del `game`, con inicializador opcional: `integer cartasJugadas = 0;`, `Carta ultimaJugada = none;`, `integer[] historial;` | `accept/31` |
| `metric` y `report` | `report` con `winrate by player`, `winrate by strategy` y `avg turns` | `metric jugadas = cartasJugadas;` dentro del `game`. `report` acepta `winrate;`, `winrate by player;`, `winrate by strategy;`, y `avg`, `min` o `max` sobre `turns` o sobre cualquier `metric`. | `accept/32` |

## Chequeos semánticos sobre el AST

Tres de los casos de rechazo del PDF no se pueden chequear en una acción de Bison, porque miran declaraciones que pueden aparecer *después* del uso: una `strategy` se reduce antes de saber si más abajo hay otro `decision`, y un `simulate` puede preceder al `game` que nombra. Para esos tres hay una pasada sobre el AST ya construido, en [SemanticAnalyzer.c](src/main/c/frontend/semantic-analysis/SemanticAnalyzer.c), que corre entre el análisis sintáctico y el backend.

| Caso | Qué chequea | Tests |
| :-: | :-- | :-- |
| 8 | Toda `strategy` tiene una política para cada `decision` de su `game`, y ninguna política nombra un `decision` que no existe. | `reject/24`, `reject/29` |
| 9 | Todo `ask` nombra un `decision` declarado en su `game`. | `reject/25` |
| 10 | Todo `simulate` nombra un `game` declarado, con una cantidad de jugadores que ese `game` admite: un rango incluye sus extremos, y un conjunto se chequea por pertenencia. | `reject/26`, `reject/27`, `reject/28`, `accept/34` |

La pasada no aborta en el primer error: reporta todos los que encuentra y recién después rechaza el programa. Los errores salen por `stderr`, con el mismo formato que los de sintaxis y el prefijo `[SemanticAnalyzer]`:

```text
[ERROR][SemanticAnalyzer] Line 8: la strategy "Incompleta" no resuelve la decision "descartar".
```

Dos aclaraciones de alcance, porque todavía no hay tabla de símbolos:

* Un `game` que no declara `players` acepta cualquier cantidad en el `simulate`: no hay contra qué comparar.
* Los `ask` se buscan en todas las expresiones del `game` (los bloques `prepare` y `turn`, el `win when`, las `metric`, los inicializadores de las variables, los criterios de las estrategias y los campos de las cartas), pero solo se chequea el nombre del `decision`, no los tipos de los argumentos. Eso es del Stage III.

## Casos de rechazo: Stage II y Stage III

El Stage II tiene análisis léxico, sintáctico y la pasada semántica de la sección anterior. Los casos de rechazo que necesitan tabla de símbolos o chequeo de tipos se aceptan por ahora, como anticipa la consigna (§4.2, FAQ). No los borramos: están en `src/test/c/pending/`, cada uno con un comentario arriba que dice qué fase lo va a detectar. `test.sh` no recorre esa carpeta. En el Stage III esos archivos pasan a `reject/`.

Los casos de rechazo del PDF (§5.2):

| # | Caso | Lo detecta | Test |
| :-: | :-- | :-- | :-- |
| 1 | Programa malformado (falta un `;`, llave sin cerrar) | Stage II: sintaxis | `reject/01`, `reject/02` |
| 2 | Ficha o jugador no declarado | Stage III: tabla de símbolos | `pending/02` |
| 3 | Rango de jugadores inválido (mínimo mayor que máximo) | Stage II: lo chequea la acción de Bison al reducir el rango | `reject/20` |
| 4 | Tipos incompatibles | Stage III: chequeo de tipos | `pending/04` |
| 5 | Carta con un campo que su `cardtype` no declara | Stage III: tabla de símbolos | `pending/05` |
| 6 | `draw` entre mazos de `cardtype` distinto | Stage III: chequeo de tipos | `pending/06` |
| 7 | `log` con más marcadores que argumentos | Ya no se puede escribir: el `log` nuevo no tiene marcadores posicionales. Los errores equivalentes (interpolación vacía o sin cerrar) son de sintaxis y los detecta el Stage II. | `reject/10`, `reject/11` |
| 8 | `strategy` que no resuelve todos los `decision` | Stage II: la pasada semántica sobre el AST | `reject/24` |
| 9 | `ask` a un `decision` no declarado | Stage II: la pasada semántica sobre el AST | `reject/25` |
| 10 | `simulate` de un juego inexistente, o con más jugadores que el máximo | Stage II: la pasada semántica sobre el AST | `reject/26`, `reject/27`, `reject/28` |
| 11 | Juego sin `win when` | Ya no es un error. La devolución (punto 8) observó que contradice el caso de aceptación 1 del mismo PDF y que un juego sin ganador es simulable. Ahora se acepta. | `accept/26` |

Además, el Stage II rechaza estos programas, que no están en el PDF:

| Test | Por qué se rechaza |
| :-- | :-- |
| `reject/03` | Programa vacío: un programa tiene al menos un `game`, `simulate` o `report`. |
| `reject/04` | Comentario de bloque sin cerrar al llegar al final del archivo. |
| `reject/05`, `reject/06` | `seed -1` y `cells -5`: los negativos solo se admiten en las caras de un dado y en los generadores de mazo. |
| `reject/07` | `roll D6;` como sentencia: no hay sentencias-expresión sueltas (ver [Supuestos](#supuestos-del-equipo)). |
| `reject/08` | `current.player`: `player` es palabra clave y no es miembro de ningún tipo. |
| `reject/09` | Carácter que no pertenece a Σ (`@`). |
| `reject/12` | `players {}`: un conjunto de jugadores no puede ser vacío. |
| `reject/13`, `reject/16`, `reject/17`, `reject/19` | Sintaxis vieja: `seed` suelto, `board` con nombre, `setup` y la firma vieja de `decision`. |
| `reject/14` | `prefer random, max ...`: `random` solo puede ir al final. |
| `reject/15` | Mazo que mezcla cartas y generadores. |
| `reject/18` | `{` dentro de una cadena que no es de un `log`. |
| `reject/21`, `reject/22` | Rangos con mínimo mayor que máximo en las caras de un dado y en un generador. |
| `reject/23` | Entero mayor que 2147483647. |
| `reject/29` | Una política de una `strategy` nombra un `decision` que el `game` no declara. |

## Supuestos del equipo

Decisiones que tomamos donde el PDF no dice nada o es ambiguo:

1. **Paréntesis obligatorios en `if`, `while` y `for`.** Se escribe `if (x > 0) { ... }`, nunca `if x > 0 { ... }`. `repeat` no lleva paréntesis: `repeat 3 { ... }`. Además, todo cuerpo va entre llaves, incluso si tiene una sola sentencia. Con eso desaparece el *dangling else* sin necesidad de precedencias.
2. **No hay sentencias-expresión sueltas.** `roll D6;` o `ask current for jugar(current.Mano);` solos no son sentencias válidas: el valor se usa en una declaración, una asignación o como argumento de una acción (`integer pasos = roll D6;`). Las sentencias son declaraciones, asignaciones, `uses`, bloques, control de flujo y acciones del dominio.
3. **Se implementa `place`** (`place p.token on board.cell(0);`), aunque la tabla de acciones del PDF (§4.5) no lo lista. El PDF lo nombra en el mapeo del dominio (§3.1) y lo usa en el ejemplo 6.1, así que lo tomamos como parte del lenguaje.
4. **Tipos de las variables del juego.** Una variable declarada en el `game` puede ser `integer`, `boolean`, `string`, `player` o un `cardtype`, o un arreglo de cualquiera de ellos. No puede ser `piece`, `die`, `deck` ni `strategy`: esas palabras ya abren su propia declaración dentro del `game`, y `piece p;` sería a la vez una variable y una ficha (conflicto reduce/reduce). Para esos componentes están sus propias declaraciones. Dentro de `prepare` y `turn`, las variables locales admiten todos los tipos.
5. **No se puede escribir una `{` literal dentro de un `log`.** Las llaves delimitan las interpolaciones, y no hay secuencias de escape. Tampoco se puede escribir `}`. Fuera del `log`, una cadena con llaves es un error de sintaxis (`reject/18`). Las cadenas tampoco pueden contener `"` ni saltos de línea.

## Los ejemplos 6.1 y 6.2 del PDF

Los dos ejemplos completos del PDF están en los tests como `accept/21-ejemplo-la-oca` (6.1) y `accept/22-ejemplo-duelo-de-cartas` (6.2). No están copiados textualmente: se adaptaron a la sintaxis nueva y se corrigieron erratas.

* **En los dos:** `seed` pasa a ser una cláusula de `simulate`, y `setup` pasa a ser `prepare`.
* **6.1:** `board Tablero cells 63;` pasa a ser `board cells 63;`, y `Tablero.cell(0)` pasa a ser `board.cell(0)`. El `log` usa interpolación: `log "{current.name} avanzo {pasos} y quedo en la celda {current.token.cell}";`. Donde el PDF pasaba `current` (un `player`) al primer marcador, la interpolación usa `current.name`.
* **6.2:** `decision jugar(opciones: Carta[]) -> Carta;` pasa a ser `decision jugar: Carta;`. El mazo tiene 4 cartas en vez de 8 para que el test sea compacto; el resto del programa no cambia.

## Por qué `decision` y `strategy` siguen separados

El punto 33 de la devolución sugirió unificar `decision` y `strategy`. Es el único punto donde no seguimos una sugerencia de la cátedra, así que dejamos la justificación escrita.

`decision` declara *qué* se decide y sobre qué tipo; `strategy` declara *cómo* lo resuelve un jugador. Los separamos por la misma razón por la que se separa una interfaz de sus implementaciones:

1. **Varias estrategias resuelven los mismos puntos de decisión.** Todo el lenguaje apunta a comparar estrategias (`winrate by strategy`). Para eso, todas tienen que responder al mismo conjunto de decisiones. `decision` es ese contrato, declarado una sola vez y no repetido en cada `strategy`.
2. **El tipo de `option` sale de la `decision`.** Con `decision jugar: Carta;`, `prefer max option.ataque` se puede chequear en todas las estrategias: `option` es una `Carta` y `ataque` es un campo de `Carta`. Sin la declaración, cada estrategia tendría que repetir el tipo, o habría que inferirlo de los `ask` del `turn`, que están en otro lugar del programa y pueden no coincidir entre sí.
3. **Los chequeos semánticos del PDF necesitan la lista de decisiones.** "Toda estrategia debe resolver todos los puntos de decisión declarados" (§4.6, rechazo 8) y "`ask` a un punto de decisión no declarado" (rechazo 9) solo tienen sentido si esa lista existe aparte de las estrategias. Si la unificáramos, una estrategia que olvida una decisión no sería un error de compilación: fallaría en medio de una simulación, recién cuando algún `ask` llegara a esa decisión.
4. **Un jugador sin estrategia también decide.** Si no recibe `uses`, resuelve todo con `prefer first` (§4.6). Los puntos de decisión existen aunque no haya ninguna `strategy` declarada.
5. **El costo es bajo.** Con la firma nueva, que sí simplificamos, cada punto de decisión es una línea: `decision jugar: Carta;`.

## Referencia del proyecto base

Esta sección viene del proyecto base.

### Configuration

Set the following environment variables to control and configure the behaviour of the application:

| Name                  | Default | Description                                                                                                                                                           |
| :-------------------- | :-----: | :-------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `ENVIRONMENT`         | `Local` | The active environment name. The available environments are: `Local`, `Development` and `Production`.                                                                 |
| `LOG_IGNORED_LEXEMES` | `true`  | When `true`, logs all of the ignored lexemes found with Flex at `DEBUGGING` level. To remove those logs from the console output set it to `false`.                    |
| `LOGGING_LEVEL`       | `ALL`   | The minimum level to log in the console output. From lower to higher, the available levels are: `ALL`, `DEBUGGING`, `INFORMATION`, `WARNING`, `ERROR` and `CRITICAL`. |

_Docker Compose_ can read the variables from an `.env` file too (see `compose.yaml` file).

### Commands

#### Start

Rises an ephemeral container, ready to start development:

```bash
docker compose run --rm compiler
```

#### Build

Builds or rebuilds the entire compiler:

```bash
src/main/bash/build.sh
```

#### Run

Compiles a program:

```bash
src/main/bash/run.sh <program>
```

where `<program>` is the path to the file that represents its entry-point.

#### Test

Executes every available unit-test under `src/test/c` folder:

```bash
src/main/bash/test.sh
```

#### Stop

Logout, destroy the ephemeral containers and shutdowns the cluster:

```bash
exit
docker compose down
```

#### Docker

| Command                                 | Description                                             |
| :-------------------------------------- | :------------------------------------------------------ |
| `docker builder prune --all`            | Removes all builds and complete build cache.            |
| `docker compose --progress=plain build` | Forces a build or rebuild of the images in the cluster. |
| `docker image prune`                    | Removes all of the dangling images from Docker.         |
| `docker network prune`                  | Removes unused networks from Docker.                    |
| `docker volume prune`                   | Removes unused volumes from Docker.                     |

### CI/CD

To trigger an automatic integration on every push or PR (_Pull Request_), you must activate _GitHub Actions_ in the _Settings_ tab. Use the following configuration:

| Key                                                        | Value                                               |
| :--------------------------------------------------------- | :-------------------------------------------------- |
| `Actions permissions`                                      | `Allow all actions and reusable workflows`          |
| `Allow GitHub Actions to create and approve pull requests` | `false`                                             |
| `Artifact and log retention`                               | `30 days`                                           |
| `Fork pull request workflows from outside collaborators`   | `Require approval for all outside collaborators`    |
| `Workflow permissions`                                     | `Read repository contents and packages permissions` |

### Recommended Extensions

* [C/C++](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools)
* [CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools)
* [Yash](https://marketplace.visualstudio.com/items?itemName=daohong-emilio.yash)
