# Changelog

Todas las novedades, mejoras y correcciones notables de **Symbolic LLM / symbols-server** quedan documentadas en este archivo.

El formato se basa en [Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/).

## [Phase 25] - 2026-09-21
- **Tolerancia a Errores Tipográficos y Sintagmas Nominales en Síntesis de Código (`ServerIsCodeSynthesisTask`)**:
  - **Detección Difusa / Levenshtein de Algoritmos (`EditDistance`, `MatchesAlgorithmKeyword`)**:
    - Incorporada distancia de edición acotada ($\le 2$) y coincidencia de prefijos en algoritmos clásicos (`fibonacci`, `factorial`, `quicksort`, etc.).
    - Consultas con erratas tipográficas comunes como `funcion fibinacci en C`, `fibonaci`, `facturial`, etc. se reconocen de forma robusta como solicitudes de síntesis de código en lugar de derivarse erróneamente al planificador de parches de repositorio.
  - **Soporte para Sintagmas Nominales sin Verbo Imperativo**:
    - Consultas estructuradas como `[sustantivo de código] + [especificación de lenguaje]` (p. ej. `funcion ... en C`, `algoritmo de ordenamiento en C`, `busqueda binaria en c`, `ejemplo de punteros en C`) se clasifican directamente como síntesis de código, eliminando el requisito excluyente previo de incluir un verbo imperativo (`escribe`, `haz`).
    - Se previene la formulación errónea de planes STRIPS con herramientas de modificación (`CMakeLists.txt`, `cmake --build`, `ctest`) ante peticiones informativas de funciones de programación.
  - **Verificación Completa**:
    - Pruebas unitarias de discriminación en `tests/test_server_tool_calling.c` (226/226 PASS).
    - Prueba E2E `test_fibinacci_typo_synthesis` en `tools/test_opencode_user_cases.py` verificando respuesta Markdown inmediata con `finish_reason: "stop"` y 0 llamadas a herramientas (100% PASS).
    - Suite global CTest 76/76 verde (67 Passed, 9 Skipped, 0 Failed).

---

## [Phase 24] - 2026-09-21
- **Protección y Limpieza de Inspección de Archivos y Directorios (`dir *.*`, `glob`, `read`, `grep`)**:
  - **Eliminación de Falsos Positivos de Error en `ServerInspectToolResponse`**:
    - Corregida la verificación del campo JSON `"status"`: ahora inspecciona estrictamente el valor del campo tras los dos puntos (`"error"`, `"fail"`), evitando que respuestas exitosas con campos posteriores como `"error": null` o `"error": false` activen falsamente `is_error = 1`.
    - Detección precisa de campos de error JSON (`"isError": true`, `"is_error": true`, `"error": "<mensaje>"`), ignorando valores neutros (`null`, `false`, `""`, `0`).
    - Detección segura de códigos de salida numéricos en JSON (`"exit_code"`, `"returncode"`, `"exitCode"`, etc.) exigiendo dígitos o signo entero tras `:`, previniendo colisiones con campos de texto como `"code": "printf(...)"`.
  - **Inmunidad para Herramientas de Inspección y Lectura de Código**:
    - Herramientas de solo lectura (`glob`, `read`, `grep`, `locate_symbol`, `view_file`, `find_by_name`, `grep_search`) quedan exentas del escaneo por patrones de error de compilador o de pruebas (`DiagnosticParseOutput` y cadenas como `error:`, `FAILED`, `Permission denied`), impidiendo que rutas de archivo o código leído conteniendo esas palabras disparen bucles de replanificación o fallos espurios de verificación.
  - **Extracción de Contenido Multiformato en `ServerExtractLastToolResponse` (`TakeJsonContent`)**:
    - Soporte transparente para contenidos de herramientas tanto en cadena de texto plano (`"content": "..."`) como en arrays de partes (`"content": [{"type": "text", "text": "..."}]`) o listas JSON crudas de coincidencias (`["file1", "file2"]`).
  - **Formateo Limpio de Salida de Exploración (`ServerFormatInspectionOutput`)**:
    - Formateo automático de arrays JSON de coincidencias (`"matches": [...]`, `"files": [...]` o listas) en líneas limpias de texto delimitadas por saltos de línea dentro del bloque markdown `### Contenido del Directorio / Exploracion`, completando con `finish_reason: "stop"` sin errores ni advertencias de verificación fallida.
  - **Inferencia de Nombre de Herramienta en Sesión Agéntica**:
    - Almacenamiento de `last_tool_call_name` en la sesión del servidor, permitiendo identificar la herramienta ejecutada incluso si el cliente OpenCode omite la clave `"name"` en el mensaje `{"role": "tool"}`.
  - **Verificación Completa**:
    - Nuevas pruebas 6f, 6g, 6h, 6i en `tests/test_server_tool_calling.c` (221/221 PASS).
    - Dos nuevas pruebas end-to-end en `tools/test_opencode_user_cases.py` (`test_dir_wildcard_flow` y `test_dir_dot_flow`) ejecutadas contra el servidor HTTP en vivo (100% PASS).
    - Suite global CTest 76/76 verde (67 Passed, 9 Skipped, 0 Failed).

---

## [Phase 23] - 2026-09-21
- **Configuración Predeterminada de Corpus Técnico para Copiloto de Programación**:
  - `data/c_lang/c_corpus.txt` (estándar C11, memoria dinámica, tipos e invariantes de libc) establecido como corpus de conocimiento predeterminado en `symbols-server` y `chat_main`.
  - Actualizados los scripts de lanzamiento (`run_symbols_server.bat`, `run_symbols_server.ps1`, `run_symbols_server.sh`) para arrancar con el corpus de C11 por defecto.
  - Los textos bíblicos (`bible.txt`) y de psicología analítica (`jung.txt`) se desvinculan del arranque predeterminado, preservándose intacta la capacidad de carga explícita mediante argumento en línea de comandos o vía `/load`.
  - Las consultas técnicas como *"What causes memory leaks?"* se resuelven directamente con las directrices de C11 (`"Failing to free allocated memory causes memory leaks that exhaust available system resources."`), sin riesgo de secuestro léxico o respuestas anacrónicas.
  - Aislamiento de estado en peticiones HTTP individuales (`nmsg <= 1`): reseteo automático de diálogo y oraciones ya mostradas (`ntshown = 0`), evitando contaminación cruzada entre clientes stateless.
  - Wrap-around determinista en `src/chat.c` (`INT_TEXTQ`): cuando todas las oraciones candidatas han sido emitidas en un diálogo y se recibe una consulta directa (`!p->t_following`), reinicia el historial de oraciones en lugar de responder "No entendi la pregunta".
  - Validación completa con CTest (67 Passed, 9 Skipped, 0 Failed de 76 tests), 212/212 pruebas unitarias de protocolo agéntico y 100% éxito en los harnesses de prueba OpenCode (`test_opencode_user_cases.py` y `test_opencode_copilot_e2e.py`).

---

## [Phase 22] - 2026-09-21
- **Saludos Conversacionales, Identidad y Supresión de Falsos Positivos Levenshtein (`ServerIsGreeting`, `ServerAnswerGreeting`, `IsGreetingTok`)**:
  - Blindaje léxico en `src/chat.c`: `IsGreetingTok` evita que tokens de cortesía (`hola`, `hello`, `hi`, `hey`, `buenas`, `saludos`) sean asimilados por distancia Levenshtein a términos bíblicos (`hold`), eliminando respuestas fuera de contexto.
  - Interceptor prioritario de saludos e identidad en `symbols-server` y `server_proto`: responde al instante en lenguaje natural ("¡Hola! Soy Symbols, tu copiloto local de IA y desarrollo...") con `finish_reason: "stop"`, tanto en español como en inglés, y con modulación por perspectiva/persona.
  - Fallback compilado y tabla declarativa externa (`COMPILED_SELF_SCOPE`, `COMPILED_SELF_GREET`, `data/agentic/self.tsv`) con 20 disparadores de saludo y presentación.
  - Priorización de `SelfAnswer` en `ChatHandleToBuf` e incorporación de guardia `IsGreetingTok` en `ParseIntentToks`.
  - Fallback compilado `LoadCompiledSelf()` con 20 disparadores canónicos y persistencia declarativa en `data/agentic/self.tsv`.

---

## [Phase 21] - 2026-09-21
- **Síntesis Directa de Código Algorítmico (`ServerIsCodeSynthesisTask`, `ServerSynthesizeCode`)**:
  - Clasificación de peticiones de síntesis de algoritmos y funciones en lenguaje C (p. ej. `escribe en C la funcion de fibonacci`, `write a function to calculate factorial`, `invertir cadena`, `busqueda binaria`).
  - Generación directa de código C11 idiomático formateado en bloques Markdown con explicaciones de complejidad temporal/espacial y verificaciones de desbordamiento, completando la respuesta con `finish_reason: "stop"`.
  - Se evita la activación del bucle STRIPS de modificación de archivos (`patch -> build -> ctest`) para consultas de generación de código que no modifican el repositorio del usuario.
- **Decodificación Robusta de Cadenas JSON (`TakeJsonString`)**:
  - Soporte para secuencias de escape estándar en cadenas JSON (`\"`, `\\`, `\/`, `\b`, `\f`, `\n`, `\r`, `\t`), impidiendo truncamientos prematuros al procesar payloads con código fuente entrecomillado.
- **Extracción y Transporte HTTP Robusto (`FindHttpHeader`, `ReadHttpBody`)**:
  - Búsqueda genérica e insensible a mayúsculas/minúsculas de cabeceras HTTP (`FindHttpHeader`).
  - Recuperación segura del cuerpo HTTP en `ReadHttpBody` cuando el contenido ya fue recibido en el búfer inicial de cabeceras o ante desconexiones tempranas con JSON completo ya recibido.

---

## [Phase 20] - 2026-09-21
- **Soporte universal para relaciones genéricas en modelos binarios (`GenericRelToConn`, `ChatLoadModel`)**:
  - Eliminada la barrera de relaciones fijas: cualquier relación arbitraria o personalizada (p. ej. `contains`, `part_of`, `requires`, `kit_contains`, `component_of`, `directed_by`, etc.) se normaliza de forma automática y composicional a conectivos naturales reconocibles por el motor de QA sin perder semántica (`HARDCODING=0`).
  - Mapeo declarativo de sinónimos canónicos frecuentes en `COMPILED_RELMAP` (`CONTIENE`, `CONTAINS`, `PART_OF`, `PARTE_DE`, `REQUIRES`, `REQUIERE`, `COMPONENT_OF`, `COMPONENTE_DE`, `GENTILICIO`).
  - Las relaciones ya no se descartan en silencio: 100% de las tripletas válidas del grafo entran al motor de preguntas y respuestas (`ChatFactCount > 0`).
  - Normalización de entidades compuestas: los espacios en nombres de símbolos se convierten en guiones bajos para garantizar sintaxis unívoca en `LearnerLearnLine`.
  - Ingesta coordinada en el grafo semántico de sesión (`ch->tgraph`) mediante `IngestTripleSource` para habilitar recuperación difusa Levenshtein y memoria asociativa.
  - Función de telemetría de hechos cargados: `ChatFactCount(const CHAT *ch)`.
- **Métricas transparentes en la carga de modelos (`/v1/model/load`, `/load`)**:
  - El endpoint HTTP `/v1/model/load` reporta ahora el campo `"facts_loaded": N` junto con `"symbols"` y `"relations"` tanto para modelos binarios (`binary_v2`) como para texto (`corpus_text`).
  - Los comandos conversacionales `/load <ruta>` confirman de inmediato en el mensaje cuántos hechos entraron al motor de QA.
- **Batería de pruebas unitarias para modelos con relaciones personalizadas (`test_model_generic_rel`)**:
  - Verificación end-to-end de serialización, carga binaria, 0 hechos descartados y respuestas de QA exactas en lenguaje natural sin respuestas "I don't know".
  - Cobertura de consultas en español e inglés con artículos determinados (`¿qué incluye el kit_a?`, `que contiene el kit_pro`, `what includes the kit_a`, `a que aplica el kit_b`).
- **Síntesis Directa de Código C y Algoritmos (`ServerIsCodeSynthesisTask`, `ServerSynthesizeCode`)**:
  - Reconocimiento preciso de consultas de generación o síntesis de código en lenguaje natural (`escribe en C la funcion de fibonacci`, `write a C function to calculate factorial`, `invertir cadena`, `busqueda binaria`, etc.).
  - Emite directamente la solución en Markdown estructurado con bloques de código C11 (````c`) completamente documentados, con funciones iterativas seguras ante desbordamiento, función de prueba `main()` y análisis de complejidad $O(n)$ / $O(1)$, retornando `finish_reason: "stop"`.
  - Opera tanto en modo chat directo como en modo agéntico con herramientas declaradas en OpenCode, impidiendo que una solicitud de código puro dispare erróneamente el plan STRIPS SWE-bench sobre archivos de proyecto o degrade a preguntas factuales ("I don't know").
- **Robustez HTTP y Lectura Inmune a Variantes de Cabecera (`FindHttpHeader`, `ReadHttpBody`)**:
  - Función de búsqueda de cabeceras RFC `FindHttpHeader` insensible a mayúsculas/minúsculas y tolerante a espacios antes de `:` (`Content-Length`, `content-length`, etc.).
  - `ReadHttpBody` mejorado para aceptar payloads recibidos íntegramente en el paquete de cabeceras y recuperación no bloqueante de payloads JSON completos ante desconexiones anticipadas del cliente.
- **Deserialización Segura de Cadenas Largas en JSON (`TakeJsonString`)**:
  - Truncamiento seguro para textos que exceden el tamaño de buffer destino: almacena de forma acotada y avanza el cursor hasta la comilla de cierre, evitando fallos `400 Bad Request` en historiales multi-turno voluminosos.

  - Reconocimiento de intenciones de creación de ficheros (p. ej. `crea un fichero test.txt`, `nuevo archivo config.json`, `create file foo.c`), formulando un plan atómico de 1 paso que despacha directamente la herramienta `write` con `filePath` y contenido inicial.
  - Se evita la ejecución espuria del ciclo de compilación STRIPS (`cmake --build` / `ctest`) sobre tareas de creación de documentos o scripts auxiliares.
  - Finalización limpia con confirmación Markdown explícita (`### Archivo Creado con Exito ('test.txt')`).
- **Gestión de Cargas HTTP de Gran Tamaño en Sesiones Multi-Turno (`SERVER_BODY_MAX`)**:
  - Ampliado el buffer máximo de peticiones HTTP a 2 MB (`2097152` bytes) y trasladado a memoria estática (`g_http_body`), eliminando de raíz el error `400 Bad Request: bad content length` que se producía cuando el cliente OpenCode enviaba esquemas de herramientas y varias vueltas de historial con salidas de archivo superiores a 64 KB.
  - Búferes estáticos de respuesta (`content`: 32 KB, `resp`: 64 KB, `last_tool_output`: 16 KB) previniendo desbordamientos de pila y truncamientos en inspección de árboles de directorio extensos.
- **Compatibilidad RFC para Cabeceras HTTP (`Content-Length`)**:
  - Comprobación insensible a mayúsculas/minúsculas para `Content-Length:`, `content-length:` y `Content-length:`.
- **Ampliación Léxica de Exploración e Inspección**:
  - Soporte en `ServerIsInspectionTask`, `ServerIsCodingTask` y `IsFolderOrGlobQuery` para comandos de exploración natural como `lista las subcarpetas`, `subdirectorios`, `subfolders`, `listar`, etc., despachando automáticamente la herramienta `glob` de OpenCode.
- **Corrección de Extracción de Nombres de Archivo (`FindFileForIssue`)**:
  - Eliminado el punto `.` como delimitador en `strtok` y añadido saneamiento de puntuación de cola (`.` `,` `;` `?` `)`), permitiendo extraer con total fidelidad nombres reales con extensión (`test.txt`, `main.c`, `config.json`, etc.) en lugar de degradar erróneamente a `CMakeLists.txt`.
- **Detección Rigurosa de Errores de Terminal y Herramientas (`ServerInspectToolResponse`)**:
  - `ServerInspectToolResponse` detecta ahora salidas de fallo en texto plano de herramientas como `bash` y `cmake` (p. ej. `Error: could not load cache`, `No tests were found`, `command not found`, `Permission denied`), impidiendo reportes falsos de verificación aprobada cuando una herramienta falla sin código JSON explícito.
- **Soporte para Formato de Contenido de Usuario en Array**:
  - `ServerExtractQuery` procesa tanto el formato simple de cadena (`"content": "..."`) como el formato en array estándar de OpenAI (`"content": [{"type": "text", "text": "..."}]`).

---

## [Phase 19] - 2026-09-20
### Añadido
- **Integración Agéntica Nativa y Copiloto Local OpenCode (Modo E)**:
  - Implementación del protocolo OpenAI Tool Calling sobre `/v1/chat/completions` en C11 puro.
  - Bucle ReAct continuo con formulación de planes STRIPS óptimos y resolución autónoma de tareas de ingeniería.
  - Discriminación estricta entre intenciones de programación (shell, inspección, parches AST) y consultas factuales de conocimiento.
  - Soporte para comandos de terminal (`dir`, `ls`, comodines `*.*`) sin fallback espurio de texto.

---

## [Phase 18] - 2026-09-20
### Añadido
- **Serialización Binaria de Alto Rendimiento y Mmap para Grafos**:
  - Carga en memoria mapeada (`mmap`) de grafos de conocimiento a escala sin latencia de parseo.
  - Empaquetamiento compacto de tripletas y persistencia atómica en disco.

---

## [Phase 17] - 2026-09-20
### Añadido
- **Persistencia Episódica Continua y Aprendizaje Conversacional**:
  - Almacenamiento continuo en disco (`data/memory/episodic.tsv`) para hechos aprendidos en tiempo de ejecución (`/learn`, `ChatLearnTriple`).
  - Recuperación episódica inmediata integrada en el despachador conversacional sin reentrenamiento.

---

## [Phase 16] - 2026-09-20
### Añadido
- **Modulación Pragmática, Filtro Cuántico-Pirata y Cero Hardcoding**:
  - Perfil declarativo `PERSONA_PIRATE_QUANTUM` implementado con tablas léxicas formales (`g_persona_lexicons`).
  - Conmutación dinámica de perspectiva conversacional (`/persona <name>`, `:persona <name>`, `modo pirata`).
  - Teorema formal de no-interferencia factual verificado empíricamente: $\text{Facts}(\Pi(Q)) \equiv \text{Facts}(Q)$.

---

## [Phase 15] - 2026-09-20
### Añadido
- **Integración Conversacional de Sentido Común y Causalidad Física**:
  - Detección de intenciones de causalidad física (`INT_QA_CONSEQUENCE`, "¿qué pasa si...") y capacidades (`INT_QA_AFFORDANCE`, "¿para qué sirve...").
  - Deducción ontológica cerrada en RAM (p. ej. vaso de cristal $\to$ material frágil $\to$ se rompe al caer).
  - Robustez de codificación de caracteres acentuados en consolas multiplataforma (UTF-8, Latin-1, CP850).

---

## [Phase 14] - 2026-09-20
### Añadido
- **Los Cuatro Pilares Cognitivos en C11 (Zero Dependencies)**:
  - **Pilar 1 (VSA / HDC)**: Computación hiperdimensional con vectores Kanerva de 256 bits, popcount AVX2 y memoria de limpieza asociativa.
  - **Pilar 2 (CCG Realizer)**: Realizador sintáctico de superficie basado en Gramática Categorial Combinatoria con combinadores formales ($>, <, >B, <B$) y tablas de concordancia multilingüe (EN, ES, FR).
  - **Pilar 3 (Sentido Común en RAM)**: Ingesta en flujo continuo de ConceptNet 5.8 (10M tripletas en ~305 MB RAM) con razonamiento de contención y affordances.
  - **Pilar 4 (Perspectivas Epistémicas & Persona)**: Filtros de perspectiva matemática (`neutral`, `architect`, `auditor`, `tutor`, `concise`, `socratic`) con abstención honesta y 0 alucinaciones.
