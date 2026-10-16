# Planner: corregir objetivo de coste, no cambiar ejecución

Base de solo lectura cbe8f149bf4595aa4076bd6406f2cf46bff5a6ba. Diseño propuesto, pendiente de aprobación. No parche de producción.

## Repro previo conservado

repro.c enlaza agent_planner.c original, con header público real. Dos fallos medidos O2 y ASAN+UBSAN local: directo coste3 frente 1+1=2; visited de estado caro3 bloquea llegada barata1. Tres controles verdes. Test existente40/40verde. Son registros custom legales, no fallo demostrado de los8operadores default. Gold exhaustivo hasta3pasos sobre fixtures cuya solución óptima tiene<=2; aritmética diminuta, sin saturación ni ambigüedad.

## Contrato propuesto

AgentPlannerFormulate busca mínimo coste entre planes legales de hasta MAX_PLAN_STEPS=16; desempate: menos pasos y luego secuencia lexicográfica de índices de operador. Ya satisfecho: plan vacío. Sin solución demostrable con recursos: return0, no etiquetar un plan no certificado como óptimo. No cambiar contratos de AgentPlanAdvance, ReplanOnError, tool IDs ni ejecución real. Los predicados describen el modelo, no pruebas del mundo exterior.

Aritmética interna uint64_t para coste: 16*UINT32_MAX=68.719.476.720 cabe. No tocar STRIPS_OPERATOR.cost ni la ABI pública. Coste cero legal; costes negativos no representables. No añadir heurística aprendida, red neuronal ni prioridad por nombre.

## Búsqueda y profundidad

Uniform-cost/Dijkstra acotado con labels (estado, pasos), no visited bool por estado. Un estado igual con menor coste pero mayor profundidad puede no dominar: aún necesita presupuesto para terminar. Mantener labels no dominados por coste y profundidad. Para dominancia exigir coste<= y pasos<=; en igualdad de ambos, conservar la secuencia lexicográficamente menor. El estado incluye todos32bits de las máscaras.

Estructura interna de hasta512labels: state, pasos, coste64, secuencia de<=16índices, estado de actividad/settled. Elegir siguiente label por (coste,pasos,secuencia). Relajar sucesores con precondiciones exactas y efectos originales. Prunar labels dominados sin reutilizar o sobrescribir una ruta referenciada: preferir secuencia completa en cada label, como hoy, evitando ambigüedad de predecessor mutable. La implementación concreta debe evaluarse contra este contrato antes de aprobar patch.

Con costes no negativos, cuando se extrae el objetivo mínimo, su orden es óptimo respecto a la frontera pendiente y futuras expansiones, siempre que ninguna frontera válida se haya perdido. Si se intenta crear un label no dominado y ya no hay espacio512, retornar0: no seguir y anunciar optimalidad. Se puede aceptar un objetivo ya extraído antes de alcanzar el límite; no cambiar los máximos ni asignar memoria sin cota. Evitar convertir overflow de recursos en "no existe plan" en texto nuevo.

El escaneo lineal del próximo label evita dependencia de heap y mantiene parche pequeño; comparación y dominancia O(512) por expansión. Medir en fixtures para evitar regresión grande; no prometer mejora de velocidad sobre BFS. Revisión independiente debe cuestionar dominancia, desempates y límites.

## Gold primero

GOLD.md fija expectativas por matemática y una referencia DP independiente por (estado,profundidad), antes del código. Añadir a tests/test_agent_planner.c o un test independiente según build actual, sin tocar oracles existentes. Gates nuevos deben matar: FIFO viejo, visited por estado, selección coste32 que hace wrap, greedy por coste de última arista y comparación que omite profundidad.

Tests mínimos: dos rojos actuales, tres controles, coste cero/noop/ciclo, coste UINT32_MAX con suma>32bits, empate coste y pasos, empate por índices, frontera16/17, contraprueba de dominancia por profundidad y agotamiento real512. Cada resultado debe replayearse y comprobar precondiciones/goal/suma64. Oracle independiente no debe copiar selección ni pruning del productor.

Contraprueba de profundidad realizable con6operadores: contador binario de5bits dual-rail(10predicados), cinco operadores incrementan con carry; shortcut de0a16 cuesta100. Incrementos cuestan1. Goal31. Ruta barata sin shortcut tarda31pasos, fuera de16. La válida es shortcut+15incrementos=16pasos coste115. Un Dijkstra con visited/dist solo por estado puede bloquear la llegada corta cara a16 por la barata de profundidad16 y fallar. Gold debe verificar el fixture de bits y fórmula, no confiar en esta descripción.

## Secuencia

1. Revisión y aprobación de diseño+gold.
2. Slice test primero, rojos esperados conservados y controles verdes contra baseline; puede ser local antes de dispatch combinado si parent lo aprueba expresamente.
3. Parche mínimo separado de producción; revisar SHA exacto antes de dispatch. Actualizar docs del contrato solo con evidencia.
4. Readback código/gold; tests locales y MSVC Release+ASAN con checkout real verificado y gates sin SKIP. No cerrar con solo Linux ni ocultar rojos. Repetición solo si autorizada.
5. Handwired aislado después. No master para Handwired hasta que Antonio vea el diseño.

## Fuentes

https://raw.githubusercontent.com/FiveTechSoft/symbols/cbe8f149bf4595aa4076bd6406f2cf46bff5a6ba/src/agent_planner.c
https://raw.githubusercontent.com/FiveTechSoft/symbols/cbe8f149bf4595aa4076bd6406f2cf46bff5a6ba/include/agent_planner.h

## Evidencia previa al patch final

Dirección aprobada por parent18:00:12. Fixturecap512 ya ejecutado antes de producción:10bits independientes cost1+directgoal1000,1024labels distintos antes de extraer goal. gold-extra DP17x2048 da1000 y capcontrato exige0. Baseline8extras7PASS/1FAIL, dosreproFAIL, controlesPASS, suiteoriginal40/40. Fixed local13/13fixtures,5000registriesDP/replay/cost/steps/lex,43/43asserts integrados y ASAN+UBSANgreen. FullbuildLinux-j1 y20targetedPASS. Ocho mutantes privados matados con controles verdes: coste32,visitedsoloestado,FIFO,depth15,últimocoste,capcontinue,lexreverse,depthpriority. No MSVC aún ni equivalencia global demostrada. Añadido highbituint32control al testintegrado después de esa corrida; verificar antes de patch.
