# Fase A — QueryPlan multi-meta (ESPEC CERRADA)

Diagnóstico que la motiva: batería A 11/11 — 8× PARSE_PARTIAL,
3× ABSTAIN_CORRECT, 0× PARSE_CORRECT, 0× PARSE_WRONG. El parser
actual nunca separa (A1 imposible): toma el primer `de/of` y
resuelve solo la primera meta **en silencio**. Una extensión
pequeña (A2, multi-slot) cubriría la coordinación de argumentos
pero no la de metas (dos `kw` distintos, huecos anafóricos,
slots huérfanos). Decisión medida: **A3, QueryPlan explícito**.

## 1. Pipeline

```text
INPUT
  ↓
CanonicalizeQuery          (Fase 4, intacta)
  ↓
QueryPlan
  ├── Goal 1
  ├── Goal 2
  ├── ...
  └── Goal N
       ↓
   QUERY → SET             (primitiva Fase C, reutilizada)
       ↓
   GoalResult
       ↓
CompositeResponse
       ↓
NLG                      (mínimo estrictamente necesario)
```

Orden de construcción: representación → evaluación (frames
existentes) → NLG. No duplicar el motor que ya funciona.

## 2. Contrato de `QueryPlan`

```c
typedef struct {
    SymbolID relation;
    SymbolID subject;
    SymbolID object;
    uint8_t  has_subject;
    uint8_t  has_object;
} QueryGoal;

typedef struct {
    QueryGoal goals[MAX_QUERY_GOALS];
    uint32_t count;
} QueryPlan;
```

- Cada meta conserva su propia estructura; **no fusionar
  resultados entre metas**.
- Ligadura C: `relation` = índice de familia deducida (`REL_KW`,
  nunca literal); subject/object = tokens canónicos post-Fase 4;
  el orden canónico por `SymbolID` se define en la fase del
  wrapper (B). Hasta entonces, el orden observable es el de
  ingest (determinista para corpus congelado).

## 3. Casos obligatorios

Coordinación de argumentos →
`father(David)` + `father(Solomon)`:

```text
¿Quién es el padre de David y de Salomón?
```

Coordinación de metas →
`father(David)` + `king_of(?X, David)`:

```text
¿Quién fue el padre de David y de quién fue rey?
```

Dos preguntas completas → dos `QueryGoal` independientes:

```text
¿Quién es el padre de David y quién es el padre de Salomón?
```

Pronombre/anaphora (`he`): resolverse vía focus/contexto; si no
es resoluble de forma segura, **esa meta es UNKNOWN**, nunca
descarte silencioso:

```text
Who was David's father and who was he king of?
```

## 4. Regla crítica

Prohibido:

```text
meta1 → ANSWER
meta2 → desaparece
```

Obligatorio, una de:

```text
meta1 → ANSWER + meta2 → ANSWER
meta1 → ANSWER + meta2 → UNKNOWN
```

## 5. Resultado formal

```text
GoalResult {
    status = ANSWER_SET | AMBIGUOUS_SET | UNKNOWN
    candidates[]
    proof
}

CompositeResponse {
    results[N]
}
```

NLG por meta, sin contaminación cruzada:

```text
Padre de David = Jesse.
Padre de Salomón = David.
```

```text
Padre de David = Jesse.
No tengo constancia suficiente para responder a "de quien fue rey".
```

La meta sin parse/arg se figura con span-echo (las palabras propias
del span), sin vocabulario de ordinales.

## 6. Batería de cierre (`tests/battery_faseA_coord.txt`)

14 casos: los 11 diagnósticos + meta desconocida tras conocida
+ meta ambigua tras conocida + mezcla argumentos/metas. Cubre:
coordinación de argumentos con `y`, coordinación de metas con
`y`, dos preguntas completas, coma, `and`/`y`, pronombre `he`,
slot huérfano `of`, fragmento sin fuerza → ABSTAIN.

## 7. Métrica

```text
META_RECALL = metas recuperadas / metas presentes
```

Baseline medido: **11/29 ≈ 38%** (8/22 en los 11 canónicos).
Cierre medido: **25/29 segmentadas + 4 en ABSTAIN íntegro
(rows 5-6, fragmento sin fuerza) = 29/29 figuradas, 0 pérdidas
silenciosas**. Aunque alguna termine en UNKNOWN. Además:
0 falsas separaciones, 0 alucinaciones, UNKNOWN independiente
por meta. `tests/battery_faseC_sets.txt` intacto como contrato
QUERY→set.

## 8. Decisiones de implementación (congeladas)

- Split 100% deducido, ninguna palabra coordinativa nombrada:
  droppable = no-vocab ∧ no-kw-deducida ∧ no-delimitador
  (`de/of/'s`) ∧ no-artículo/disyunción (roles congelados);
  selección = trial-parse del span izquierdo + viabilidad del
  derecho + recursión con kw heredada. ES-2, R7, R9 y aposiciones
  colapsan a meta única por construcción.
- Veto cross-clausal: una línea que parsea como WHY/COMPOSE es
  una unidad de discurso y nunca se parte (detectado por intent,
  no por palabras; `test_compose` 31/31 intacto).
- `wh` inglesas al stop-set funcional (colisión vocab 0/674):
  un `who` capturado como slot es veto G2 (evita que el trial
  rehidratado figure metas fantasma).
- NLG existente intacta vía buffer + status por meta; el fix
  `char out[128]`→`proof` en COMPOSE elimina un shadowing que
  rompía el buffer (fail-closed verificado por ejecución).
