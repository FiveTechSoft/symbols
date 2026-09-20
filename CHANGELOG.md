# Changelog

Todas las novedades, mejoras y correcciones notables de **Symbolic LLM / symbols-server** quedan documentadas en este archivo.

El formato se basa en [Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/).

---

## [Unreleased] - 2026-09-20

### Añadido
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

- **Detección e Invocación Quirúrgica para Creación de Archivos (`IsFileCreationTask`)**:
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
