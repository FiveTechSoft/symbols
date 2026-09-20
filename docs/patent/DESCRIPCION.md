# DESCRIPCIÓN

## TÍTULO DE LA INVENCIÓN
SISTEMA DE INTELIGENCIA ARTIFICIAL SIMBÓLICA DETERMINISTA Y MÉTODO ABDUCTIVO DE BUCLE CERRADO PARA SÍNTESIS, VERIFICACIÓN Y AUTOCURACIÓN DE SOFTWARE

## SECTOR DE LA TÉCNICA
La presente invención se encuadra en el sector de la inteligencia artificial, la síntesis automatizada de programas, el análisis de diagnósticos de compiladores y las herramientas de ingeniería de software. Específicamente, se refiere a un motor de ejecución simbólica no neuronal configurado para realizar generación autónoma de código, análisis de impacto multifichero, abducción diagnóstica de compiladores y autocuración atómica sin retropropagación ni predicción probabilística de tokens.

## ESTADO DE LA TÉCNICA
Los enfoques contemporáneos de inteligencia artificial para programación se basan predominantemente en Modelos Masivos de Lenguaje (LLMs) basados en arquitecturas neuronales Transformer. Dichos modelos presentan limitaciones críticas:
1. Alucinación semántica e identificadores inventados.
2. Ausencia de verificación determinista con el compilador del sistema.
3. Bloqueos de procesos e interbloqueos en tuberías de ejecución.
4. Degradación o corrupción de repositorios al fallar las pruebas.
5. Elevada latencia y consumo intensivo de recursos de cálculo.

## EXPLICACIÓN DE LA INVENCIÓN
La invención proporciona un agente de programación simbólica determinista que opera nativamente en procesadores estándar (ISO C11 puro) con cero operaciones tensoriales y ausencia estricta de alucinación.
Comprende un Grafo de Conocimiento de Código AST políglota para análisis de radio de impacto en tiempo O(1), un motor de subprocesos asíncrono multiplataforma con drenaje no bloqueante de tuberías independientes para stdout y stderr, un motor de diagnóstico abductivo que deduce reparaciones a partir de los errores del compilador y una compuerta de verificación con reversión atómica en menos de 0,001 segundos.

## BREVE DESCRIPCIÓN DE LOS DIBUJOS
- FIG. 1: Diagrama de bloques de la arquitectura general del sistema.
- FIG. 2: Diagrama de flujo del método de autocuración abductiva.
- FIG. 3: Esquema del motor de subprocesos con drenaje asíncrono y temporizador.
- FIG. 4: Grafo de cálculo del radio de impacto y estratificación de riesgo.
- FIG. 5: Diagrama de estados de la transacción de parche y reversión atómica.

## REALIZACIÓN PREFERENTE DE LA INVENCIÓN
El sistema comprende una capa de almacenamiento de conocimiento (CODE_GRAPH, TEXTLEX, GRAPH), un planificador clásico STRIPS con vectores de estado de precondiciones y efectos, un motor de diagnóstico que clasifica errores de GCC, Clang y MSVC deduciendo símbolos mediante el grafo de código, y un motor de subprocesos que ejecuta shells nativos (cmd, powershell, bash, zsh) con terminación forzosa por tiempo límite. La aplicación de modificaciones está protegida por verificación previa de contexto único y reversión automática ante códigos de salida distintos de cero.
