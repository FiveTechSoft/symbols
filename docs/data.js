// data.js - Prepackaged Knowledge Corpora for Instant Browser Exploration

const PRESET_CORPORA = {
  jung: {
    name: "C.G. Jung: Psychology of the Unconscious",
    category: "Psychology & Symbolism",
    description: "Seminal treatise on archetypal symbols, unconscious libido, and mythological representations.",
    sentences: [
      "Quoted from Frazer: 'Golden Bough', Part IV, p. 115 regarding ancient religious rites.",
      "This especial tree seems simply to continue the category of the mother symbols.",
      "Pausanias mentions a related myth fragment where the statue of Artemis Orthia is also called Lygodesma, because it was found in a willow tree.",
      "Demeter had unsuspectingly eaten the shoulder from this feast, when Zeus discovered the outrage.",
      "The symbol of the mother is the maternal archetype of the collective unconscious.",
      "Libido is psychic energy that manifests through archaic symbols and dreams.",
      "Faust enters into a pact with Mephistopheles to attain transcendent worldly experience.",
      "The unconscious mind retains forgotten memories and ancestral symbolic archetypes.",
      "The sun represents the supreme life-giving libido in primitive solar mythologies.",
      "Transformation of the libido occurs through mythological rituals and heroic journeys."
    ],
    triples: [
      { s: "TREE", p: "IS_A", o: "MOTHER_SYMBOL" },
      { s: "ARTEMIS", p: "CALLED", o: "LYGODESMA" },
      { s: "LIBIDO", p: "DEFINED_AS", o: "PSYCHIC_ENERGY" },
      { s: "FAUST", p: "PACT_WITH", o: "MEPHISTOPHELES" },
      { s: "SUN", p: "REPRESENTS", o: "LIFE_GIVING_LIBIDO" },
      { s: "GOLDEN_BOUGH", p: "AUTHORED_BY", o: "FRAZER" },
      { s: "DEMETER", p: "WORSHIPPER_OF", o: "ZEUS" }
    ]
  },
  bible: {
    name: "Biblia (Genealogías, Reyes y Libros Canónicos)",
    category: "Historical & Theological",
    description: "Canon bíblico de 66 libros, genealogías cronológicas y linajes reales.",
    sentences: [
      "Jesse begat David the king, who ruled over Israel.",
      "David begat Solomon of her that had been the wife of Uriah.",
      "Solomon begat Rehoboam, and Rehoboam begat Abijah.",
      "Abraham begat Isaac, and Isaac begat Jacob.",
      "Jacob begat Judah and his brethren.",
      "Boaz begat Obed of Ruth, and Obed begat Jesse.",
      "The Lord sent a great wind into the sea, and there was a mighty tempest.",
      "Jonah rose up to flee unto Tarshish from the presence of the Lord.",
      "Saul was the first king of Israel before David.",
      "Samuel anointed David to be king in the midst of his brethren.",
      "Los libros de la Biblia son 66 libros canonicos divididos en el Antiguo Testamento con 39 libros y el Nuevo Testamento con 27 libros.",
      "Los libros del Antiguo Testamento son Genesis, Exodo, Levitico, Numeros, Deuteronomio, Josue, Jueces, Rut, 1 Samuel, 2 Samuel, 1 Reyes, 2 Reyes, 1 Cronicas, 2 Cronicas, Esdras, Nehemias, Ester, Job, Salmos, Proverbios, Eclesiastes, Cantares, Isaias, Jeremias, Lamentaciones, Ezequiel, Daniel, Oseas, Joel, Amos, Abdias, Jonas, Miqueas, Nahum, Habacuc, Sofonias, Hageo, Zacarias y Malaquias.",
      "Los libros del Nuevo Testamento son Mateo, Marcos, Lucas, Juan, Hechos, Romanos, 1 Corintios, 2 Corintios, Galatas, Efesios, Filipenses, Colosenses, 1 Tesalonicenses, 2 Tesalonicenses, 1 Timoteo, 2 Timoteo, Tito, Filemon, Hebreos, Santiago, 1 Pedro, 2 Pedro, 1 Juan, 2 Juan, 3 Juan, Judas y Apocalipsis.",
      "The Bible contains 66 canonical books divided into the Old Testament (39 books) and the New Testament (27 books).",
      "The books of the Old Testament are Genesis, Exodus, Leviticus, Numbers, Deuteronomy, Joshua, Judges, Ruth, 1 Samuel, 2 Samuel, 1 Kings, 2 Kings, 1 Chronicles, 2 Chronicles, Ezra, Nehemiah, Esther, Job, Psalms, Proverbs, Ecclesiastes, Song of Solomon, Isaiah, Jeremiah, Lamentations, Ezekiel, Daniel, Hosea, Joel, Amos, Obadiah, Jonah, Micah, Nahum, Habakkuk, Zephaniah, Haggai, Zechariah, and Malachi.",
      "The books of the New Testament are Matthew, Mark, Luke, John, Acts, Romans, 1 Corinthians, 2 Corinthians, Galatians, Ephesians, Philippians, Colossians, 1 Thessalonians, 2 Thessalonians, 1 Timothy, 2 Timothy, Titus, Philemon, Hebrews, James, 1 Peter, 2 Peter, 1 John, 2 John, 3 John, Jude, and Revelation.",
      "La tradicion biblica atribuye al rey Salomon la autoria de los libros que escribio: Proverbios, Eclesiastes y Cantares.",
      "The biblical tradition attributes the authorship of the books written by King Solomon to include Proverbs, Ecclesiastes, and Song of Solomon.",
      "El libro de Proverbios es un libro sapiencial del Antiguo Testamento que contiene proverbios, aforismos morales e instrucciones practicas de sabiduria para la vida.",
      "The Book of Proverbs is a wisdom book of the Old Testament containing proverbs, moral maxims, and practical teachings on wisdom."
    ],
    triples: [
      { s: "DAVID", p: "SON_OF", o: "JESSE" },
      { s: "SOLOMON", p: "SON_OF", o: "DAVID" },
      { s: "REHOBOAM", p: "SON_OF", o: "SOLOMON" },
      { s: "ABIJAH", p: "SON_OF", o: "REHOBOAM" },
      { s: "ISAAC", p: "SON_OF", o: "ABRAHAM" },
      { s: "JACOB", p: "SON_OF", o: "ISAAC" },
      { s: "JUDAH", p: "SON_OF", o: "JACOB" },
      { s: "OBED", p: "SON_OF", o: "BOAZ" },
      { s: "JESSE", p: "SON_OF", o: "OBED" },
      { s: "DAVID", p: "KING_OF", o: "ISRAEL" },
      { s: "SAUL", p: "FIRST_KING_OF", o: "ISRAEL" },
      { s: "JONAH", p: "FLED_TO", o: "TARSHISH" },
      { s: "BIBLIA", p: "CONTIENE", o: "66_LIBROS" },
      { s: "ANTIGUO_TESTAMENTO", p: "CONTIENE", o: "39_LIBROS" },
      { s: "NUEVO_TESTAMENTO", p: "CONTIENE", o: "27_LIBROS" },
      { s: "BIBLE", p: "CONTAINS", o: "66_BOOKS" },
      { s: "OLD_TESTAMENT", p: "CONTAINS", o: "39_BOOKS" },
      { s: "NEW_TESTAMENT", p: "CONTAINS", o: "27_BOOKS" },
      { s: "LIBROS_DE_LA_BIBLIA", p: "TOTAL", o: "66_LIBROS" },
      { s: "SALOMON", p: "ESCRIBIO", o: "PROVERBIOS" },
      { s: "SALOMON", p: "ESCRIBIO", o: "ECLESIASTES" },
      { s: "SALOMON", p: "ESCRIBIO", o: "CANTARES" },
      { s: "SOLOMON", p: "WROTE", o: "PROVERBS" },
      { s: "PROVERBIOS", p: "TRATA_DE", o: "SABIDURIA_Y_MORAL" }
    ]
  },
  quantum: {
    name: "Quantum Computing & Information Theory",
    category: "Modern Physics & Science",
    description: "Foundational principles of quantum states, superposition, and quantum entanglement.",
    sentences: [
      "A qubit is the basic unit of quantum information, capable of existing in a superposition of zero and one.",
      "Quantum entanglement is a physical phenomenon where entangled particles remain interconnected regardless of distance.",
      "Quantum teleportation transmits quantum states between separated locations using entanglement and classical communication.",
      "Shor's algorithm is a quantum polynomial-time integer factorization algorithm.",
      "Grover's algorithm provides a quadratic speedup for unstructured database searching on a quantum computer.",
      "Decoherence is the loss of quantum coherence caused by interaction with the surrounding environment.",
      "Superconducting circuits and trapped ions are leading physical architectures for quantum processors."
    ],
    triples: [
      { s: "QUBIT", p: "UNIT_OF", o: "QUANTUM_INFORMATION" },
      { s: "QUBIT", p: "EXHIBITS", o: "SUPERPOSITION" },
      { s: "ENTANGLEMENT", p: "ENABLES", o: "QUANTUM_TELEPORTATION" },
      { s: "SHOR_ALGORITHM", p: "SOLVES", o: "INTEGER_FACTORIZATION" },
      { s: "GROVER_ALGORITHM", p: "SPEEDUP_FOR", o: "UNSTRUCTURED_SEARCH" },
      { s: "DECOHERENCE", p: "CAUSES", o: "COHERENCE_LOSS" }
    ]
  },
  code: {
    name: "SWE-bench Lite & Code Knowledge Graph",
    category: "Autonomous Software Engineering",
    description: "Polyglot AST symbols, Blast Radius, Abductive Bug Diagnosis, STRIPS Planning & Atomic Verification.",
    sentences: [
      "The Code Knowledge Graph extracts polyglot AST symbols, class hierarchies, and call graphs from C, Python, and JavaScript without external runtimes.",
      "Django validators use ASCIIUsernameValidator with regex pattern to validate system usernames strictly.",
      "Flask Blueprint registers application routes, url prefixes, and view endpoints into the central WSGI dispatch table.",
      "The STRIPS Task Planner synthesizes software repair sequences over bitmask propositional states.",
      "The Blast Radius engine computes the transitive impact closure of function edits across polyglot project files via breadth-first search.",
      "Pre-flight patch verification checks AST syntax and unified diff context before applying changes atomically with rollback.",
      "The abductive compiler diagnostic engine maps GCC, Clang, and MSVC error messages directly to missing symbols in the Code Knowledge Graph.",
      "The SWE-bench harness verifies candidate patches against golden instances; it does not claim to resolve SWE-bench issues on its own.",
      "El Grafo de Conocimiento de Codigo extrae simbolos AST, jerarquias de clases y grafos de llamadas en C, Python y JavaScript.",
      "El planificador STRIPS genera secuencias de reparacion de codigo sobre estados binarios de bits.",
      "El motor de Blast Radius calcula el impacto transitivo de una modificacion de funcion en todo el proyecto mediante busqueda en anchura.",
      "La validacion de nombres de usuario en Django utiliza ASCIIUsernameValidator para restringir los caracteres a formato ASCII.",
      "symbols-agent is the standalone native CLI for blast radius inspection, abductive diagnosis, and autonomous bug repair."
    ],
    triples: [
      { s: "CODE_GRAPH", p: "EXTRACTS", o: "POLYGLOT_AST" },
      { s: "DJANGO_VALIDATOR", p: "VALIDATES", o: "ASCII_USERNAME" },
      { s: "ASCII_USERNAME_VALIDATOR", p: "DEFINED_IN", o: "DJANGO_VALIDATORS" },
      { s: "FLASK_BLUEPRINT", p: "REGISTERS", o: "URL_ROUTES" },
      { s: "STRIPS_PLANNER", p: "SYNTHESIZES", o: "REPAIR_ACTIONS" },
      { s: "BLAST_RADIUS", p: "COMPUTES", o: "TRANSITIVE_IMPACT" },
      { s: "PREFLIGHT_VERIFY", p: "CHECKS", o: "AST_SYNTAX" },
      { s: "ABDUCTIVE_DIAGNOSE", p: "RESOLVES", o: "COMPILER_ERRORS" },
      { s: "SWE_BENCH_LITE", p: "EVALUATES", o: "AUTONOMOUS_AGENTS" },
      { s: "SYMBOLS_AGENT", p: "CLI_FOR", o: "AUTONOMOUS_ENGINEERING" }
    ]
  }
};

const DEFAULT_DICTIONARY_TEXT = `# Declarative Bidirectional English <-> Spanish Alignment Table
sun = sol
moon = luna
sky = cielo
light = luz
stars = estrellas
psychic energy = energia psiquica
energy = energia
psychic = psiquica
psychic = psiquico
unconscious = inconsciente
collective unconscious = inconsciente colectivo
collective = colectivo
collective = colectiva
archetype = arquetipo
archetypes = arquetipos
maternal archetype = arquetipo materno
maternal = materno
maternal = materna
paternal = paterno
paternal = paterna
symbol = simbolo
symbols = simbolos
mother = madre
father = padre
libido = libido
dream = sueno
dreams = suenos
mythology = mitologia
transformation = transformacion
heroic = heroico
journey = viaje
bible = biblia
book = libro
books = libros
old testament = antiguo testamento
new testament = nuevo testamento
testament = testamento
old = antiguo
new = nuevo
proverbs = proverbios
ecclesiastes = eclesiastes
song of solomon = cantares
king = rey
first = primer
first = primero
prophet = profeta
anointed = ungido
begat = engendro
sea = mar
wind = viento
tempest = tempestad
wisdom = sabiduria
moral = moral
father = padre
son = hijo
daughter = hija
brother = hermano
sister = hermana
first king of israel = primer rey de israel
solomon = salomon
jonah = jonas
david = david
jesse = isai
saul = saul
samuel = samuel
abraham = abraham
isaac = isaac
jacob = jacob
judah = juda
boaz = booz
obed = obed
ruth = rut
quantum = cuantico
quantum = cuantica
quantum information = informacion cuantica
qubit = qubit
superposition = superposicion
entanglement = entrelazamiento
quantum entanglement = entrelazamiento cuantico
teleportation = teletransportacion
quantum teleportation = teletransportacion cuantica
decoherence = decoherencia
algorithm = algoritmo
shor algorithm = algoritmo de shor
shor = shor_algorithm
grover algorithm = algoritmo de grover
grover = grover_algorithm
database = base de datos
factorization = factorizacion
integer = entero
speedup = aceleracion
represents = representa
defined as = definido como
wrote = escribio
authored = autor
ruled = reino
anointed = ungio
fled = huyo
manifests = manifiesta
contains = contiene
causes = causa
enables = permite
solves = resuelve
exhibits = exhibe
code = codigo
codigo = code
grafo = graph
grafo de codigo = code_graph
code graph = code_graph
radio de impacto = blast_radius
blast radius = blast_radius
planificador strips = strips_planner
planificador = strips_planner
strips planner = strips_planner
validador = validator
validator = validador
validacion = validation
validation = validacion
ast = ast
reparacion = repair
repair = reparacion
`;

if (typeof module !== "undefined" && module.exports) {
  module.exports = { PRESET_CORPORA, DEFAULT_DICTIONARY_TEXT };
}
