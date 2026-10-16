# Gold propuesto antes del parche

1. shorter_not_cheaper: initial00, goal10. Direct(pre0,add10,del0,cost3); toM(pre0,add01,del0,cost1); fromM(pre01,add10,del0,cost1). Esperado toM/fromM,2pasos,coste2. Ningún coste<2 alcanza goal: solo direct y fromM añaden goal, fromM necesita toM.
2. visited_blocks_cheaper: initial0,goalbit1. expensive(pre0,addgoal,cost3) índice0 y cheap igualcost1 índice1. Esperado cheap,coste1. Invertir orden siguecoste1.
3. already_goal: planvacío,coste0.
4. unit_cost_control: directcost1 vs toM/fromM1+1. Esperado direct,1paso,coste1.
5. zero: noopcost0 + zerodirectcost0 + positivedirect1. Esperado zerodirect,1paso,coste0; no ciclo infinito ni512labels por repetir noop.
6. uint32sum: directcostUINT32_MAX vs toMcostUINT32_MAX-1 y fromMcost10. Esperado directcost4.294.967.295, no aceptar wrapde8. Costes públicos sin cambiar.
7. tiecost: directcost2 vs dos1+1. Esperado direct2 por menos pasos. Otro par de direct iguales: índiceinferior.
8. depthtrap: dualrail5bits; inicialmente bitsOFF=1 y ON=0. Inc_i requiere OFF_i y ON_j para j<i; efecto activa ON_i/OFF_j y borra OFF_i/ON_j. shortcut requiere todosOFF, pone número16(cost100). Goal todosON. DP hasta16: coste115,16pasos; hasta15: inalcanzable. Control sin shortcut hasta16 inalcanzable; hasta31 coste31.
9. pasos16/17: fijar mediante contador dualrail el goal16(cost16) y17(cost17),sinshortcut: primero16stepsválido, segundo no alcanzable dentro16. No usar17operadores y violar límite público.
10. resourcecap ejecutable: directgoal sin precondiciones, añade bit10,coste1000,índice0. Diez operadores añaden independientemente bits0..9,coste1. Goal bit10. OracleDP coste1000; existen1024estados distintos sin goal de coste<=10 y profundidad<=10, todos antes de extraer goal1000. Una búsqueda que conserve la frontera necesitaría más de512labels antes de certificar. Esperado return0 al agotar; no devolver goal truncando. BaselineBFS devuelve1/coste1000, FAIL del contrato nuevo de recursos, no nuevo bug de coste del baseline.

Oracle de referencia para futuros tests: DP exact-depth independiente, `best[d][s]`, sin cola prioritaria ni pruning de dominancia; cost64 y enumeración de operadores. Fixtures pequeños permiten estados enumerados8/32/1024 con cotas claras. Reconstruir acciones con desempate fijado solo después del coste esperado. Exhaustivo3bits en registros pequeños más randomdeterminista si necesario; jamás ajustar el gold a salida del productor.

Repro ejecutado: casos1/2rojos,3/4y barata-primero verdes O2 y ASAN+UBSAN. gold-extra.c ejecuta5-10 mediante DP exactdepth de17x2048cost64. Casos5-9PASS baseline;10FAIL esperado por nuevo contrato de recursos. Profundidadtradeoff115/16 y16/17 confirmados por DP. No son test de implementación corregida. Dirección aprobada por parent18:00:12, fixturecap antes del código; parche pendiente.
