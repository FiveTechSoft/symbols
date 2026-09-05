# Roadmap: capacidades tipo TinyLlama — APARCADO (2026-09-05)

> Decisión: **camino propio** (simbólico, sin LLM en el producto).
> Este roadmap híbrido queda aparcado. Experimento local eliminado;
> URLs por si se retoma algún día:
> - llama.cpp b10816 win-cpu-x64:
>   `https://github.com/ggml-org/llama.cpp/releases/download/b10816/llama-b10816-bin-win-cpu-x64.zip`
> - TinyLlama-1.1B-Chat Q4_K_M (TheBloke):
>   `https://huggingface.co/TheBloke/TinyLlama-1.1B-Chat-v1.0-GGUF/resolve/main/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf`
>
> Ver plan vigente: P1–P5 del camino propio (debajo).

# Plan vigente: camino propio (sin ser un LLM tradicional)

- [ ] P1 — Bucle de crecimiento medido: extractores → lint → regen →
  eval. Métrica: triples en modelo + eval estable (ver
  `tools/progress.csv`: columnas model_relations/model_symbols).
- [ ] P2 — Profundidad de QA: multi-hop, conteo, comparación,
  negación; eval por categorías.
- [ ] P3 — NLG acotada y honesta ("no lo sé" como feature).
- [ ] P4 — Razonamiento con confianza (pesos + contradicciones en
  respuestas).
- [ ] P5 — Set held-out con verdad humana (deuda explícita).

No-objetivos: redacción abierta, chat general, ayuda a programar.

# (Aparcado) Roadmap híbrido original

## Premisa honesta

Este proyecto (grafo simbólico + embeddings 32D) **nunca será TinyLlama
por vía simbólica pura**: la generación abierta de texto exige un modelo
neuronal autorregresivo. Entrenar uno aquí es inviable (sin CUDA
funcional, sin corpus de billones de tokens).

El objetivo alcanzable es **paridad de capacidades observables**
(conversar con fluidez + saber cosas + razonar) con arquitectura
distinta: el grafo aporta memoria exacta, explicabilidad y
verificación sin GPU; un LLM adjunto aporta fluidez.

## Estado

- [x] Fase 1 — `/llm` en el CLI contra `llama-cli` externo + pesos
  TinyLlama-1.1B-Chat Q4_K_M (`models/`, gitignored). Test con SKIP
  si no hay binario/pesos. Sin acoplamiento CMake a llama.cpp.
- [ ] Fase 0 — puerta de no-regresión ya cubierta por `test_eval_qa`
  (99/99) + `test_qa_hygiene`; falta set held-out con verdad humana
  (deuda explícita, ver ROADMAP general).
- [ ] Fase 2 — Grounding (grafo → prompt): inyectar triples como
  contexto. Medida: % respuestas con cita trazable al grafo.
- [ ] Fase 3 — Verificación (prompt → grafo): S-P-O de la respuesta
  vs detector de contradicciones. Medida: % detectado en set
  adversarial.
- [ ] Fase 4 — Destilación inversa (LLM → grafo): crecer el grafo
  desde texto generado (cierre del bucle de aprendizaje).
- [ ] Fase 5 (opcional, didáctica) — micro-Transformer en C.
  No es camino a paridad.

## Dependencias externas (opcionales, fuera de git)

- `tools/llama-cli.exe` — release oficial llama.cpp win-cpu-x64.
- `models/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf` (~0,7 GB).
- Sin ellos: todo compila y pasa (test_llm → SKIP).

## Decisiones registradas

- 2026-09-05: híbrido con dependencia externa ligera en vez de
  submódulo llama.cpp (build CI rápido, sin red obligatoria).
- No entrenar LLM propio en este hardware.
