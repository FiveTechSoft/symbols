// app.js - Web UI Controller & Live Graph Visualizer for Symbolic LLM

let engine = null;
let currentMode = "browser"; // "browser" or "server"
let serverUrl = "http://127.0.0.1:8080/v1";
let isAgenticWebSearchEnabled = true; // Auto-fallback when local graph has no ground truth
let queryHistory = []; // arrow up/down navigation
let queryHistoryIdx = -1;

// Canvas Graph State
let canvas, ctx;
let graphNodes = [];
let graphEdges = [];
let isDragging = false;
let draggedNode = null;
let activeHighlightedNode = null;
let graphZoom = 1;
let graphPanX = 0;
let graphPanY = 0;
let isPanning = false;
let panStartX = 0;
let panStartY = 0;
let selectedNode = null;

// Preset Quick Prompts Table
const PRESET_PROMPTS = {
  code: [
    { label: "🛠️ \"How does Django validate usernames?\"", query: "How does Django validate usernames?" },
    { label: "🌐 \"What is the blast radius of a function?\"", query: "What is the blast radius of a function?" },
    { label: "⚡ \"¿Cómo funciona el planificador STRIPS?\"", query: "¿Cómo funciona el planificador STRIPS?" },
    { label: "🔍 \"¿Qué extrae el Code Knowledge Graph?\"", query: "¿Qué extrae el Code Knowledge Graph?" },
    { label: "🚀 \"What is symbols-agent?\"", query: "What is symbols-agent?" }
  ],
  bible: [
    { label: "📜 \"Who is the father of David?\"", query: "Who is the father of David?" },
    { label: "👑 \"¿Quién fue el primer rey de Israel?\"", query: "¿Quién fue el primer rey de Israel?" },
    { label: "📖 \"Libros de la Biblia\"", query: "Libros de la Biblia" },
    { label: "🌊 \"Where did Jonah flee?\"", query: "Where did Jonah flee?" },
    { label: "✍️ \"¿Quién escribió Proverbios?\"", query: "¿Quién escribió Proverbios?" }
  ],
  jung: [
    { label: "🧠 \"What is libido?\"", query: "What is libido?" },
    { label: "🌳 \"What does the tree represent?\"", query: "What does the tree represent?" },
    { label: "📖 \"Who wrote the Golden Bough?\"", query: "Who wrote the Golden Bough?" },
    { label: "🎭 \"What pact did Faust make?\"", query: "What pact did Faust make?" },
    { label: "☀️ \"¿Qué representa el sol?\"", query: "¿Qué representa el sol?" }
  ],
  quantum: [
    { label: "⚛️ \"What is a qubit?\"", query: "What is a qubit?" },
    { label: "🔗 \"What does entanglement enable?\"", query: "What does entanglement enable?" },
    { label: "🧮 \"What does Shor's algorithm solve?\"", query: "What does Shor's algorithm solve?" },
    { label: "⚡ \"What is Grover's algorithm?\"", query: "What is Grover's algorithm?" },
    { label: "🌪️ \"What causes decoherence?\"", query: "What causes decoherence?" }
  ]
};

function updatePromptPills(presetKey = "code") {
  const container = document.querySelector(".pills-grid");
  if (!container) return;
  const list = PRESET_PROMPTS[presetKey] || PRESET_PROMPTS.code;
  container.innerHTML = list.map(p => 
    `<button class="prompt-pill" onclick="sendPrompt(${JSON.stringify(p.query)})">${p.label}</button>`
  ).join("");
}

function resetChatHistoryUI(presetKey = "code") {
  const history = document.getElementById("chatHistory");
  if (!history) return;
  history.innerHTML = `
    <div class="hero-pills-container">
      <div class="hero-pills-title">
        <span>⚡</span> Autonomous Deterministic Dialogue — Try asking:
      </div>
      <div class="pills-grid"></div>
    </div>
  `;
  updatePromptPills(presetKey);
}

// Initialize Web Application (WASM C11 engine)
if (typeof document !== "undefined") {
  document.addEventListener("DOMContentLoaded", async () => {
    /* Show loading state */
    const sendBtn = document.getElementById("sendBtn");
    if (sendBtn) { sendBtn.disabled = true; sendBtn.innerText = "Loading WASM..."; }

    try {
      engine = new WASMSymbolicEngine();
      await engine.init("symbolic_wasm.js", "symbolic_wasm.wasm");
      addSystemMessage("C11 WASM engine loaded (226 KB binary, 44 source modules).");
    } catch (e) {
      /* Fallback to JS engine if WASM fails */
      console.warn("WASM init failed, falling back to JS engine:", e);
      engine = new SymbolicEngine();
      addSystemMessage("WASM unavailable, using JS engine fallback.");
    }

    if (sendBtn) { sendBtn.disabled = false; sendBtn.innerText = "Execute"; }

    fetch("english-spanish.txt")
      .then(r => r.ok ? r.text() : null)
      .then(txt => { if (txt) engine.loadTranslationTable(txt); })
      .catch(() => {});
    initCanvas();
    loadStoredSession();

    const savedPreset = localStorage.getItem("symbolic_active_preset") || "code";

    if (engine.sentences.length === 0) {
      loadPresetCorpus(savedPreset);
      highlightActivePresetCard(savedPreset);
    } else {
      highlightActivePresetCard(savedPreset);
      updateUIStats();
      rebuildGraphVisualizer();
      updatePromptPills(savedPreset);
    }

    setupEventListeners();
    startCanvasLoop();
  });
}

// Setup DOM Event Listeners
function setupEventListeners() {
  const chatInput = document.getElementById("chatInput");
  const sendBtn = document.getElementById("sendBtn");
  const fileInput = document.getElementById("fileInput");
  const dropZone = document.getElementById("dropZone");

  sendBtn.addEventListener("click", () => handleUserSend());
  chatInput.addEventListener("keydown", (e) => {
    if (e.key === "Enter") {
      handleUserSend();
    } else if (e.key === "ArrowUp") {
      e.preventDefault();
      if (queryHistory.length > 0) {
        if (queryHistoryIdx < queryHistory.length - 1) queryHistoryIdx++;
        chatInput.value = queryHistory[queryHistory.length - 1 - queryHistoryIdx];
      }
    } else if (e.key === "ArrowDown") {
      e.preventDefault();
      if (queryHistoryIdx > 0) {
        queryHistoryIdx--;
        chatInput.value = queryHistory[queryHistory.length - 1 - queryHistoryIdx];
      } else if (queryHistoryIdx === 0) {
        queryHistoryIdx = -1;
        chatInput.value = "";
      }
    }
  });

  // Dropzone file upload
  dropZone.addEventListener("click", () => fileInput.click());
  dropZone.addEventListener("dragover", (e) => {
    e.preventDefault();
    dropZone.style.borderColor = "var(--accent-cyan)";
  });
  dropZone.addEventListener("dragleave", () => {
    dropZone.style.borderColor = "var(--border-bright)";
  });
  dropZone.addEventListener("drop", (e) => {
    e.preventDefault();
    dropZone.style.borderColor = "var(--border-bright)";
    if (e.dataTransfer.files.length > 0) {
      handleFileUpload(e.dataTransfer.files[0]);
    }
  });

  fileInput.addEventListener("change", (e) => {
    if (e.target.files.length > 0) {
      handleFileUpload(e.target.files[0]);
    }
  });

  // Preset buttons
  document.querySelectorAll(".preset-card").forEach(card => {
    card.addEventListener("click", () => {
      const presetKey = card.getAttribute("data-preset");
      highlightActivePresetCard(presetKey);
      // Clean previous graph data before loading selected preset
      engine.sentences = [];
      engine.relations = [];
      engine.relMap.clear();
      engine.subMap.clear();
      engine.objMap.clear();
      engine.symbols.clear();
      localStorage.setItem("symbolic_active_preset", presetKey);
      loadPresetCorpus(presetKey);
    });
  });

  // Export / Import / Clear Memory
  document.getElementById("btnExport").addEventListener("click", exportMemorySession);
  document.getElementById("btnImport").addEventListener("click", () => document.getElementById("importFileInput").click());
  document.getElementById("importFileInput").addEventListener("change", importMemorySession);
  document.getElementById("btnClear").addEventListener("click", clearMemorySession);

  // Agentic Web Tool Toggle
  const btnAgentic = document.getElementById("btnAgenticToggle");
  if (btnAgentic) {
    btnAgentic.addEventListener("click", () => {
      isAgenticWebSearchEnabled = !isAgenticWebSearchEnabled;
      btnAgentic.innerText = isAgenticWebSearchEnabled ? "⚡ Web Tool: ON" : "⚡ Web Tool: OFF";
      btnAgentic.style.borderColor = isAgenticWebSearchEnabled ? "var(--accent-cyan)" : "var(--border-muted)";
      btnAgentic.style.color = isAgenticWebSearchEnabled ? "var(--accent-cyan)" : "var(--text-muted)";
      addSystemMessage(`Agentic Web Search auto-fallback: ${isAgenticWebSearchEnabled ? 'ENABLED (Real-Time Ingestion)' : 'DISABLED (Local Graph Only)'}`);
    });
  }

  // Web search tool manual trigger if button exists
  const btnWebSearch = document.getElementById("btnWebSearchTool");
  if (btnWebSearch) {
    btnWebSearch.addEventListener("click", triggerWebSearchTool);
  }

  // Engine Mode Toggle
  const engineModeSelect = document.getElementById("engineModeSelect");
  if (engineModeSelect) {
    engineModeSelect.addEventListener("change", (e) => {
      currentMode = e.target.value;
      if (currentMode === "server") {
        addSystemMessage(`Switched execution mode to: Local C11 Server (${serverUrl}). Ensure './symbols_server 8080' is running in your terminal.`);
      } else {
        addSystemMessage(`Switched execution mode to: Client-Side In-Browser Engine (Deterministic WebAssembly / JS).`);
      }
    });
  }

  // Mobile Sidebar Toggle
  const btnToggleSidebar = document.getElementById("btnToggleSidebar");
  if (btnToggleSidebar) {
    btnToggleSidebar.addEventListener("click", () => {
      const sidebar = document.getElementById("sidebarLeft");
      if (sidebar) sidebar.classList.toggle("open");
    });
  }
}

// Helper to highlight active preset card in sidebar
function highlightActivePresetCard(presetKey) {
  document.querySelectorAll(".preset-card").forEach(c => {
    if (c.getAttribute("data-preset") === presetKey) {
      c.classList.add("active");
    } else {
      c.classList.remove("active");
    }
  });
}

// Load Preset Corpus
function loadPresetCorpus(presetKey) {
  const res = engine.loadPreset(presetKey);
  if (res) {
    saveSessionToStorage();
    updateUIStats();
    rebuildGraphVisualizer();
    updatePromptPills(presetKey);
    addSystemMessage(`Loaded corpus '${res.name}': ${res.sentencesCount} sentences & ${res.triplesCount} triples indexed in ${res.elapsedMs.toFixed(2)} ms.`);
  }
}

// Handle Plain Text File Ingestion
function handleFileUpload(file) {
  const reader = new FileReader();
  reader.onload = (e) => {
    const text = e.target.result;
    const res = engine.ingestText(text, file.name);
    saveSessionToStorage();
    updateUIStats();
    rebuildGraphVisualizer();
    addSystemMessage(`Ingested '${file.name}': ${res.sentencesAdded} sentences & ${res.symbolsAdded} unique symbols incorporated into graph in ${res.elapsedMs.toFixed(2)} ms.`);
  };
  reader.readAsText(file);
}

// Send Message
async function handleUserSend() {
  const chatInput = document.getElementById("chatInput");
  const query = chatInput.value.trim();
  if (!query) return;

  chatInput.value = "";
  queryHistory.push(query);
  queryHistoryIdx = -1;
  appendMessage("user", query);

  // Live HUD animation
  document.getElementById("hudLatency").innerText = "…";

  if (currentMode === "server") {
    // Forward query to C11 symbols-server
    try {
      const t0 = performance.now();
      const resp = await fetch(`${serverUrl}/chat/completions`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          model: "symbolic-llm-c11",
          messages: [{ role: "user", content: query }]
        })
      });
      const data = await resp.json();
      const dt = performance.now() - t0;
      const content = data.choices[0].message.content;
      appendMessage("assistant", content, {
        status: "C11_SERVER_RESPONSE",
        elapsedMs: dt,
        citation: "symbols-server (C11 native engine)"
      });
    } catch (err) {
      appendMessage("assistant", `Error connecting to local C11 server at ${serverUrl}: ${err.message}. Falling back to in-browser engine.`, {
        status: "SERVER_UNREACHABLE"
      });
      // Fallback
      await executeInBrowserEngine(query);
    }
  } else {
    await executeInBrowserEngine(query);
  }
}

// Extract semantic topic entity by stripping conversational preambles and pronouns
function extractSearchTopic(query, activeFocus = null) {
  if (!query) return activeFocus ? activeFocus.replace(/_/g, " ").trim() : "";
  let clean = query
    .trim()
    .normalize("NFD")
    .replace(/[\u0300-\u036f]/g, "")
    .replace(/[?¿!¡;:,"'.()]/g, " ")
    .replace(/\s+/g, " ")
    .trim();

  const preambles = [
    // Spanish
    /^(que\s+mas\s+(puedes\s+)?(decir(me)?|contar(me)?|sabes|hay)\s*(acerca\s+de|sobre|de)?)/i,
    /^(que\s+sabes\s+(acerca\s+de|sobre|de)?)/i,
    /^(quien(es)?\s+(es|son|fue|fueron)?\s*(el|la|los|las|su|sus|de|del)?)/i,
    /^(who\s+(is|are|was|were)\s*(the|a|an)?)/i,
    /^(que\s+es\s+(un|una|el|la)?)/i,
    /^(cual(es)?\s+(es|son)?\s*(el|la|los|las|su|sus|de|del)?)/i,
    /^(para\s+que\s+sirve\s*(el|la|su)?)/i,
    /^(que\s+proposito\s+tiene\s*(el|la|su)?)/i,
    /^(hablame\s+(mas\s+)?(acerca\s+de|sobre|de)?)/i,
    /^(cuentame\s+(mas\s+)?(acerca\s+de|sobre|de)?)/i,
    /^(dime\s+(mas\s+)?(acerca\s+de|sobre|de)?)/i,
    /^(explicame\s+(mas\s+)?(acerca\s+de|sobre|de)?)/i,
    /^(de\s+que\s+(trata|habla|va))\s*(el|la|los|las|de)?/i,
    /^(puedes\s+decir(me)?\s+(mas\s+)?(acerca\s+de|sobre|de)?)/i,
    /^(acerca\s+de\s+)/i,
    /^(sobre\s+)/i,
    // English
    /^(what\s+else\s+can\s+you\s+tell\s+me\s+(about)?)/i,
    /^(what\s+can\s+you\s+tell\s+me\s+(about)?)/i,
    /^(what\s+(is|are)\s*(the|its|their|a|an)?)/i,
    /^(tell\s+me\s+(more\s+)?(about)?)/i,
    /^(what\s+(do\s+you\s+know\s+about|is\s+it\s+about))/i,
    /^(can\s+you\s+tell\s+me\s+(about)?)/i
  ];
  for (const pat of preambles) {
    if (pat.test(clean)) {
      clean = clean.replace(pat, "").trim();
      break;
    }
  }

  clean = clean
    .replace(/^(y|and|e)\s+/i, "")
    .replace(/^(el\s+)?libro\s+de\s+/i, "")
    .replace(/^libro\s+(?=[a-z])/i, "")
    .replace(/\b(acerca\s+de|respecto\s+a|en\s+cuanto\s+a|sobre|de|del|a|al|para|por|con|en|hacia)\s+(el|ella|ellos|ellas|esto|eso|aquello|este|esta)\b/gi, "")
    .replace(/\b(about|of|to|for|with|in|towards|regarding)\s+(it|him|her|them|this|that)\b/gi, "")
    .replace(/\b(al\s+respecto|al\s+caso|al\s+tema|sobre\s+ello|acerca\s+de\s+ello|respecto\s+a\s+ello)\b/gi, "")
    .trim();

  clean = clean.replace(/^(su|sus|his|her|its)\s+/i, "").trim();

  const pronounOnly = /^(a\s+)?(el|ella|ellos|ellas|esto|eso|aquello|este|esta|al|it|him|her|them|this|that)$/i;
  const cliticVerbs = /^(explica(lo|la|los|las|me|melo|mela)?|describe(lo|la|los|las|me|melo|mela)?|cuenta(lo|la|los|las|me|melo|mela)?|dime(lo)?|aclara(lo|la|los|las)?|detalla(lo|la)?|desarrolla(lo|la)?|continua(lo|la)?|sigue(lo|la)?|hazlo|muestraw*(lo|la)?|explain(\s+(it|this|that))?|describe(\s+(it|this|that))?|elaborate(\s+on\s+(it|this|that))?|continue|proceed|go\s+on)$/i;
  const propertyWords = /^(proposito|propositos|funcion|funciones|significado|origen|autor|autoria|historia|purpose|function|meaning|origin|author)$/i;
  const kinshipWords = /^(hijo|hija|hijos|padre|madre|padres|hermano|hermana|son|daughter|father|mother|brother|sister)$/i;

  const isAnaphoricWord = propertyWords.test(clean) || kinshipWords.test(clean);

  if (!clean || pronounOnly.test(clean) || cliticVerbs.test(clean) || (isAnaphoricWord && activeFocus)) {
    return activeFocus ? activeFocus.replace(/_/g, " ").trim() : "";
  }
  clean = clean.replace(/^(el|la|los|las|un|una|the|a|an)\s+/i, "").trim();
  if (!clean || cliticVerbs.test(clean) || (isAnaphoricWord && activeFocus)) {
    return activeFocus ? activeFocus.replace(/_/g, " ").trim() : "";
  }
  return clean;
}

// Detect if query or conversational context is Spanish
function detectIsSpanish(query, targetTerm) {
  if (/[áéíóúñ¿¡]/i.test(query) || /[áéíóúñ¿¡]/i.test(targetTerm)) return true;

  const spanishWords = /\b(de|la|el|los|las|un|una|unos|unas|en|que|cual|cuales|quien|quienes|su|sus|mi|mis|tu|tus|por|para|con|sin|como|donde|cuando|sobre|entre|tras|hasta|desde|hacia|es|son|era|eran|fue|fueron|hay|tiene|tienen|proposito|significado|origen|funcion|autor|autoria|historia|tema|energia|psiquica|psiquico|arquetipo|simbolo|inconsciente|libros|biblia|proverbios|salomon|sol)\b/i;
  if (spanishWords.test(query) || spanishWords.test(targetTerm)) return true;

  const spanishSuffixes = /(cion|ciones|dad|dades|ica|ico|icas|icos|ismo|ismos|ista|istas|mente)\b/i;
  if (spanishSuffixes.test(query) || spanishSuffixes.test(targetTerm)) return true;

  if (typeof engine !== "undefined" && engine?.episodic?.turns?.length > 0) {
    const lastTurns = engine.episodic.turns.slice(-4);
    for (const t of lastTurns) {
      if (/[áéíóúñ¿¡]/i.test(t.query) || spanishWords.test(t.query)) {
        return true;
      }
    }
  }

  return false;
}

// Verify that candidate title actually shares meaningful tokens with search query
function isTitleRelevant(title, queryTerm) {
  if (!title || !queryTerm) return false;
  const tNorm = title.toLowerCase().normalize("NFD").replace(/[\u0300-\u036f]/g, "").replace(/[^a-z0-9]/g, " ");
  const qNorm = queryTerm.toLowerCase().normalize("NFD").replace(/[\u0300-\u036f]/g, "").replace(/[^a-z0-9]/g, " ");
  const qWords = qNorm.split(/\s+/).filter(w => w.length > 2);
  if (qWords.length === 0) return true;
  return qWords.some(w => tNorm.includes(w) || w.includes(tNorm));
}

// In-Browser Engine Execution with Autonomous Agentic Web Search Fallback
async function executeInBrowserEngine(query) {
  let res = engine.query(query);

  // If local knowledge graph returns UNKNOWN_FAIL_CLOSED and agenticWebSearch is enabled:
  if (res.status === "UNKNOWN_FAIL_CLOSED" && isAgenticWebSearchEnabled) {
    const topic = extractSearchTopic(query, engine?.episodic?.activeFocus);
    addSystemMessage(`🔍 No-evidence gate triggered: No local ground truth for "${query}". Invoking Real-Time Web Search Tool${topic ? ` (Target: '${topic}')` : ""}...`);
    const webResult = await fetchWebKnowledge(query, topic);

    if (webResult && webResult.text) {
      const ing = engine.ingestText(webResult.text, `WebSearch: ${webResult.title}`);
      
      saveSessionToStorage();
      updateUIStats();
      rebuildGraphVisualizer();

      addSystemMessage(`📥 Knowledge assimilated in ${ing.elapsedMs.toFixed(2)} ms (+${ing.sentencesAdded} sentences, +${ing.symbolsAdded} symbols from '${webResult.title}'). Re-evaluating query...`);

      let secondPass = engine.query(query);
      if (secondPass.status === "UNKNOWN_FAIL_CLOSED" && topic && topic !== query.trim().toLowerCase()) {
        secondPass = engine.query(topic);
      }
      if (secondPass.status !== "UNKNOWN_FAIL_CLOSED") {
        secondPass.status = "AGENTIC_WEB_GROUNDED";
        if (!secondPass.citation) {
          secondPass.citation = `Web Source: Wikipedia ("${webResult.title}") [${webResult.url}]`;
        }
        res = secondPass;
      }
    } else {
      addSystemMessage(`⚠ Web search returned no verified ground truth for "${query}". Preserving honest fail-closed unknown.`);
    }
  }

  saveSessionToStorage();
  updateUIStats();

  // Highlight graph node if focus changed
  if (res.focus) {
    highlightGraphNode(res.focus);
  }

  appendMessage("assistant", res.response, {
    status: res.status,
    proofTrace: res.proofTrace,
    citation: res.citation,
    elapsedMs: res.elapsedMs
  });
}

// Autonomous Web Knowledge Retrieval (Wikipedia API, CORS enabled with origin=*)
async function fetchWebKnowledge(query, topic = null) {
  try {
    const focus = engine?.episodic?.activeFocus;
    const targetTerm = topic || extractSearchTopic(query, focus) || query.replace(/[?¿!¡]/g, "").trim();

    const isSpanish = detectIsSpanish(query, targetTerm);
    const primaryLang = isSpanish ? "es" : "en";
    const fallbackLang = isSpanish ? "en" : "es";

    let result = await searchWiki(targetTerm, primaryLang);
    if (!result) {
      result = await searchWiki(targetTerm, fallbackLang);
    }
    return result;
  } catch (e) {
    console.warn("fetchWebKnowledge error:", e);
    return null;
  }
}

async function searchWiki(term, lang) {
  try {
    let candidateTitle = null;

    // 1. Try opensearch first (exact title prefix matching)
    try {
      const openUrl = `https://${lang}.wikipedia.org/w/api.php?action=opensearch&search=${encodeURIComponent(term)}&limit=5&format=json&origin=*`;
      const openRes = await fetch(openUrl);
      if (openRes.ok) {
        const openData = await openRes.json();
        const titles = openData[1] || [];
        const relevant = titles.find(t => isTitleRelevant(t, term));
        if (relevant) {
          candidateTitle = relevant;
        }
      }
    } catch (e) {
      // ignore opensearch error, proceed to fallback
    }

    // 1b. If opensearch gave no exact title and term is a multi-word compound, try its head noun
    if (!candidateTitle && term.includes(" ")) {
      const parts = term.trim().split(/\s+/).filter(w => w.length > 2);
      const headNoun = parts[parts.length - 1];
      if (headNoun && headNoun !== term) {
        try {
          const openUrl = `https://${lang}.wikipedia.org/w/api.php?action=opensearch&search=${encodeURIComponent(headNoun)}&limit=5&format=json&origin=*`;
          const openRes = await fetch(openUrl);
          if (openRes.ok) {
            const openData = await openRes.json();
            const titles = openData[1] || [];
            const relevant = titles.find(t => isTitleRelevant(t, headNoun));
            if (relevant) {
              candidateTitle = relevant;
            }
          }
        } catch (e) {}
      }
    }

    // 2. Fallback to list=search if opensearch gave nothing
    if (!candidateTitle) {
      const url = `https://${lang}.wikipedia.org/w/api.php?action=query&list=search&srsearch=${encodeURIComponent(term)}&format=json&origin=*`;
      const res = await fetch(url);
      if (res.ok) {
        const data = await res.json();
        if (data.query && data.query.search && data.query.search.length > 0) {
          const hit = data.query.search.find(item => isTitleRelevant(item.title, term));
          if (hit) {
            candidateTitle = hit.title;
          }
        }
      }
    }

    if (candidateTitle) {
      const sumUrl = `https://${lang}.wikipedia.org/api/rest_v1/page/summary/${encodeURIComponent(candidateTitle.replace(/\s+/g, "_"))}`;
      const sumRes = await fetch(sumUrl);
      if (sumRes.ok) {
        const sumData = await sumRes.json();
        if (sumData.extract) {
          return {
            title: sumData.title || candidateTitle,
            text: sumData.extract,
            url: sumData.content_urls ? sumData.content_urls.desktop.page : `https://${lang}.wikipedia.org/wiki/${encodeURIComponent(candidateTitle)}`,
            lang
          };
        }
      }
    }
    return null;
  } catch (err) {
    console.warn("searchWiki error:", err);
    return null;
  }
}

// Append Message to UI
function appendMessage(role, text, meta = {}) {
  const history = document.getElementById("chatHistory");
  const msgDiv = document.createElement("div");
  msgDiv.className = `chat-message ${role}`;

  let innerHTML = `<div class="message-bubble">${escapeHtml(text)}`;

  if (meta.proofTrace && meta.proofTrace.length > 0) {
    innerHTML += `<div class="proof-trace-box"><strong>PROOF TRACE:</strong><br>${meta.proofTrace.map(p => escapeHtml(p)).join("<br>")}</div>`;
  }

  if (meta.citation) {
    innerHTML += `<div class="citation-box">${escapeHtml(meta.citation)}</div>`;
  }

  innerHTML += `</div>`;

  if (role === "assistant" && meta.status) {
    const hud = document.getElementById("hudLatency");
    if (hud) hud.innerText = meta.elapsedMs ? `${meta.elapsedMs.toFixed(3)} ms` : "n/a";
    const isUnknown = meta.status.includes("UNKNOWN");
    innerHTML += `
      <div class="message-meta">
        <span class="meta-pill ${isUnknown ? 'unknown' : ''}">${meta.status}</span>
        <span>Latency: ${meta.elapsedMs ? meta.elapsedMs.toFixed(3) + ' ms' : 'n/a'}</span>
      </div>
    `;
  }

  msgDiv.innerHTML = innerHTML;
  history.appendChild(msgDiv);
  setTimeout(() => {
    history.scrollTop = history.scrollHeight;
  }, 10);
}

function addSystemMessage(text) {
  const history = document.getElementById("chatHistory");
  const div = document.createElement("div");
  div.style.textAlign = "center";
  div.style.fontSize = "11px";
  div.style.color = "var(--text-muted)";
  div.style.fontFamily = "var(--font-mono)";
  div.style.margin = "8px 0";
  div.innerText = `[system] ${text}`;
  history.appendChild(div);
  setTimeout(() => {
    history.scrollTop = history.scrollHeight;
  }, 10);
}

// Real Autonomous Web Search Tool & Dynamic Ingestion
async function triggerWebSearchTool() {
  const topic = prompt("Enter web search topic or entity to acquire and assimilate into the graph:", "James Webb Space Telescope");
  if (!topic) return;

  addSystemMessage(`🌐 Autonomous Real-Time Web Tool: Searching Wikipedia for '${topic}'...`);
  const webResult = await fetchWebKnowledge(topic, topic);
  if (webResult && webResult.text) {
    const res = engine.ingestText(webResult.text, `WebSearch: ${webResult.title}`);
    saveSessionToStorage();
    updateUIStats();
    rebuildGraphVisualizer();
    addSystemMessage(`✅ Ingested '${webResult.title}' from Wikipedia in ${res.elapsedMs.toFixed(2)} ms (+${res.sentencesAdded} sentences, +${res.symbolsAdded} symbols). You can now ask questions about '${webResult.title}' grounded in that text.`);
  } else {
    addSystemMessage(`⚠ Could not retrieve verified Wikipedia article for '${topic}'. Preserving honest fail-closed unknown.`);
  }
}

// Quick Prompt click
function sendPrompt(text) {
  document.getElementById("chatInput").value = text;
  handleUserSend();
}

// Update UI Statistics
function updateUIStats() {
  document.getElementById("statSentences").innerText = engine.sentences.length;
  document.getElementById("statSymbols").innerText = engine.symbols.size;
  document.getElementById("statRelations").innerText = engine.relations.length;
  document.getElementById("statTurns").innerText = engine.episodic.turns.length;

  const focusEl = document.getElementById("activeFocusDisplay");
  if (focusEl) {
    focusEl.innerText = engine.episodic.activeFocus ? engine.episodic.activeFocus.toUpperCase() : "None";
  }
}

// LocalStorage Persistence
function saveSessionToStorage() {
  try {
    const state = engine.exportState();
    localStorage.setItem("symbolic_llm_state", JSON.stringify(state));
  } catch (e) {
    console.warn("Storage write quota or disabled:", e);
  }
}

function loadStoredSession() {
  try {
    const saved = localStorage.getItem("symbolic_llm_state");
    if (saved) {
      const data = JSON.parse(saved);
      if (engine.importState(data)) {
        addSystemMessage(`Restored previous session from localStorage (${engine.sentences.length} sentences, ${engine.episodic.turns.length} turns).`);
      }
    }
  } catch (e) {
    console.warn("Could not load stored session:", e);
  }
}

// Export Session File Download
function exportMemorySession() {
  const state = engine.exportState();
  const blob = new Blob([JSON.stringify(state, null, 2)], { type: "application/json" });
  const url = URL.createObjectURL(blob);
  const a = document.createElement("a");
  a.href = url;
  a.download = `symbolic_llm_memory_${new Date().toISOString().slice(0,10)}.json`;
  a.click();
  URL.revokeObjectURL(url);
  addSystemMessage("Downloaded full episodic memory snapshot (.json). Zero information lost.");
}

// Import Session File
function importMemorySession(e) {
  const file = e.target.files[0];
  if (!file) return;
  const reader = new FileReader();
  reader.onload = (event) => {
    try {
      const data = JSON.parse(event.target.result);
      if (engine.importState(data)) {
        saveSessionToStorage();
        updateUIStats();
        rebuildGraphVisualizer();
        addSystemMessage(`Successfully imported memory snapshot: ${engine.sentences.length} sentences & ${engine.relations.length} relations.`);
      }
    } catch (err) {
      alert("Invalid JSON session file.");
    }
  };
  reader.readAsText(file);
}

// Clear Memory
function clearMemorySession() {
  if (confirm("Reset and clear episodic memory and active knowledge base?")) {
    localStorage.removeItem("symbolic_llm_state");
    if (engine instanceof WASMSymbolicEngine) {
      engine._module._wasm_reset();
      engine._module._wasm_init(0);
      engine.sentences = [];
      engine.relations = [];
      engine.relMap.clear();
      engine.subMap.clear();
      engine.objMap.clear();
      engine.symbols.clear();
      engine.episodic = { turns: [], activeFocus: null, citedSentIds: new Set(), learnedFacts: [], lastQueryTimeMs: 0 };
    } else {
      engine = new SymbolicEngine();
    }
    const currentPreset = localStorage.getItem("symbolic_active_preset") || "code";
    loadPresetCorpus(currentPreset);
    resetChatHistoryUI(currentPreset);
    addSystemMessage("Memory reset to zero. Active preset reloaded.");
  }
}

// ==========================================
// Canvas Interactive Concept Graph
// ==========================================
function initCanvas() {
  canvas = document.getElementById("graphCanvas");
  ctx = canvas.getContext("2d");

  function resize() {
    canvas.width = canvas.parentElement.clientWidth;
    canvas.height = canvas.parentElement.clientHeight;
  }
  window.addEventListener("resize", resize);
  resize();

  /* Convert screen coords to graph coords (accounting for zoom + pan) */
  function screenToGraph(sx, sy) {
    return {
      x: (sx - graphPanX) / graphZoom,
      y: (sy - graphPanY) / graphZoom
    };
  }

  /* Find node under screen coords */
  function nodeAtScreen(sx, sy) {
    const g = screenToGraph(sx, sy);
    for (let i = graphNodes.length - 1; i >= 0; i--) {
      const n = graphNodes[i];
      const dx = n.x - g.x;
      const dy = n.y - g.y;
      if (Math.sqrt(dx * dx + dy * dy) < n.radius + 6) return n;
    }
    return null;
  }

  /* Hover cursor */
  canvas.addEventListener("mousemove", (e) => {
    if (isPanning) {
      graphPanX += e.movementX;
      graphPanY += e.movementY;
      return;
    }
    if (isDragging && draggedNode) {
      const rect = canvas.getBoundingClientRect();
      const g = screenToGraph(e.clientX - rect.left, e.clientY - rect.top);
      draggedNode.x = g.x;
      draggedNode.y = g.y;
      draggedNode.fx = g.x;
      draggedNode.fy = g.y;
      return;
    }
    const rect = canvas.getBoundingClientRect();
    const hovered = nodeAtScreen(e.clientX - rect.left, e.clientY - rect.top);
    canvas.style.cursor = hovered ? "pointer" : "grab";
  });

  /* Mouse down: start drag node, start pan, or click node */
  canvas.addEventListener("mousedown", (e) => {
    const rect = canvas.getBoundingClientRect();
    const mx = e.clientX - rect.left;
    const my = e.clientY - rect.top;
    const hit = nodeAtScreen(mx, my);

    if (hit) {
      /* Drag node */
      isDragging = true;
      draggedNode = hit;
      const g = screenToGraph(mx, my);
      hit.fx = g.x;
      hit.fy = g.y;
      canvas.style.cursor = "grabbing";
    } else {
      /* Pan background */
      isPanning = true;
      panStartX = mx;
      panStartY = my;
      canvas.style.cursor = "grabbing";
    }
  });

  /* Mouse up: release drag/pan, detect click on node */
  canvas.addEventListener("mouseup", (e) => {
    const rect = canvas.getBoundingClientRect();
    const mx = e.clientX - rect.left;
    const my = e.clientY - rect.top;

    if (isDragging && draggedNode) {
      /* Was a drag — check if it was actually a click (minimal movement) */
      const dx = mx - panStartX;
      const dy = my - panStartY;
      if (Math.sqrt(dx * dx + dy * dy) < 5) {
        /* Click on node → select it */
        selectGraphNode(draggedNode);
      }
      draggedNode.fx = null;
      draggedNode.fy = null;
      isDragging = false;
      draggedNode = null;
    }

    if (isPanning) {
      isPanning = false;
    }
    canvas.style.cursor = "grab";
  });

  /* Double-click: reset zoom and pan */
  canvas.addEventListener("dblclick", () => {
    graphZoom = 1;
    graphPanX = 0;
    graphPanY = 0;
  });

  /* Mouse wheel: zoom in/out */
  canvas.addEventListener("wheel", (e) => {
    e.preventDefault();
    const rect = canvas.getBoundingClientRect();
    const mx = e.clientX - rect.left;
    const my = e.clientY - rect.top;
    const delta = e.deltaY > 0 ? 0.9 : 1.1;
    const newZoom = Math.max(0.2, Math.min(5, graphZoom * delta));
    /* Zoom toward cursor position */
    graphPanX = mx - (mx - graphPanX) * (newZoom / graphZoom);
    graphPanY = my - (my - graphPanY) * (newZoom / graphZoom);
    graphZoom = newZoom;
  }, { passive: false });
}

function rebuildGraphVisualizer() {
  graphNodes = [];
  graphEdges = [];
  const topConcepts = engine.getTopConcepts(16);
  const nodeMap = new Map();

  const width = canvas.width || 340;
  const height = canvas.height || 400;
  const cx = width / 2;
  const cy = height / 2;

  // Position nodes radially
  topConcepts.forEach((c, idx) => {
    const angle = (idx / topConcepts.length) * Math.PI * 2;
    const r = Math.min(width, height) * 0.35;
    const node = {
      id: c.name,
      label: c.name.toUpperCase(),
      x: cx + Math.cos(angle) * r + (Math.random() - 0.5) * 20,
      y: cy + Math.sin(angle) * r + (Math.random() - 0.5) * 20,
      vx: 0,
      vy: 0,
      radius: Math.max(6, Math.min(14, 6 + c.kappa * 4)),
      isHighlighted: false
    };
    graphNodes.push(node);
    nodeMap.set(c.name, node);
  });

  // Extract edges among top concepts
  for (const rel of engine.relations) {
    if (nodeMap.has(rel.subject) && nodeMap.has(rel.object)) {
      graphEdges.push({
        source: nodeMap.get(rel.subject),
        target: nodeMap.get(rel.object),
        label: rel.predicate
      });
    }
  }
}

function highlightGraphNode(name) {
  const canon = engine.canonicalize(name);
  graphNodes.forEach(n => {
    n.isHighlighted = (n.id === canon);
  });
}

function selectGraphNode(node) {
  selectedNode = (selectedNode === node) ? null : node;
  if (!selectedNode) {
    updatePromptPills("code");
    return;
  }

  /* Find connected relations for the selected node */
  const name = selectedNode.id;
  const related = [];
  for (const rel of engine.relations) {
    if (rel.subject === name && related.length < 4) {
      related.push({ label: `${rel.predicate} ${rel.object}`, query: `What is the relation between ${rel.subject} and ${rel.object}?` });
    } else if (rel.object === name && related.length < 6) {
      related.push({ label: `who/what ${rel.predicate} ${rel.subject}?`, query: `Who or what relates to ${rel.subject} via ${rel.predicate}?` });
    }
  }

  /* Add node frequency as context */
  const concepts = engine.getTopConcepts(50);
  const entry = concepts.find(c => c.name === name);
  if (entry) {
    related.push({ label: `${name} (freq: ${entry.frequency})`, query: `Tell me about ${name}` });
  }

  if (related.length === 0) {
    related.push({ label: `Explore ${name}`, query: `Tell me about ${name}` });
  }

  /* Update pills with related prompts */
  const container = document.querySelector(".pills-grid");
  if (container) {
    container.innerHTML = related.slice(0, 6).map(p =>
      `<button class="prompt-pill" onclick="sendPrompt(${JSON.stringify(p.query)})">${p.label}</button>`
    ).join("");
  }
}

function startCanvasLoop() {
  function tick() {
    updatePhysics();
    renderGraph();
    requestAnimationFrame(tick);
  }
  requestAnimationFrame(tick);
}

function updatePhysics() {
  const width = canvas.width;
  const height = canvas.height;
  const cx = width / 2;
  const cy = height / 2;

  // Repulsion between nodes
  for (let i = 0; i < graphNodes.length; i++) {
    for (let j = i + 1; j < graphNodes.length; j++) {
      const a = graphNodes[i];
      const b = graphNodes[j];
      const dx = b.x - a.x;
      const dy = b.y - a.y;
      const dist = Math.sqrt(dx * dx + dy * dy) || 1;
      if (dist < 120) {
        const force = (120 - dist) / 120 * 0.6;
        const fx = (dx / dist) * force;
        const fy = (dy / dist) * force;
        if (!a.fx) { a.vx -= fx; a.vy -= fy; }
        if (!b.fx) { b.vx += fx; b.vy += fy; }
      }
    }
  }

  // Edge springs
  for (const edge of graphEdges) {
    const dx = edge.target.x - edge.source.x;
    const dy = edge.target.y - edge.source.y;
    const dist = Math.sqrt(dx * dx + dy * dy) || 1;
    const force = (dist - 80) * 0.005;
    const fx = (dx / dist) * force;
    const fy = (dy / dist) * force;
    if (!edge.source.fx) { edge.source.vx += fx; edge.source.vy += fy; }
    if (!edge.target.fx) { edge.target.vx -= fx; edge.target.vy -= fy; }
  }

  // Center gravity & damping
  for (const n of graphNodes) {
    if (n.fx) continue;
    n.vx += (cx - n.x) * 0.002;
    n.vy += (cy - n.y) * 0.002;
    n.vx *= 0.88;
    n.vy *= 0.88;
    n.x += n.vx;
    n.y += n.vy;
  }
}

function renderGraph() {
  ctx.clearRect(0, 0, canvas.width, canvas.height);
  ctx.save();
  ctx.translate(graphPanX, graphPanY);
  ctx.scale(graphZoom, graphZoom);

  // Draw edges
  ctx.lineWidth = 1;
  for (const edge of graphEdges) {
    ctx.strokeStyle = "rgba(45, 66, 100, 0.6)";
    ctx.beginPath();
    ctx.moveTo(edge.source.x, edge.source.y);
    ctx.lineTo(edge.target.x, edge.target.y);
    ctx.stroke();
  }

  // Draw nodes
  for (const node of graphNodes) {
    ctx.beginPath();
    ctx.arc(node.x, node.y, node.radius, 0, Math.PI * 2);

    if (node === selectedNode) {
      ctx.fillStyle = "#ffcc00";
      ctx.shadowColor = "#ffcc00";
      ctx.shadowBlur = 18;
    } else if (node.isHighlighted) {
      ctx.fillStyle = "#00ff88";
      ctx.shadowColor = "#00ff88";
      ctx.shadowBlur = 15;
    } else {
      ctx.fillStyle = "#00f0ff";
      ctx.shadowColor = "rgba(0, 240, 255, 0.4)";
      ctx.shadowBlur = 8;
    }
    ctx.fill();
    ctx.shadowBlur = 0;

    // Node label
    ctx.fillStyle = "#e6edf3";
    ctx.font = "9px 'JetBrains Mono', monospace";
    ctx.textAlign = "center";
    ctx.fillText(node.label, node.x, node.y + node.radius + 11);
  }

  ctx.restore();
}

function escapeHtml(text) {
  if (typeof document !== "undefined") {
    const div = document.createElement("div");
    div.innerText = text;
    return div.innerHTML;
  }
  return text.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
}

if (typeof module !== "undefined" && module.exports) {
  module.exports = { extractSearchTopic, detectIsSpanish, isTitleRelevant };
}
