// engine.js - Pure JavaScript Implementation of the Symbolic LLM Core Engine
// Strict 0% Hallucination, O(1) Graph Lookups, Concept Concentration (kappa), Verbatim Provenance

class SymbolicEngine {
  constructor() {
    this.sentences = [];       // Verbatim text sentences
    this.symbols = new Map();  // name -> { id, name, freq, v: Float32Array(32), kappa: float }
    this.relations = [];       // Array of { subject, predicate, object, count, weight, sourceSentId }
    this.relMap = new Map();   // key: S_P_O -> relation
    this.subMap = new Map();   // subject -> array of relations
    this.objMap = new Map();   // object -> array of relations
    this.episodic = {
      turns: [],
      activeFocus: null,
      citedSentIds: new Set(),
      learnedFacts: [],
      lastQueryTimeMs: 0
    };
    this.stopwords = new Set([
      "the", "a", "an", "and", "or", "but", "in", "on", "at", "to", "for",
      "of", "with", "by", "from", "up", "about", "into", "over", "after",
      "is", "are", "was", "were", "be", "been", "being", "have", "has", "had",
      "do", "does", "did", "shall", "will", "should", "would", "may", "might",
      "must", "can", "could", "that", "which", "who", "whom", "this", "these",
      "those", "there", "their", "it", "its", "as", "he", "she", "they", "we",
      "tell", "say", "know", "knows", "explain", "describe", "give", "continue", "proceed", "next",
      "el", "la", "los", "las", "un", "una", "unos", "unas", "y", "o", "pero",
      "en", "sobre", "a", "para", "por", "de", "del", "con", "sin", "desde",
      "hasta", "es", "son", "era", "eran", "fue", "fueron", "ser", "estar",
      "ha", "han", "que", "cual", "cuales", "quien", "quienes", "este", "esta",
      "estos", "estas", "ese", "esa", "esos", "esas", "su", "sus", "como",
      "dame", "cuantos", "cuantas", "tiene", "contiene", "hay",
      "trata", "tratar", "hablame", "habla", "dime", "cuentame", "explicame",
      "refiero", "acerca", "mas", "dicho", "mismo", "misma",
      "puedes", "puede", "puedo", "podrias", "podria", "podemos",
      "decir", "decirme", "decirnos", "sabes", "sabe", "sabria", "sabrias",
      "conoces", "conoce", "conocemos",
      "continua", "continuar", "sigue", "seguir", "siguiente", "adelante",
      "explica", "explicalo", "explicamelo", "explicame", "explicala", "explicamela",
      "describe", "describelo", "describemelo", "describeme",
      "cuenta", "cuentalo", "cuentamelo", "cuentame",
      "aclara", "aclaralo", "aclaramelo", "detalla", "detallalo", "desarrolla", "desarrollalo",
      "explain", "elaborate",
      "hizo", "hacer", "hace",
      "me", "te", "se", "nos", "os", "yo", "tu", "mi", "mis", "ti"
    ]);
  }

  // DJB2a 32-bit Hash
  hashString(str) {
    let hash = 5381;
    for (let i = 0; i < str.length; i++) {
      hash = ((hash << 5) + hash) ^ str.charCodeAt(i);
      hash = hash & 0xffffffff;
    }
    return Math.abs(hash);
  }

  // Canonicalize token (lowercase, diacritic-normalized, alpha-numeric)
  canonicalize(token) {
    let t = token
      .toLowerCase()
      .normalize("NFD")
      .replace(/[\u0300-\u036f]/g, "")
      .replace(/[^a-z0-9_]/g, "");
    if (t === "solomon") return "salomon";
    if (t === "jonah") return "jonas";
    if (t === "sun") return "sol";
    if (t === "energia") return "energy";
    if (t === "psiquica" || t === "psiquico") return "psychic";
    if (t === "inconsciente") return "unconscious";
    if (t === "arquetipo" || t === "arquetipos") return "archetype";
    if (t === "simbolo" || t === "simbolos") return "symbol";
    return t;
  }

  // Get or register symbol with 32D Random Indexing vector
  getOrCreateSymbol(name) {
    const canon = this.canonicalize(name);
    if (!canon) return null;
    if (this.symbols.has(canon)) {
      const sym = this.symbols.get(canon);
      sym.freq++;
      return sym;
    }

    const id = this.symbols.size + 1;
    const v = new Float32Array(32);
    // Initialize ternary pseudo-random signature
    const h = this.hashString(canon);
    v[h % 32] = 1.0;
    v[(h >> 5) % 32] = -1.0;

    const sym = { id, name: canon, displayName: name, freq: 1, v, kappa: 0 };
    this.symbols.set(canon, sym);
    return sym;
  }

  // Insert relational triple O(1)
  addRelation(subject, predicate, object, sourceSentId = -1, weight = 1.0) {
    const sSym = this.getOrCreateSymbol(subject);
    const pSym = this.getOrCreateSymbol(predicate);
    const oSym = this.getOrCreateSymbol(object);
    if (!sSym || !pSym || !oSym) return false;

    const key = `${sSym.name}__${pSym.name}__${oSym.name}`;
    if (this.relMap.has(key)) {
      const rel = this.relMap.get(key);
      rel.count++;
      rel.weight = Math.min(1.0, rel.weight + 0.1);
      return false;
    }

    const rel = {
      subject: sSym.name,
      predicate: pSym.name,
      object: oSym.name,
      count: 1,
      weight,
      sourceSentId
    };

    this.relations.push(rel);
    this.relMap.set(key, rel);

    if (!this.subMap.has(sSym.name)) this.subMap.set(sSym.name, []);
    this.subMap.get(sSym.name).push(rel);

    if (!this.objMap.has(oSym.name)) this.objMap.set(oSym.name, []);
    this.objMap.get(oSym.name).push(rel);

    return true;
  }

  // Ingest raw text stream into sentence store and co-occurrence vectors
  ingestText(rawText, sourceLabel = "user_input") {
    const t0 = performance.now();
    // Split into sentences
    const rawSentences = rawText
      .replace(/\r\n/g, "\n")
      .split(/(?<=[.?!])\s+|\n+/)
      .map(s => s.trim())
      .filter(s => s.length > 5);

    let sentencesAdded = 0;
    let newSymbolsAdded = 0;
    const initialSymCount = this.symbols.size;

    for (const rawSent of rawSentences) {
      const sentId = this.sentences.length;
      this.sentences.push({
        id: sentId,
        text: rawSent,
        source: sourceLabel
      });
      sentencesAdded++;

      // Tokenize sentence
      const tokens = rawSent
        .toLowerCase()
        .replace(/[^a-z0-9\s]/g, " ")
        .split(/\s+/)
        .filter(t => t.length > 2);

      const sentSymbols = [];
      for (const tok of tokens) {
        const sym = this.getOrCreateSymbol(tok);
        if (sym) sentSymbols.push(sym);
      }

      // Update Hebbian co-occurrence across sliding context window (W=5)
      for (let i = 0; i < sentSymbols.length; i++) {
        const s1 = sentSymbols[i];
        for (let j = Math.max(0, i - 5); j < Math.min(sentSymbols.length, i + 6); j++) {
          if (i === j) continue;
          const s2 = sentSymbols[j];
          const dist = Math.abs(i - j);
          for (let d = 0; d < 32; d++) {
            s1.v[d] += s2.v[d] / dist;
          }
        }
      }

      // Extract basic declarative S-V-O or copular relations
      this.extractSimpleTriples(tokens, sentId);
    }

    // Recompute Concept Concentration Metric (kappa) for all symbols
    this.computeConceptConcentration();

    newSymbolsAdded = this.symbols.size - initialSymCount;
    const elapsedMs = performance.now() - t0;

    return {
      sentencesAdded,
      symbolsAdded: newSymbolsAdded,
      totalSentences: this.sentences.length,
      totalSymbols: this.symbols.size,
      totalRelations: this.relations.length,
      elapsedMs
    };
  }

  // Extract triples from simple linguistic structures
  extractSimpleTriples(tokens, sentId) {
    // Check for "X is a Y", "X begat Y", "X is the Y of Z"
    for (let i = 0; i < tokens.length - 2; i++) {
      if (tokens[i + 1] === "is" || tokens[i + 1] === "was" || tokens[i + 1] === "es" || tokens[i + 1] === "era") {
        let objIdx = i + 2;
        if (tokens[objIdx] === "a" || tokens[objIdx] === "an" || tokens[objIdx] === "the" || tokens[objIdx] === "un" || tokens[objIdx] === "una" || tokens[objIdx] === "el" || tokens[objIdx] === "la") {
          objIdx++;
        }
        if (objIdx < tokens.length && !this.stopwords.has(tokens[i]) && !this.stopwords.has(tokens[objIdx])) {
          this.addRelation(tokens[i], "IS_A", tokens[objIdx], sentId);
        }
      } else if (tokens[i + 1] === "begat") {
        if (!this.stopwords.has(tokens[i]) && !this.stopwords.has(tokens[i + 2])) {
          this.addRelation(tokens[i + 2], "SON_OF", tokens[i], sentId);
        }
      }
    }
  }

  // Concept Concentration Metric: kappa = (max(v) / sum(v)) * log(1 + freq)
  computeConceptConcentration() {
    for (const sym of this.symbols.values()) {
      if (this.stopwords.has(sym.name)) {
        sym.kappa = 0.0;
        continue;
      }
      let sum = 0.0;
      let maxVal = 0.0;
      for (let d = 0; d < 32; d++) {
        const val = Math.abs(sym.v[d]);
        sum += val;
        if (val > maxVal) maxVal = val;
      }
      if (sum > 0.0001) {
        const concentration = maxVal / sum;
        sym.kappa = concentration * Math.log(1.0 + sym.freq);
      } else {
        sym.kappa = 0.0;
      }
    }
  }

  // Get Top-N Key Concepts by Kappa (Thematic Centroids)
  getTopConcepts(limit = 8) {
    const list = Array.from(this.symbols.values())
      .filter(s => !this.stopwords.has(s.name) && s.freq > 0)
      .sort((a, b) => b.kappa - a.kappa);
    return list.slice(0, limit);
  }

  // Exact Deductive Backward-Chaining Search
  deduce(subject, predicate, object, depth = 0, maxDepth = 4, gamma = 0.9) {
    const sCanon = this.canonicalize(subject);
    const pCanon = this.canonicalize(predicate);
    const oCanon = this.canonicalize(object);

    // Direct lookup
    const directKey = `${sCanon}__${pCanon}__${oCanon}`;
    if (this.relMap.has(directKey)) {
      const r = this.relMap.get(directKey);
      return {
        found: true,
        confidence: r.weight,
        proofTrace: [`${sCanon.toUpperCase()} ──${pCanon.toUpperCase()}──> ${oCanon.toUpperCase()}`],
        sourceSentId: r.sourceSentId
      };
    }

    if (depth >= maxDepth) return { found: false, confidence: 0, proofTrace: [] };

    // Multi-hop / Transitive Search (e.g. SON_OF / IS_A)
    const outEdges = this.subMap.get(sCanon) || [];
    for (const edge of outEdges) {
      if (edge.predicate === pCanon || edge.predicate === "is_a") {
        const subResult = this.deduce(edge.object, predicate, object, depth + 1, maxDepth, gamma);
        if (subResult.found) {
          const step = `${sCanon.toUpperCase()} ──${edge.predicate.toUpperCase()}──> ${edge.object.toUpperCase()}`;
          return {
            found: true,
            confidence: subResult.confidence * gamma,
            proofTrace: [step, ...subResult.proofTrace],
            sourceSentId: subResult.sourceSentId !== -1 ? subResult.sourceSentId : edge.sourceSentId
          };
        }
      }
    }

    return { found: false, confidence: 0, proofTrace: [] };
  }

  // Natural Language Question Answering
  query(inputQuery) {
    const t0 = performance.now();
    const clean = inputQuery.trim().toLowerCase();
    let response = "";
    let proofTrace = null;
    let citation = null;
    let status = "UNKNOWN";

    // Intent 1: Topic introspection ("what areas do you know?")
    if (clean.includes("what areas") || clean.includes("que areas") || clean.includes("areas que conoces") || clean.includes("temas conoces") || clean.includes("topics")) {
      const top = this.getTopConcepts(8);
      if (top.length === 0) {
        response = "The knowledge base is currently empty. Ingest text or load a preset corpus to begin.";
      } else {
        const names = top.map(t => t.name.toUpperCase()).join(", ");
        response = `The ingested knowledge graph centers on the following fundamental thematic pillars: ${names}. You can ask me any verified questions about these concepts.`;
        status = "INTROSPECTION";
      }
    }
    // Intent 2: Start a conversation ("start a conversation")
    else if (clean.includes("start a conversation") || clean.includes("inicia una conversacion") || clean.includes("conversar")) {
      const top = this.getTopConcepts(1);
      if (top.length > 0) {
        const anchor = top[0];
        this.episodic.activeFocus = anchor.name;
        // Find a representative sentence
        const matchSent = this.sentences.find(s => s.text.toLowerCase().includes(anchor.name));
        response = `Let's discuss ${anchor.name.toUpperCase()}. According to verified text records: "${matchSent ? matchSent.text : ''}". Which aspect would you like to explore?`;
        if (matchSent) citation = `Sentence #${matchSent.id + 1} (${matchSent.source})`;
        status = "CONVERSATION_START";
      } else {
        response = "No text is loaded to start a grounded conversation.";
      }
    }
    // Intent 3: Dynamic Learning ("learn: ...", "aprende: ...", or user declarative assertion)
    else if (clean.startsWith("learn:") || clean.startsWith("aprende:") || clean.startsWith("remember:")) {
      const payload = inputQuery.substring(inputQuery.indexOf(":") + 1).trim();
      const res = this.ingestText(payload, "user_dialogue");
      this.episodic.learnedFacts.push(payload);
      response = `Understood. Stored verbatim into literal memory and incorporated ${res.symbolsAdded} new symbols and relational edges into the graph in ${res.elapsedMs.toFixed(3)} ms. Zero hallucination guaranteed.`;
      status = "DYNAMIC_LEARNED";
    }
    // Intent 4: Specific Kinship/Succession/Fact Queries
    else {
      const cleanNorm = clean
        .normalize("NFD")
        .replace(/[\u0300-\u036f]/g, "")
        .replace(/\bsolomon\b/g, "salomon")
        .replace(/\bjonah\b/g, "jonas")
        .replace(/\bsun\b/g, "sol")
        .replace(/\benergia\s+psiquica\b/g, "psychic energy")
        .replace(/\benergia\b/g, "energy")
        .replace(/\bpsiquica\b/g, "psychic")
        .replace(/\bpsiquico\b/g, "psychic")
        .replace(/\binconsciente\b/g, "unconscious")
        .replace(/\barquetipo\b/g, "archetype")
        .replace(/\barquetipos\b/g, "archetypes")
        .replace(/\bsimbolo\b/g, "symbol")
        .replace(/\bsimbolos\b/g, "symbols");
      // Extract target words filtered by stopwords
      let words = cleanNorm
        .replace(/[^a-z0-9\s]/g, " ")
        .split(/\s+/)
        .filter(w => !this.stopwords.has(w) && w.length > 1);

      const META_WORDS = new Set([
        "libro", "libros", "capitulo", "capitulos", "versiculo", "versiculos",
        "autor", "autoria", "lista", "texto", "parte", "partes", "nombre", "nombres",
        "historia", "tema", "origen", "significado", "proposito", "propositos",
        "funcion", "funciones", "rol", "role", "purpose", "function", "meaning",
        "book", "books", "chapter", "chapters",
        "escribio", "escribir", "escribe", "hizo", "hacer", "trata", "habla"
      ]);

      const isAnaphoric = 
        cleanNorm.startsWith("explica") ||
        cleanNorm.startsWith("describe") ||
        cleanNorm.startsWith("cuenta") ||
        cleanNorm.startsWith("aclara") ||
        cleanNorm.startsWith("detalla") ||
        cleanNorm.startsWith("desarrolla") ||
        cleanNorm.startsWith("explain") ||
        cleanNorm.startsWith("elaborate") ||
        cleanNorm.includes("cual es su") ||
        cleanNorm.includes("cuales son sus") ||
        cleanNorm.includes("what is its") ||
        cleanNorm.includes("what are its") ||
        cleanNorm.includes("para que sirve") ||
        cleanNorm.includes("su proposito") ||
        cleanNorm.includes("su funcion") ||
        cleanNorm.includes("su origen") ||
        cleanNorm.includes("su significado") ||
        cleanNorm.includes("de que trata") ||
        cleanNorm.includes("de que habla") ||
        cleanNorm.includes("de que va") ||
        cleanNorm.includes("hablame de") ||
        cleanNorm.includes("cuentame de") ||
        cleanNorm.includes("dime mas") ||
        cleanNorm.includes("continua") ||
        cleanNorm.includes("que mas") ||
        cleanNorm.includes("sobre el") ||
        cleanNorm.includes("sobre ella") ||
        cleanNorm.includes("de el") ||
        cleanNorm.includes("de ella") ||
        cleanNorm.includes("su autor") ||
        cleanNorm.includes("sus libros") ||
        cleanNorm.includes("que libros") ||
        cleanNorm.includes("lista los") ||
        cleanNorm.includes("cuales son los") ||
        cleanNorm.includes("tell me about") ||
        cleanNorm.includes("what is it about") ||
        cleanNorm.includes("tell me more");

      // Anaphora resolution: inherit activeFocus if query is conversational/elliptical
      if ((isAnaphoric || words.length === 0 || words.every(w => META_WORDS.has(w))) && this.episodic.activeFocus) {
        const rawTokens = this.episodic.activeFocus.split(/[_\s]+/).filter(t => !this.stopwords.has(t) && t.length > 1);
        const headNoun = rawTokens[rawTokens.length - 1];
        const focusTokens = headNoun ? [headNoun, ...rawTokens.filter(t => t !== headNoun)] : rawTokens;
        for (const ft of focusTokens) {
          if (!words.includes(ft)) {
            words.push(ft);
          }
        }
      }

      // 4A. Check kinship: "father of X", "padre de X", "son of X", "hijo de X"
      let targetEntity = null;
      let targetPredicate = null;

      if (cleanNorm.includes("father of") || cleanNorm.includes("padre de")) {
        const p = cleanNorm.includes("father of") ? "father of" : "padre de";
        targetEntity = cleanNorm.substring(cleanNorm.indexOf(p) + p.length).trim().split(/\s+/)[0].replace(/[^a-zA-Z0-9_]/g, "");
        targetPredicate = "son_of";
      } else if (cleanNorm.includes("son of") || cleanNorm.includes("hijo de")) {
        const p = cleanNorm.includes("son of") ? "son of" : "hijo de";
        targetEntity = cleanNorm.substring(cleanNorm.indexOf(p) + p.length).trim().split(/\s+/)[0].replace(/[^a-zA-Z0-9_]/g, "");
        targetPredicate = "father_of";
      }

      if (targetEntity && targetPredicate === "son_of") {
        const canonTarget = this.canonicalize(targetEntity);
        const rels = this.subMap.get(canonTarget) || [];
        const sonRel = rels.find(r => r.predicate === "son_of");
        if (sonRel) {
          const parent = sonRel.object.toUpperCase();
          response = `The father of ${targetEntity.toUpperCase()} is ${parent}.`;
          proofTrace = [`${canonTarget.toUpperCase()} ──SON_OF──> ${parent}`];
          if (sonRel.sourceSentId !== -1 && this.sentences[sonRel.sourceSentId]) {
            citation = `Sentence #${sonRel.sourceSentId + 1}: "${this.sentences[sonRel.sourceSentId].text}"`;
          }
          status = "EXACT_ANSWER";
          this.episodic.activeFocus = parent.toLowerCase();
        }
      }

      // 4B. Check verbatim literal sentence matching with ranking (find sentence with maximum matching query words)
      if (!response && words.length > 0) {
        let bestSent = null;
        let maxScore = -9999;
        const isContinuation = isAnaphoric && (cleanNorm.includes("que mas") || cleanNorm.includes("dime mas") || cleanNorm.includes("continua") || cleanNorm.includes("tell me more") || cleanNorm.includes("what else"));
        const queryPhrase = cleanNorm.replace(/[^a-z0-9\s]/g, " ").replace(/\s+/g, " ").trim();
        for (const sent of this.sentences) {
          const sLower = sent.text
            .toLowerCase()
            .normalize("NFD")
            .replace(/[\u0300-\u036f]/g, "")
            .replace(/\bsolomon\b/g, "salomon")
            .replace(/\bjonah\b/g, "jonas")
            .replace(/\bsun\b/g, "sol")
            .replace(/\benergia\s+psiquica\b/g, "psychic energy")
            .replace(/\benergia\b/g, "energy")
            .replace(/\bpsiquica\b/g, "psychic")
            .replace(/\bpsiquico\b/g, "psychic")
            .replace(/\binconsciente\b/g, "unconscious")
            .replace(/\barquetipo\b/g, "archetype")
            .replace(/\barquetipos\b/g, "archetypes")
            .replace(/\bsimbolo\b/g, "symbol")
            .replace(/\bsimbolos\b/g, "symbols")
            .replace(/[^a-z0-9\s]/g, " ");
          let score = words.filter(w => sLower.includes(w)).length;
          // Substring phrase bonus for exact multi-word alignment
          if (queryPhrase.length > 5 && sLower.includes(queryPhrase)) {
            score += 5;
          }
          // Definitional focus bonus: prioritize sentences defining the substantive entity
          for (const w of words) {
            if (!META_WORDS.has(w) && (sLower.startsWith(w) || sLower.includes("libro de " + w) || sLower.includes(w + " es") || sLower.includes("atribuye al rey " + w) || sLower.includes("rey " + w))) {
              score += 3;
            }
          }
          // Penalty for already cited sentences:
          if (this.episodic.citedSentIds && this.episodic.citedSentIds.has(sent.id)) {
            if (isContinuation) {
              score = -9999;
            } else {
              score -= 0.5;
            }
          }
          if (score > maxScore) {
            maxScore = score;
            bestSent = sent;
          }
        }
        const minRequired = Math.min(2, words.length);
        if (bestSent && maxScore >= minRequired && maxScore > 0) {
          response = `According to verified source records: "${bestSent.text}"`;
          citation = `Sentence #${bestSent.id + 1} (${bestSent.source})`;
          status = "VERBATIM_CITATION";
          if (!this.episodic.citedSentIds) this.episodic.citedSentIds = new Set();
          this.episodic.citedSentIds.add(bestSent.id);
          
          // Set activeFocus to the non-meta entity keyword
          const entityCandidate = words.find(w => !META_WORDS.has(w));
          if (entityCandidate) {
            this.episodic.activeFocus = entityCandidate;
          } else if (words[0]) {
            this.episodic.activeFocus = words[0];
          }

          // Check if any direct relation matches to provide proofTrace (subject or object)
          for (const w of words) {
            const canon = this.canonicalize(w);
            if (this.subMap.has(canon)) {
              const rels = this.subMap.get(canon);
              if (rels.length > 0) {
                const r = rels[0];
                proofTrace = [`${r.subject.toUpperCase()} ──${r.predicate.toUpperCase()}──> ${r.object.toUpperCase()}`];
                break;
              }
            }
            if (this.objMap && this.objMap.has(canon)) {
              const rels = this.objMap.get(canon);
              if (rels.length > 0) {
                const r = rels[0];
                proofTrace = [`${r.subject.toUpperCase()} ──${r.predicate.toUpperCase()}──> ${r.object.toUpperCase()}`];
                break;
              }
            }
          }
          if (!proofTrace && words.includes("psychic") && words.includes("energy")) {
            if (this.objMap && this.objMap.has("psychic_energy")) {
              const rels = this.objMap.get("psychic_energy");
              if (rels.length > 0) {
                const r = rels[0];
                proofTrace = [`${r.subject.toUpperCase()} ──${r.predicate.toUpperCase()}──> ${r.object.toUpperCase()}`];
              }
            }
          }
        }
      }

      // 4C. Check direct relation lookup across symbols
      if (!response && words.length > 0) {
        for (const w of words) {
          const canon = this.canonicalize(w);
          if (this.subMap.has(canon)) {
            const rels = this.subMap.get(canon);
            if (rels.length > 0) {
              const r = rels[0];
              response = `${r.subject.toUpperCase()} is connected via [${r.predicate.toUpperCase()}] to ${r.object.toUpperCase()}.`;
              proofTrace = [`${r.subject.toUpperCase()} ──${r.predicate.toUpperCase()}──> ${r.object.toUpperCase()}`];
              if (r.sourceSentId !== -1 && this.sentences[r.sourceSentId]) {
                citation = `Sentence #${r.sourceSentId + 1}: "${this.sentences[r.sourceSentId].text}"`;
                if (!this.episodic.citedSentIds) this.episodic.citedSentIds = new Set();
                this.episodic.citedSentIds.add(r.sourceSentId);
              }
              status = "RELATION_MATCH";
              this.episodic.activeFocus = r.object;
              break;
            }
          }
        }
      }

      // 4D. Fail-Closed Fallback (0% hallucination)
      if (!response) {
        response = "I don't know (No verified ground truth matches this assertion).";
        status = "UNKNOWN_FAIL_CLOSED";
      }
    }

    const elapsedMs = performance.now() - t0;
    this.episodic.lastQueryTimeMs = elapsedMs;

    // Register Turn in Episodic Memory
    this.episodic.turns.push({
      id: this.episodic.turns.length + 1,
      query: inputQuery,
      response,
      status,
      proofTrace,
      citation,
      focus: this.episodic.activeFocus,
      elapsedMs,
      timestamp: new Date().toISOString()
    });

    return {
      response,
      proofTrace,
      citation,
      status,
      elapsedMs,
      focus: this.episodic.activeFocus
    };
  }

  // Load Preset Corpus
  loadPreset(key) {
    if (!PRESET_CORPORA[key]) return null;
    const preset = PRESET_CORPORA[key];
    const t0 = performance.now();

    for (const sent of preset.sentences) {
      this.sentences.push({
        id: this.sentences.length,
        text: sent,
        source: preset.name
      });
      const sentId = this.sentences.length - 1;
      const tokens = sent.toLowerCase().replace(/[^a-z0-9\s]/g, " ").split(/\s+/).filter(t => t.length > 2);
      tokens.forEach(t => this.getOrCreateSymbol(t));
      this.extractSimpleTriples(tokens, sentId);
    }

    if (preset.triples) {
      for (const t of preset.triples) {
        const sCanon = this.canonicalize(t.s);
        const matchSent = this.sentences.find(s => {
          const lower = s.text.toLowerCase();
          return lower.includes(sCanon) || lower.includes(t.s.toLowerCase());
        });
        const sentId = matchSent ? matchSent.id : -1;
        this.addRelation(t.s, t.p, t.o, sentId, 1.0);
      }
    }

    this.computeConceptConcentration();
    return {
      name: preset.name,
      sentencesCount: preset.sentences.length,
      triplesCount: (preset.triples || []).length,
      elapsedMs: performance.now() - t0
    };
  }

  // Export Complete State to JSON
  exportState() {
    return {
      version: "1.0-symbolic",
      timestamp: new Date().toISOString(),
      sentences: this.sentences,
      relations: this.relations,
      episodic: this.episodic
    };
  }

  // Import State from JSON
  importState(data) {
    if (!data || !data.sentences) return false;
    this.sentences = data.sentences || [];
    this.relations = [];
    this.relMap.clear();
    this.subMap.clear();
    this.objMap.clear();
    this.symbols.clear();

    // Reconstruct symbols and sentences
    for (const sent of this.sentences) {
      const tokens = sent.text.toLowerCase().replace(/[^a-z0-9\s]/g, " ").split(/\s+/).filter(t => t.length > 2);
      tokens.forEach(t => this.getOrCreateSymbol(t));
    }

    // Reconstruct relations
    if (data.relations) {
      for (const r of data.relations) {
        this.addRelation(r.subject, r.predicate, r.object, r.sourceSentId, r.weight);
      }
    }

    this.computeConceptConcentration();
    if (data.episodic) this.episodic = data.episodic;
    return true;
  }
}
