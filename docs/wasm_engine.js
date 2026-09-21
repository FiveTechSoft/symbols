/* wasm_engine.js - WASM-backed SymbolicEngine replacement
 * Provides the same API surface as the original JS engine.js
 * but delegates all computation to the C11 engine via WebAssembly.
 * NO dependency on engine.js — this is a full replacement.
 */

class WASMSymbolicEngine {
  constructor() {
    this._module = null;
    this._ready = false;

    /* JS-side mirrors of engine state (for UI graph/stats) */
    this.sentences = [];
    this.relations = [];
    this.relMap = new Map();
    this.subMap = new Map();
    this.objMap = new Map();
    this.symbols = new Map();
    this.episodic = {
      turns: [],
      activeFocus: null,
      citedSentIds: new Set(),
      learnedFacts: [],
      lastQueryTimeMs: 0
    };
    this.stopwords = new Set();
    this.translationMap = new Map();
    this.translationPhrases = [];
  }

  /* ---- Async init: must be called before any query ---- */
  async init(wasmJsPath, wasmBinPath) {
    const factory = SymbolicLLM || (typeof SymbolicLLM !== 'undefined' ? SymbolicLLM : null);
    if (!factory) throw new Error('symbolic_wasm.js not loaded');

    this._module = await factory({
      locateFile: (name) => {
        if (name.endsWith('.wasm')) return wasmBinPath || name;
        return wasmJsPath || name;
      }
    });

    /* Init C engine with empty corpus */
    this._module._wasm_init(0);
    this._ready = true;
    return this;
  }

  _check() {
    if (!this._ready) throw new Error('WASM engine not initialized');
  }

  /* ---- Corpus loading (text → Emscripten FS → C engine) ---- */
  ingestText(rawText, sourceLabel = 'user_input') {
    this._check();
    const t0 = performance.now();

    /* Write text to Emscripten virtual filesystem */
    const safeName = `/ingest_${Date.now()}.txt`;
    this._module.FS.writeFile(safeName, rawText);

    /* Load via C engine */
    const pathBuf = this._module._malloc(256);
    this._module.stringToUTF8(safeName, pathBuf, 256);
    const loaded = this._module._wasm_load_corpus(pathBuf);
    this._module._free(pathBuf);

    /* Cleanup temp file */
    try { this._module.FS.unlink(safeName); } catch(e) {}

    /* Update JS-side mirrors */
    const lines = rawText.split(/[.!?\n]+/).filter(s => s.trim().length > 5);
    let sentencesAdded = 0;
    let symbolsAdded = 0;
    for (const line of lines) {
      this.sentences.push({ id: this.sentences.length, text: line.trim(), source: sourceLabel });
      sentencesAdded++;
      const tokens = line.toLowerCase().replace(/[^a-z0-9\s]/g, ' ').split(/\s+/).filter(t => t.length > 2);
      for (const tok of tokens) {
        if (!this.symbols.has(tok)) {
          symbolsAdded++;
          this.symbols.set(tok, { name: tok, freq: 1 });
        } else {
          this.symbols.get(tok).freq++;
        }
      }
    }

    const elapsedMs = performance.now() - t0;
    return { sentencesAdded, symbolsAdded, elapsedMs };
  }

  /* ---- Load preset corpus from data.js PRESET_CORPORA ---- */
  loadPreset(key) {
    this._check();
    if (typeof PRESET_CORPORA === 'undefined') return null;
    const preset = PRESET_CORPORA[key];
    if (!preset) return null;

    const t0 = performance.now();

    /* Combine sentences and triples into a loadable text format */
    let corpusText = preset.sentences.join('.\n');
    if (preset.triples && preset.triples.length > 0) {
      corpusText += '\n' + preset.triples.map(t => `${t.s} ${t.p} ${t.o}`).join('\n');
    }

    /* Write to FS and load */
    const safeName = `/preset_${key}.txt`;
    this._module.FS.writeFile(safeName, corpusText);
    const pathBuf = this._module._malloc(256);
    this._module.stringToUTF8(safeName, pathBuf, 256);
    this._module._wasm_load_corpus(pathBuf);
    this._module._free(pathBuf);
    try { this._module.FS.unlink(safeName); } catch(e) {}

    /* Mirror state to JS */
    for (const sent of preset.sentences) {
      this.sentences.push({ id: this.sentences.length, text: sent, source: preset.name });
      const tokens = sent.toLowerCase().replace(/[^a-z0-9\s]/g, ' ').split(/\s+/).filter(t => t.length > 2);
      for (const tok of tokens) {
        if (!this.symbols.has(tok)) this.symbols.set(tok, { name: tok, freq: 1 });
        else this.symbols.get(tok).freq++;
      }
    }
    if (preset.triples) {
      for (const t of preset.triples) {
        const key = `${t.s}__${t.p}__${t.o}`;
        const rel = { subject: t.s, predicate: t.p, object: t.o, count: 1, weight: 1.0 };
        this.relations.push(rel);
        this.relMap.set(key, rel);
        if (!this.subMap.has(t.s)) this.subMap.set(t.s, []);
        this.subMap.get(t.s).push(rel);
        if (!this.objMap.has(t.o)) this.objMap.set(t.o, []);
        this.objMap.get(t.o).push(rel);
      }
    }

    return {
      name: preset.name,
      sentencesCount: preset.sentences.length,
      triplesCount: (preset.triples || []).length,
      elapsedMs: performance.now() - t0
    };
  }

  /* ---- Query (delegates to WASM) ---- */
  query(inputQuery) {
    this._check();
    const t0 = performance.now();

    const qBuf = this._module._malloc(inputQuery.length * 4 + 1);
    this._module.stringToUTF8(inputQuery, qBuf, inputQuery.length * 4 + 1);
    const resultPtr = this._module._wasm_query(qBuf);
    const resultStr = this._module.UTF8ToString(resultPtr);
    this._module._free(qBuf);

    const elapsedMs = performance.now() - t0;

    /* Parse the C engine's response (plain text or JSON-ish) */
    let response = resultStr;
    let status = 'VERBATIM_CITATION';
    let proofTrace = null;
    let citation = null;

    /* The C engine returns plain text; detect UNKNOWN */
    if (resultStr.includes('sin constancia') || resultStr.includes('No verified ground truth') || resultStr.includes('UNKNOWN')) {
      status = 'UNKNOWN_FAIL_CLOSED';
    }

    /* Improve UNKNOWN messages with actionable guidance + corpus suggestion */
    if (status === 'UNKNOWN_FAIL_CLOSED') {
      const q = inputQuery.toLowerCase();
      const suggestion = this._suggestCorpus(q);
      const hasCorpus = this.sentences.length > 0;
      if (!hasCorpus) {
        response = 'No tengo informacion suficiente para responder. Selecciona un corpus en el sidebar izquierdo (Biblia, Jung, Quantum, Code) o ingresa un archivo .txt.';
      } else if (suggestion) {
        response = `No tengo constancia de eso en el corpus cargado. Para esta pregunta te conviene usar el corpus "${suggestion.name}" (${suggestion.hint}). Cambialo en el sidebar izquierdo.`;
      } else {
        response = 'No tengo constancia suficiente en el corpus cargado para responder esa pregunta. Prueba con otro corpus o ingresa mas texto.';
      }
    }

    /* Register turn */
    this.episodic.turns.push({
      id: this.episodic.turns.length + 1,
      query: inputQuery,
      response,
      status,
      elapsedMs,
      timestamp: new Date().toISOString()
    });

    return { response, proofTrace, citation, status, elapsedMs, focus: this.episodic.activeFocus };
  }

  /* ---- Corpus suggestion based on query keywords ---- */
  _suggestCorpus(query) {
    const corpusHints = [
      {
        name: 'Biblia Canonica',
        hint: 'genealogias, reyes, libros biblicos',
        keywords: ['david', 'solomon', 'jonah', 'jonas', 'abraham', 'jacob', 'judah', 'isaac',
                   'bible', 'biblia', 'king', 'rey', 'queen', 'reina', 'lord', 'señor',
                   'proverbs', 'proverbios', 'psalms', 'salmos', 'genesis', 'exodus', 'exodo',
                   'samuel', 'saul', 'oboed', 'boaz', 'jesse', 'rehoboam', 'abijah',
                   'padre', 'hijo', 'begat', 'engendro', 'anointed', 'uncio',
                   'testament', 'testamento', 'apocalipsis', 'revelation']
      },
      {
        name: 'C.G. Jung: Unconscious',
        hint: 'arquetipos, simbolos, inconsciente, libido',
        keywords: ['jung', 'arquetipo', 'archetype', 'libido', 'inconscient', 'unconscious',
                   'dreams', 'suenos', 'simbolo', 'symbol', 'faust', 'mephistopheles',
                   'dios', 'madre', 'mother', 'tree', 'arbol', 'sun', 'sol',
                   'psyche', 'psique', 'collective', 'colectivo', 'primordial',
                   'golden bough', 'rama dorada', 'pact', 'pacto']
      },
      {
        name: 'Quantum Information Theory',
        hint: 'qubits, entrelazamiento, superposicion',
        keywords: ['qubit', 'quantum', 'cuantic', 'entangle', 'entrelaz', 'superposition',
                   'superposicion', 'shor', 'grover', 'decoherence', 'decoherencia',
                   'teleportation', 'teleportacion', 'algorithm', 'algoritmo',
                   'processor', 'procesador', 'circuit', 'circuito', 'ion', 'photon', 'foton']
      },
      {
        name: 'SWE-bench & Code Graph',
        hint: 'AST, blast radius, STRIPS planner, Django',
        keywords: ['django', 'flask', 'sympy', 'scikit', 'pytest', 'blast radius',
                   'strips', 'planner', 'planificador', 'code graph', 'grafo',
                   'function', 'funcion', 'class', 'clase', 'module', 'modulo',
                   'compile', 'compilar', 'linker', 'debug', 'memory leak',
                   'fuga de memoria', 'buffer overflow', 'pointer', 'puntero']
      }
    ];

    const q = query.normalize('NFD').replace(/[\u0300-\u036f]/g, '').toLowerCase();
    let bestMatch = null;
    let bestScore = 0;

    for (const corpus of corpusHints) {
      let score = 0;
      for (const kw of corpus.keywords) {
        if (q.includes(kw)) score++;
      }
      if (score > bestScore) {
        bestScore = score;
        bestMatch = corpus;
      }
    }

    return bestScore >= 1 ? bestMatch : null;
  }

  /* ---- Stats ---- */
  get factCount() { return this._ready ? this._module._wasm_fact_count() : 0; }
  get symbolCount() { return this._ready ? this._module._wasm_symbol_count() : 0; }
  get relationCount() { return this._ready ? this._module._wasm_relation_count() : 0; }
  get sentenceCount() { return this._ready ? this._module._wasm_sentence_count() : 0; }

  /* ---- Concept graph (JS-side mirror for canvas) ---- */
  getTopConcepts(limit = 8) {
    return Array.from(this.symbols.values())
      .filter(s => s.freq > 0)
      .sort((a, b) => b.freq - a.freq)
      .slice(0, limit)
      .map(s => ({ name: s.name, kappa: Math.log(1 + s.freq), freq: s.freq }));
  }

  canonicalize(token) {
    return token.toLowerCase().normalize('NFD').replace(/[\u0300-\u036f]/g, '').replace(/[^a-z0-9_]/g, '');
  }

  /* ---- Translation table ---- */
  loadTranslationTable(rawText) {
    if (!rawText) return;
    this.translationMap.clear();
    const lines = rawText.split(/\r?\n/);
    for (const line of lines) {
      const trimmed = line.trim();
      if (!trimmed || trimmed.startsWith('#')) continue;
      const parts = trimmed.split(/[=\t]/).map(s => s.trim().toLowerCase());
      if (parts.length >= 2) this.translationMap.set(parts[0], parts[1]);
    }
  }

  /* ---- State export/import (for localStorage persistence) ---- */
  exportState() {
    return {
      version: '1.0-wasm',
      timestamp: new Date().toISOString(),
      sentences: this.sentences,
      relations: this.relations,
      episodic: this.episodic
    };
  }

  importState(data) {
    if (!data || !data.sentences) return false;
    this.sentences = data.sentences || [];
    this.relations = data.relations || [];
    this.relMap.clear();
    this.subMap.clear();
    this.objMap.clear();
    for (const r of this.relations) {
      const key = `${r.subject}__${r.predicate}__${r.object}`;
      this.relMap.set(key, r);
      if (!this.subMap.has(r.subject)) this.subMap.set(r.subject, []);
      this.subMap.get(r.subject).push(r);
      if (!this.objMap.has(r.object)) this.objMap.set(r.object, []);
      this.objMap.get(r.object).push(r);
    }
    if (data.episodic) this.episodic = data.episodic;
    return true;
  }

  /* ---- Episodic learn (delegates to WASM) ---- */
  learnTriple(subject, relation, object) {
    this._check();
    return this._module._wasm_learn(subject, relation, object);
  }
}
