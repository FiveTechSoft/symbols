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

### Corregido
- **Salto de artículos y partículas funcionales en captura de argumentos relacionales (`FallbackOpen`, `TokAfterDe`)**:
  - En `FallbackOpen` y `TokAfterDe`, el analizador salta de forma no destructiva los artículos y determinantes (`el`, `la`, `los`, `las`, `the`, `un`, `una`, etc.) antes de capturar el argumento relacional. Consultas formuladas de forma natural como `¿qué incluye el kit_a?` o `what includes the kit_a` resuelven limpiamente al identificador objetivo (`kit_a`) en lugar de ser rechazadas por veto de palabras vacías (`SlotOk`).
  - Soporte composicional para nombres de relaciones con sufijos (`_A`, `_TO`, `_DE`, `_OF`) y separadores infijos (`_a_`, `_de_`, `_to_`, `_for_`, `_with_`) extrayendo la raíz verbal correcta (`APLICA_A` / `APLICA_A_MOTOR` -> `aplica`).
  - Añadidas equivalencias canónicas en `COMPILED_RELMAP` para dominios industriales y de ingeniería (`INCLUYE`, `INCLUDES`, `APLICA`, `APLICA_A`, `APLICA_A_MOTOR`, `APPLIES_TO`).
- **Extracción de patrones de comodín (`ExtractGlobPattern`)**:
  - Los signos de interrogación (`?`, `¿`) de preguntas en lenguaje natural (p. ej. *"¿qué es lo que hay en esta carpeta?"*) ahora se tratan como puntuación y no se confunden con comodines de archivo único, resolviendo por defecto al patrón universal `*`.
- **Visualización del contenido del directorio en inspección de carpetas**:
  - Preservación íntegra de la salida devuelta por la herramienta `glob` en el resumen final de Markdown para que el usuario siempre vea la lista real de archivos.
- **Traducción de consultas no entendidas**:
  - Corregido el fallo por el cual consultas legítimas sobre modelos binarios cargados devolvían "I don't know." debido a que las relaciones no se habían ingresado en `ch->kb`.

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
