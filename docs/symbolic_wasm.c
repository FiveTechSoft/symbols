/*
 * symbolic_wasm.c - Emscripten WASM wrapper for Symbolic LLM engine
 * Exposes core C11 API to JavaScript via Emscripten FFI.
 * NO modifications to the engine source — only linking.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "chat.h"
#include "graph.h"
#include "symbol.h"
#include "relation.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#define EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define EXPORT
#endif

/* Static CHAT instance (single-threaded browser context) */
static CHAT g_chat;
static int  g_initialized = 0;

/* Output buffer for responses */
#define WASM_OUT_BUF_SIZE 4096
static char g_out_buf[WASM_OUT_BUF_SIZE];

/* ---- Lifecycle ---- */
EXPORT int wasm_init(const char *corpus_path) {
    memset(&g_chat, 0, sizeof(g_chat));
    ChatInit(&g_chat, corpus_path);
    g_initialized = 1;
    return (int)ChatFactCount(&g_chat);
}

EXPORT int wasm_load_corpus(const char *path) {
    if (!g_initialized) return -1;
    return (int)ChatLoadCorpus(&g_chat, path);
}

EXPORT int wasm_load_model(const char *path) {
    if (!g_initialized) return -1;
    return (int)ChatLoadModel(&g_chat, path);
}

EXPORT int wasm_fact_count(void) {
    if (!g_initialized) return 0;
    return (int)ChatFactCount(&g_chat);
}

/* ---- Stats for UI ---- */
EXPORT int wasm_symbol_count(void) {
    if (!g_initialized || !g_chat.tgraph || !g_chat.tgraph->symbols)
        return 0;
    return (int)g_chat.tgraph->symbols->count;
}

EXPORT int wasm_relation_count(void) {
    if (!g_initialized || !g_chat.tgraph || !g_chat.tgraph->relations)
        return 0;
    return (int)g_chat.tgraph->relations->count;
}

EXPORT int wasm_turn_count(void) {
    if (!g_initialized) return 0;
    return (int)g_chat.episodic.count;
}

EXPORT int wasm_sentence_count(void) {
    /* tlex[0] holds the loaded text sentences; count via text_lex */
    if (!g_initialized) return 0;
    uint32_t total = 0;
    for (uint32_t i = 0; i < g_chat.ntfiles && i < CHAT_TEXT_FILES_MAX; i++) {
        total += g_chat.tlex[i].nsent;
    }
    return (int)total;
}

/* ---- Query ---- */
EXPORT const char *wasm_query(const char *query) {
    if (!g_initialized || !query) {
        snprintf(g_out_buf, WASM_OUT_BUF_SIZE, "Engine not initialized");
        return g_out_buf;
    }
    memset(g_out_buf, 0, WASM_OUT_BUF_SIZE);
    int handled = ChatHandleToBuf(&g_chat, query, g_out_buf, WASM_OUT_BUF_SIZE);
    if (!handled || g_out_buf[0] == '\0') {
        snprintf(g_out_buf, WASM_OUT_BUF_SIZE,
            "{\"response\":\"No verified ground truth.\",\"status\":\"UNKNOWN_FAIL_CLOSED\"}");
    }
    return g_out_buf;
}

/* ---- Episodic Memory ---- */
EXPORT int wasm_learn(const char *subject, const char *relation, const char *object) {
    if (!g_initialized) return -1;
    return ChatLearnTriple(&g_chat, subject, relation, object, "wasm_user");
}

EXPORT int wasm_episodic_count(void) {
    if (!g_initialized) return 0;
    return (int)ChatEpisodicCount(&g_chat);
}

EXPORT void wasm_reset(void) {
    if (g_initialized) {
        ChatDestroy(&g_chat);
        memset(&g_chat, 0, sizeof(g_chat));
        g_initialized = 0;
    }
}

/* ---- Persona ---- */
EXPORT void wasm_set_persona(int id) {
    if (g_initialized) ChatSetPersona(&g_chat, (PERSONA_ID)id);
}

EXPORT int wasm_get_persona(void) {
    if (!g_initialized) return 0;
    return (int)ChatGetPersona(&g_chat);
}
