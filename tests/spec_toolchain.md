# Tool Chaining (ESPEC CERRADA)

Mide si el diseño se comporta como agente: el resultado de A
modifica/resuelve B, con `ExecutionContext` y proveniencia, sin
contaminar el KB.

## 1. Memoria de ejecución (no KB)

```c
ExecCtx { entities[], number, prov[] }  // en CHAT, por query
```

- Entidades/números fluyen entre metas; `prov[]` responde de
  dónde salió cada dato (`KB ANSWER`, `TOOL lookup_person(jesse)`,
  `MISS`, `ECHO`).
- El corpus KB nunca se escribe (fixtures de tools tampoco).

## 2. Anáfora de cadena (posicional, sin palabras)

Token en posición de sujeto (tras cópula del set congelado),
fuera de vocab/kw/stop/dígitos:

- la meta menciona dígitos → contexto numérico (`eso` → 391);
- si no → última entidad (`he` → Jesse);
- sin contexto → intacto (fail-closed).

## 3. Reglas de split finales (cero palabras nuevas)

- Droppable: no-vocab ∧ no-kw-deducida ∧ no-delimitador.
- L: trial-parse o tool-route, con final no-stop.
- R: abre con de/of, wh congelado, contenido simple, anáfora
  contenida, o cópula con kw propia.
- Fallback-capture vetado si sigue cópula (sujeto ≠ argumento).
- WHY/COMPOSE nunca se parten (veto por intent).

## 4. Batería (`tests/battery_chain.txt`, 3 filas)

- `23 * 17 y eso + 9` → `391` fluye → `400` (tool→tool).
- `father(David) + where was he born` → Jesse fluye →
  `lookup_person` → Bethlehem (KB→tool).
- `father(Babylonia) + ...` → miss honesto → echo.

## 5. Métrica

Cadenas figuradas íntegras + proveniencia verificada +
`test_chain` 7/7 + matriz 54/54 + 0 regresiones byte-idénticas
(Fase 4, A, C, Clarify, unknown-diag).
