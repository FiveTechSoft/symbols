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
    name: "King James Bible (Genealogies & Kings)",
    category: "Historical & Theological",
    description: "Chronological genealogies, ancestral lines, and royal succession records.",
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
      "Samuel anointed David to be king in the midst of his brethren."
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
      { s: "JONAH", p: "FLED_TO", o: "TARSHISH" }
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
  }
};
