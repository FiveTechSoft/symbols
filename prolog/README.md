# Motor simbólico cognitivo (Prolog)

De los 80 hechos de EXP1 a la correferencia de EXP18, capa por capa,
sin `backpropagation`, sin `gradient descent`, sin embeddings ni matrices
de atención. Solo símbolos, relaciones, variables, reglas y memoria.

> **Afirmación defendible (y nada más):** es posible construir un mecanismo
> de aprendizaje simbólico que descubre estructura, variables, composición,
> restricciones y meta-estructuras, y que transfiere esas estructuras a
> vocabularios que nunca había visto. No se afirma paridad con LLMs.

## Requisitos

- [SWI-Prolog](https://www.swi-prolog.org/) 9+ (`swipl` en el `PATH`).
- Todo se ejecuta desde esta carpeta `prolog/` (los `consult` son relativos).

## Cómo ejecutar

```sh
swipl -s experiment.pl   -g experiment   -t halt
swipl -s experiment2.pl  -g experiment2  -t halt
swipl -s experiment3.pl  -g experiment3  -t halt
swipl -s experiment4.pl  -g experiment4  -t halt
swipl -s experiment5.pl  -g experiment5  -t halt
swipl -s experiment6.pl  -g experiment6  -t halt
swipl -s experiment7.pl  -g experiment7  -t halt
swipl -s experiment8.pl  -g experiment8  -t halt
swipl -s experiment9.pl  -g experiment9  -t halt
swipl -s experiment10.pl -g experiment10 -t halt
swipl -s experiment11.pl -g experiment11 -t halt
swipl -s experiment12.pl -g experiment12 -t halt
swipl -s experiment13.pl -g experiment13 -t halt
swipl -s experiment14.pl -g experiment14 -t halt
swipl -s experiment15.pl -g experiment15 -t halt
swipl -s experiment16.pl -g experiment16 -t halt
swipl -s experiment17.pl -g experiment17 -t halt
swipl -s experiment18.pl -g experiment18 -t halt
swipl -s experiment19.pl -g experiment19 -t halt
swipl -s experiment20.pl -g experiment20 -t halt
swipl -s experiment21.pl -g experiment21 -t halt
swipl -s experiment22.pl -g experiment22 -t halt
swipl -s experiment23.pl -g experiment23 -t halt
swipl -s experiment24.pl -g experiment24 -t halt
swipl -s experiment25.pl -g experiment25 -t halt
swipl -s experiment26.pl -g experiment26 -t halt
swipl -s experiment27.pl -g experiment27 -t halt
```

Ritual pre-vuelo (estático, sin ejecutar) y corpus:

```sh
swipl -s preflight.pl -g "preflight('experiment19.pl')" -t halt
swipl -s preflight.pl -g "preflight_project('.')" -t halt
swipl -s run_corpus1.pl -g main -t halt
python3 make_corpus1.py   # regenera corpus1/corpus1.txt + heldout1.pl
swipl -s experiment28.pl -g experiment28 -t halt
python3 make_corpus2.py   # regenera corpus2/*.txt + heldout2.pl
swipl -s experiment29.pl -g experiment29 -t halt
python3 make_corpus3.py   # regenera corpus3/* + heldout3/distractor3
swipl -s experiment30.pl -g experiment30 -t halt
python3 make_corpusA.py   # regenera corpusA/* + secreto + heldoutA/distractorA
swipl -s experiment31.pl -g experiment31 -t halt
swipl -s experiment32.pl -g experiment32 -t halt
swipl -s experiment33.pl -g experiment33 -t halt
swipl -s experiment34.pl -g experiment34 -t halt
swipl -s experiment35.pl -g experiment35 -t halt
swipl -s experiment36.pl -g experiment36 -t halt
swipl -s experiment37.pl -g experiment37 -t halt
```

Cada experimento arranca con memoria vacía, carga solo sus módulos
explícitos y demuestra qué conocimiento entra, qué se induce y qué sobrevive.

## Resultados

| Exp | Capacidad | Resultado verificado |
|-----|-----------|----------------------|
| 1 | Conceptos y reglas base (80 hechos) | 3 conceptos, 3 reglas, 20/20 + 20/20 |
| 2 | Símbolos opacos | TP=20 FN=0 FP=0 TN=20, F1=1.00 |
| 3 | Variación estructural | F1=0.00 por bug propio (`object_index/3` devolvía enteros, no átomos `xN`): el test preguntaba por símbolos que nunca existieron. Lección, no evidencia |
| 4 | Correspondencia `Q(N)→X(N)` por sufijo | F1=1.00; baseline sobregeneraliza 6/6 distractores |
| 5 | Variable compartida emergente `tag(S,B)∧tag(O,B)` | F1=1.00, sin números programados |
| 6 | Composición `r1∧r2→r3` (longitud descubierta 1..3) | F1=1.00 |
| 7 | Abducción nivel 1 (variable latente, sin inventar nodos) | 3/3 enlaces retenidos exactos + `ambiguous/2` |
| 8 | Aprendizaje continuo sin interferencia | 24/24; fase 4: +4 hechos, +0 conceptos |
| 9 | Multivariable con identidad (`A≠B`) | F1=1.00 |
| 10 | Reutilización: `[1,2,3,4]` vs `[1,2,2,3]` vs `[1,2,1]` + control mixto `r7` rehusado | TP=3 FP=0 TN=8, F1=1.00 |
| 11 | Meta-reglas y transferencia cross-vocabulario (lo concreto rehúsa F1=0.5, la forma licencia) | TP=5 FP=0 TN=10, F1=1.00 |
| 12 | Instanciación persistente (`longterm.pl`: reload + sin meta, idéntico) | 3× (TP=4 FP=0 TN=6), INVENTION=0 |
| 13 | Lenguaje→grafo ES (limpio/variación/ruido) | Extracción P/R=1.00 ×4 capas, held-out F1=1.00, adversarial 6/6 |
| 14 | Sesiones EN: convergencia símbolos==lenguaje; sesión 2 aplica sin reinducir | S1 TP=2 TN=4, S2 TP=2 TN=4 |
| 15 | Vocabulario abierto: unknowns sin tipos inventados | Recall 5/5, tipos 4/4, F1=1.00, hallucination 0% |
| 16 | Conceptos emergentes sin ningún hecho de tipo + `what_is/1` | Partición exacta en 5, held-out dentro, 0 tipos |
| 17 | Identidad SAME/DIFFERENT/UNKNOWN/CONTRADICTION; funcionalidad descubierta | 16/16 |
| 18 | Correferencia como abducción de identidad (género+funcionalidad, nunca azar) | 17/17, memoria exacta post-reconstrucción |
| 19 | Preguntas sobre memoria: retrieval vs reasoning + prueba (`Why`) | 10/10; Q3 inferida (no almacenada) con cadena de justificación |
| 20 | Conflicto, tiempo y revisión (provenance; pasado interpola, futuro UNKNOWN) | 18/18; `INVENTION` de tipos: 0 |
| 21 | Regla transferida con memoria factual borrada (solo el programa sobrevive) | 11/11; `sofia→italy` sin ningún hecho `reaches` |
| 22 | Ciclo cerrado: olvidar experiencia, conservar regla, 2ª familia sin interferencia | 11/11; `reaches` intacta tras aprender `arrives` |
| 23 | Búsqueda guiada vs exhaustiva (mismo F1, 16.6× menos patrones) | 5/5; ganador y F1 idénticos |
| 24 | Composición jerárquica: reglas como unidades (340 vs 16 evals, mismas predicciones) | 3/3; skill library `r3`, `r6` |
| 25 | Composición profunda 3 niveles (340 vs 20 evals, mismas predicciones) | 3/3; skills `r3`, `r7` |
| 26 | Skills persistentes sin hechos; transferencia a entidades nuevas | 10/10; `r9` se ejecuta sin `constrained` ni inducción |
| 27 | Clases emergentes sin declarar + herencia de skills (0 tipos) | 12/12; `obj6/obj8` heredan `reaches`, `obj7` no |
| 28 | Corpus v0.1: ingesta separada + ciclo + Corpus 1 (150 frases EN) | 21/21; 150/150 hechos, held-out 10/10 razonado |
| EXP28 | Guided corpus learning por bloques + control negativo de ruido | 33/33; guided << exhaustive por bloque |
| EXP29 | Corpus 3: transferencia real (vocabulario y relaciones inéditos) | 40/40; 10 ocultos con proof atribuida a meta, 20 TN verificados |
| EXP30 | Primer corpus natural (cero lexicón) + secreto temporal | 20/20; retrieve/reason/unknown, prueba por acierto |
| EXP31 | Alineación de marcos (roles descubiertos, eventos reificados) | 12/12; 12→6 nodos, mapa 3/3, queries cruzadas |
| EXP32 | Embeddings simbólicos (firmas WL sin etiquetas, 3 vocabularios) | 8/8; clases absorbentes + similitud explicada |
| EXP33 | POO emergente: clase→comportamiento heredado (identidad≠pertenencia) | 10/10; `zorin/velara` heredan, `wex` UNKNOWN |
| EXP34 | Separación estructura/comportamiento (mismo dato, veredictos opuestos) | 9/9; sin `reaches`: identidad exacta de 5 |
| EXP35 | SSE por tarea (un objeto, 3 firmas auditadas, 0 fuga) | 12/12; firmas distintas + respuestas correctas |
| EXP36 | SSE de estructuras (transferencia estructural + regla `findall`) | 12/12; estructura→concepto→skill en vocabulario nuevo |
| EXP37 | Composición de conceptos (skills compuestas sin volver a hechos) | 16/16; `cater` vía cA+cB, prueba composicional |

## Arquitectura emergente

```text
LANGUAGE → SYMBOLIZER → GRAPH → MEMORY → CONCEPTS → PATTERNS
→ VARIABLES → CONSTRAINTS → RULES → META-RULES → INSTANTIATION
→ LONG-TERM MEMORY → NEW EXPERIENCE
```

Capas de conocimiento: `L0` hechos, `L1` reglas, `L2` meta-reglas,
`L3` instancias (una `L3` funciona sin su `L2`: independencia real).
Identidad separada en 4 niveles: `ENTITY IDENTITY`, `CONCEPT MEMBERSHIP`,
`STRUCTURAL EQUIVALENCE`, `RELATIONAL KNOWLEDGE`.

Checkpoint pre-EXP36 (secuencia conceptual cerrada EXP30–35):

```text
TEXT
 ↓
SYMBOLS
 ↓
GRAPH
 ↓
ROLES
 ↓
OBJECTS
 ↓
SSE
 ↓
CLASSES
 ↓
SKILLS
 ↓
TASK-CONDITIONED SSE
 ↓
INFERENCE
 ↓
PROOF
```

`SSE(object)` distingue de `SSE(object | task)`: la segunda se
construye excluyendo explícitamente la variable objetivo
(principio de exclusión evidencial, EXP34–35). EXP36–37 elevan la
unidad a estructuras (`SSE(structure | task)` → conceptos
estructurales) y a su composición (conceptos como ladrillos,
skills compuestas con prueba composicional, sin volver a hechos).

## Principios verificados (con refs)

- **Rehusar es válido**: `ambiguous` (EXP7), `REFUSED r7` (EXP10),
  `miss` antes que falso positivo (EXP13), `UNKNOWN ≠ DIFFERENT` (EXP17).
- **Existencia ≠ significado**: el símbolo entra al grafo; el tipo solo
  con evidencia `is_a` (EXP15). INVENTION=0 (EXP12/15).
- **Indistinguibilidad**: sin evidencia diferencial, no hay diferencia
  (Barcelona≈Velara hasta `mia lives_in barcelona`, EXP16).
- **Estructura ≠ entidad**: mismo concepto no implica misma entidad (EXP17.5).
- **Forma ≠ contenido**: la meta-regla licencia estructura, no inventa
  vocabulario (`z4` rehusado, EXP12).
- **Localidad**: `ΔKnowledge ≈ ΔExperience` (EXP8 fase 4; EXP18:
  reconstrucción exacta de 10 tripletas).
- **Reglas autónomas**: disparan en memoria cruda sin sus conceptos de
  origen (EXP8 `nora`, EXP10 `xilo`).
- **Exclusión evidencial**: la representación empleada para inferir una
  propiedad debe construirse sin utilizar evidencia de esa misma
  propiedad (`sse_excluding/3` lo garantiza por construcción, no por
  disciplina: EXP34 separa estructura/comportamiento, EXP35 una firma
  distinta por tarea con cero fuga).
- **Exclusión temporal** (EXP37): la evidencia de una conclusión solo
  puede entrar después de descubierta la estructura que debe
  explicarla (`cater` observado tras inducir sub-skills; presente
  antes, empataría el descubrimiento y rompería el margen).
- **Exclusión global por objetivo** (EXP37): `SSE(X | T)` =
  representación estructural de `X` menos toda evidencia de `T`, en
  todas las estructuras que la usan (no solo en su clase); los
  miembros compartidos (`P` en cA+cB) la exigen uniforme.

## Mapa de ficheros

- `memory.pl` — memoria de tripletas con peso/usos.
- `experiment.pl` … `experiment18.pl` — experimentos (punto de entrada
  `experiment`, `experiment2`, …).
- `correspondence.pl`, `variable_induction.pl`, `composition.pl`,
  `multivariable.pl`, `reuse.pl` — descubrimiento (correspondencia,
  puentes, caminos, firmas de igualdad).
- `abduction.pl`, `identity.pl`, `coreference.pl` — hipótesis, identidad,
  pronombres.
- `question_parser.pl` — preguntas (directa/inversa/sí-no/por-qué) con
  prueba: `retrieved` (hecho) vs `reasoned` (regla + cadena).
- `conflict.pl` — creencias con estado (provenance: fuente, tiempo,
  `active|contested|superseded`) y revisión temporal.
- `guided_search.pl` — vocabulario incidente + filtro de extremos
  (podas sólidas: un patrón con soporte>0 nunca se poda).
- `hierarchical.pl` — biblioteca de skills: reglas como unidades de
  búsqueda sin expandir sus internos.
- `corpus.pl` — sistema v0.1: `learn_sentence/file/corpus`,
  `learn_cycle(Targets)`, `ask/why`, `save/load_knowledge`,
  `knowledge_stats` (ingesta separada del aprendizaje).
- `preflight.pl` — ritual estático: sintaxis, aridad, dinámicas,
  indefinidas, consults; más `preflight_project/1` (unión del proyecto)
  y regla `findall_template` (EXP36: toda variable nombrada de la
  plantilla `findall/bagof/setof` debe aparecer en el objetivo;
  solo cuerpos de reglas, las anónimas `_` están exentas).
- `run_corpus1.pl` + `corpus1/` + `make_corpus1.py` —
  Corpus 1 piloto (150 frases, 10 held-out; `longterm_c1.pl` generado).
- `experiment28.pl` + `make_corpus2.py` + `corpus2/` + `heldout2.pl`
  + `distractor2.pl` — Corpus 2 por bloques con tabla marginal
  (evaluaciones/fact) y control negativo en el bloque de ruido.
- `experiment29.pl` + `make_corpus3.py` + `corpus3/` — Corpus 3:
  transferencia a vocabulario y relaciones inéditos, test oculto con
  proof atribuida (meta vs inducida) y distractores verificados.
- `positional.pl` — simbolizador sin lexicón (SVO estructural puro).
- `frame_align.pl` — eventos reificados, merge por participantes,
  alineación de marcos, queries cross-frame (sin `findall` en rutas
  de respuesta: copiar desconecta variables).
- `experiment30.pl` + `make_corpusA.py` + `corpusA/` + `secret/` +
  `heldoutA.pl` + `distractorA.pl` — primer corpus natural con parser
  posicional y test secreto temporal (retrieve/reason/unknown).
- `experiment33.pl` — POO emergente: clases por SSE, skill `reaches`
  adherida a CLASS_A y heredada por objetos nuevos (`wex` → UNKNOWN).
- `experiment34.pl` — separación estructura/comportamiento: misma
  evidencia, dos regímenes de firma (con/sin `reaches`); el SSE
  representa lo sabido ANTES de la inferencia a predecir.
- `experiment35.pl` — SSE por tarea (`sse_excluding/3`): un objeto, una
  firma distinta por objetivo, cero átomos, sin fuga; `reaches` sale
  por regla con prueba, no por la firma.
- `experiment36.pl` — SSE estructural: estructuras `{P owns O,
  O belongs_to P, P visits L}` como unidad; `stocked :-
  [belongs_to, visits]` adherida al concepto de estructura y heredada
  en vocabulario nuevo (`s3`), distractores por arista ausente/extra,
  UNKNOWN sin estructura.
- `experiment37.pl` — composición de conceptos: cA (suministro) + cB
  (cocina) compartiendo `P`; `cater(P,L)` sobre conceptos + sub-skills
  (`stocked`, `cuisine`), sin volver a hechos crudos; `wex`/`yago`
  parciales → UNKNOWN; exclusión uniforme y temporal.
- `question_parser.pl` — preguntas con prueba; `longterm21.pl` es la regla
  `reaches` exportada que EXP21 recarga tras borrar los hechos.
- `continuous.pl`, `meta_pattern.pl`, `rule_instantiation.pl` —
  incrementalidad, segundo orden, persistencia (`longterm.pl`,
  `longterm14.pl` son salidas de ejemplo regenerables).
- `language_graph.pl` (ES), `english_graph.pl` (EN), `open_vocab.pl`
  (abierto + pronombres) — simbolizadores.
- `equivalence.pl` — exploratorio (perfiles exactos), superado por
  `identity.pl` en EXP17; se conserva por trazabilidad.

## Límites honestos

Léxico cerrado con tipos (salvo EXP15/16: abierto sin tipos inventados);
sin correferencia encadenada entre pronombres; sin tiempo; sin vocabulario
abierto de verdad (palabras nuevas fuera del lexicón no parsean);
longitudes de camino acotadas (cota 3–4 por coste combinatorio);
funcionalidad y completitud descritas por escenario (mundo parcialmente
observado puede subdeterminar veredictos).

## Corpus 1 (piloto, 150 frases EN)

```text
BEFORE:      0 hechos, 0 símbolos
ingesta:     150/150 hechos (cobertura total, 0 rechazadas)
AFTER:       150 hechos, 96 símbolos, 64 unknowns, 4 conceptos, 5 reglas
held-out:    10/10 razonado (nunca en corpus) + 10 distractores TN
```

Escala honesta: la rejilla de composición crece cuadráticamente con las
entidades; el piloto valida el bucle end-to-end (el Corpus 2 usará la
maquinaria guiada de EXP23 para escalar).

## Corpus 2 por bloques (EXP28, 345 frases EN en 4 bloques)

```text
b1: guided 39 gen / 11 eval  (exhaustive 39: igualdad verificada)
b2: guided 155 gen / 15 eval (exhaustive contaría 258)
b3: guided 84 gen / 7 eval   (exhaustive contaría 399)
b4: ruido owns/likes: 0 reglas nuevas (control negativo)
held-out 15/15 razonado + 15 distractores TN (verificados falsos)
total: 33 evals / 345 hechos = 0.096 evals/fact
```

## Regla de módulos (pagada con sangre)

Delegar y añadir predicados nuevos; **jamás reabrir predicados estáticos
de otro fichero** (SWI-Prolog borra silenciosamente el original:
nos costó `detect_verb/2` y una recursión infinita en `person/1`).
