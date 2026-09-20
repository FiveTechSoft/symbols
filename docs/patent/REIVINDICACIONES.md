# REIVINDICACIONES

1. Un método implementado por ordenador para la síntesis autónoma y autocuración en bucle cerrado de software, comprendiendo el método:
   - mantener en memoria un grafo de conocimiento de código AST determinista que representa funciones, estructuras, dependencias de llamadas y relaciones de inclusión de una base de código multifichero;
   - generar, mediante un planificador determinista, un plan de parche quirúrgico dirigido a un fichero fuente en almacenamiento persistente;
   - realizar una verificación previa de ambigüedad en dicho fichero fuente, rechazando dicho plan de parche si un patrón de código coincide con múltiples ubicaciones en dicho fichero;
   - aplicar dicho plan de parche quirúrgico de forma atómica a dicho fichero fuente;
   - ejecutar una orden de verificación externa dentro de una interfaz de comandos nativa del sistema operativo mediante un motor de subprocesos asíncrono de doble tubería que drena independientemente los flujos de salida estándar y error estándar;
   - al detectar un código de salida distinto de cero de dicha orden de verificación:
     - procesar dicho flujo de error estándar mediante un motor de diagnóstico abductivo para clasificar un defecto de compilador o ejecución en una categoría taxonómica formal;
     - revertir automáticamente dicho fichero fuente a su secuencia exacta de bytes previa al parche en almacenamiento persistente; y
     - formular dinámicamente un plan de parche revisado basado en dicha clasificación abductiva.

2. El método de la reivindicación 1, en el que dicho motor de subprocesos ejecuta un sondeo no bloqueante de tuberías en rodajas de tiempo para evitar bloqueos por buffer, y termina dicho proceso hijo al alcanzar un umbral de tiempo límite de precisión milimétrica con estado de salida 124.

3. El método de la reivindicación 1, en el que dicho grafo de conocimiento calcula un radio de impacto que comprende la clausura transitiva de todos los llamadores directos e indirectos afectados antes de aplicar modificaciones al disco.

4. El método de la reivindicación 1, en el que dicho motor de diagnóstico abductivo consulta dicho grafo de conocimiento para identificar un fichero de cabecera faltante ante un diagnóstico de símbolo no declarado emitido por el compilador.

5. Un sistema determinista para ingeniería autónoma de software que comprende uno o más procesadores y una memoria que almacena instrucciones en lenguaje C configuradas para ejecutar el método de la reivindicación 1 sin inferencia de redes neuronales y sin aproximaciones tensoriales de coma flotante.
