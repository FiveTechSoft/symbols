/* ============================================================
   passage_nlg: Document-Level Passage Generation & Soft Intent Mapping.
   Pure C11, zero tensors, zero backprop, fail-closed truth preservation.

   Key capabilities:
     1. SOFT INTENT MAPPING:
        - Elastic parsing of open natural phrasing ("hablame de X",
          "cuentame acerca de X", "tell me about X", "de que trata X").
     2. CROSS-LINGUAL ENTITY LINKING:
        - Binds Spanish query terms to English corpus symbols via DICT
          (e.g. "salomon" -> "solomon", "proverbios" -> "proverbs").
     3. SUB-GRAPH STORYTELLING (Passage Generator):
        - Multi-paragraph structured essay generation:
          [Executive framing] -> [Relational core] -> [Active memory context] -> [Socratic closure].
     4. CADENCE STREAMING:
        - Word-by-word streaming simulation reproducing the feel of an LLM.

   Design principles:
     - HARDCODING=0: Discourse frames and lexicons are declarative.
     - Zero hallucinations: Every claim is mathematically bound to graph nodes.
   ============================================================ */

#ifndef PASSAGE_NLG_H
#define PASSAGE_NLG_H

#include <stdint.h>
#include <stddef.h>
#include "graph.h"
#include "graph_reasoning.h"
#include "cognitive_learning.h"
#include "stochastic_nlg.h"
#include "metacognition.h"
#include "dict.h"

#define PASSAGE_BUF_MAX   4096
#define PASSAGE_MAX_FACTS 16

/* Conversational intent classification */
typedef enum
{
    INTENT_UNKNOWN = 0,
    INTENT_SUMMARIZE_ENTITY, /* "hablame de X", "cuentame de X", "tell me about X" */
    INTENT_RELATION_QUERY,   /* "que relacion hay entre X e Y" */
    INTENT_WHY_QUERY,        /* "por que X es Y" */
    INTENT_VERIFY_QUERY,     /* "es X padre de Y" */
    INTENT_EXPLORE_TOPIC,    /* "que temas se relacionan con X" */
    INTENT_METACOGNITION     /* "como sabes eso", "por que crees eso" */
} PASSAGE_INTENT;

/* Structured query representation extracted from natural language */
typedef struct
{
    PASSAGE_INTENT intent;
    char           raw_query[256];
    char           entity1[64];
    char           entity2[64];
    SYMBOL_ID      sym1;
    SYMBOL_ID      sym2;
} PARSED_QUERY_INTENT;

/* Classify open conversational query and resolve entities via cross-lingual bridge */
PARSED_QUERY_INTENT PassageClassifyQuery(
    const GRAPH *graph,
    const DICT *dict,
    const char *user_input);

/* Generate full multi-paragraph passage for an entity using sub-graph storytelling */
uint32_t PassageGenerateTopicStory(
    const GRAPH *graph,
    const COGNITIVE_LEARNER *learner,
    WORKING_MEMORY *wm,
    SYMBOL_ID entity,
    STOCHASTIC_NLG_CONFIG *cfg,
    char *out,
    size_t out_size);

/* Master conversational dispatcher that answers any open user query like an LLM */
uint32_t PassageHandleTurn(
    const GRAPH *graph,
    const COGNITIVE_LEARNER *learner,
    WORKING_MEMORY *wm,
    const PROVENANCE_TABLE *pt,
    const DICT *dict,
    const char *user_input,
    STOCHASTIC_NLG_CONFIG *cfg,
    STOCHASTIC_DISCOURSE_HISTORY *hist,
    char *out,
    size_t out_size);

/* Stream-print a generated passage token-by-token (LLM typing cadence) */
void PassageStreamOutput(const char *text, uint32_t ms_per_word);

#endif /* PASSAGE_NLG_H */
