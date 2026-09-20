/* =========================================================================
   test_model_generic_rel.c: Test Universal Generic Relation Ingestion
   Verifies that binary models (.bin) with arbitrary/custom relation names
   (contains, part_of, requires, custom domain tags) are fully ingested
   into the QA knowledge base (ChatFactCount > 0, 0 dropped facts),
   and answer questions correctly via the conversational QA engine.
   ========================================================================= */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "model.h"
#include "chat.h"
#include "server_proto.h"

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__); \
        exit(EXIT_FAILURE); \
    } else { \
        printf("PASS: %s\n", msg); \
    } \
} while (0)

int main(void)
{
    printf("==================================================\n");
    printf("  TEST MODEL GENERIC RELATION INGESTION & QA      \n");
    printf("==================================================\n\n");

    const char *bin_path = "test_custom_kits.bin";

    /* 1. Build a model with diverse custom/generic relations */
    MODEL *m = ModelCreate(64, 64);
    TEST_ASSERT(m != NULL, "ModelCreate succeeded");

    /* Add symbols */
    SYMBOL_ID s_kit_a   = SymbolAdd(m->graph->symbols, "kit_a");
    SYMBOL_ID s_sensor  = SymbolAdd(m->graph->symbols, "sensor_temp");
    SYMBOL_ID s_batt    = SymbolAdd(m->graph->symbols, "battery_9v");
    SYMBOL_ID s_sys     = SymbolAdd(m->graph->symbols, "telemetry_system");
    SYMBOL_ID s_kit_pro = SymbolAdd(m->graph->symbols, "kit_pro");
    SYMBOL_ID s_wifi    = SymbolAdd(m->graph->symbols, "modulo_wifi");
    SYMBOL_ID s_france  = SymbolAdd(m->graph->symbols, "francia");
    SYMBOL_ID s_paris   = SymbolAdd(m->graph->symbols, "paris");

    /* Add custom relations: contains, requires, part_of, CONTIENE, capital */
    SYMBOL_ID r_contains = SymbolAdd(m->graph->symbols, "contains");
    SYMBOL_ID r_requires = SymbolAdd(m->graph->symbols, "requires");
    SYMBOL_ID r_part_of  = SymbolAdd(m->graph->symbols, "part_of");
    SYMBOL_ID r_contiene = SymbolAdd(m->graph->symbols, "CONTIENE");
    SYMBOL_ID r_capital  = SymbolAdd(m->graph->symbols, "capital");

    RelationAdd(m->graph->relations, s_kit_a,   r_contains, s_sensor);
    RelationAdd(m->graph->relations, s_kit_a,   r_requires, s_batt);
    RelationAdd(m->graph->relations, s_sensor,  r_part_of,  s_sys);
    RelationAdd(m->graph->relations, s_kit_pro, r_contiene, s_wifi);
    RelationAdd(m->graph->relations, s_france,  r_capital,  s_paris);

    TEST_ASSERT(RelationCount(m->graph->relations) == 5, "5 relations in graph");

    /* Save model to disk */
    int saved = ModelSave(m, bin_path);
    TEST_ASSERT(saved == 1, "ModelSave succeeded");
    ModelDestroy(m);
    m = NULL;

    /* 2. Verify binary model detection */
    TEST_ASSERT(ChatIsBinaryModel(bin_path) == 1, "ChatIsBinaryModel returns 1 for .bin");

    /* 3. Ingest into CHAT session */
    CHAT ch;
    ChatInit(&ch, bin_path);

    uint32_t facts = ChatFactCount(&ch);
    printf("   Facts loaded into QA engine: %u / 5\n", facts);
    TEST_ASSERT(facts == 5, "All 5 custom relations learned into QA facts (0 dropped)");

    /* 4. Query QA engine on custom relations */
    char out[1024];

    /* Query 1: kit_a contains -> sensor_temp */
    memset(out, 0, sizeof(out));
    int ok = ChatHandleToBuf(&ch, "what contains kit_a", out, sizeof(out));
    TEST_ASSERT(ok == 1, "ChatHandleToBuf handled 'what contains kit_a'");
    printf("   Answer: %s\n", out);
    TEST_ASSERT(strstr(out, "Sensor_temp") != NULL, "Answer contains 'Sensor_temp'");
    TEST_ASSERT(!ServerIsUnknown(out), "Answer is not unknown");

    /* Query 2: kit_a requires -> battery_9v */
    memset(out, 0, sizeof(out));
    ok = ChatHandleToBuf(&ch, "what is the requires of kit_a", out, sizeof(out));
    TEST_ASSERT(ok == 1, "ChatHandleToBuf handled 'what is the requires of kit_a'");
    printf("   Answer: %s\n", out);
    TEST_ASSERT(strstr(out, "Battery_9v") != NULL, "Answer contains 'Battery_9v'");
    TEST_ASSERT(!ServerIsUnknown(out), "Answer is not unknown");

    /* Query 3: sensor_temp part_of -> telemetry_system */
    memset(out, 0, sizeof(out));
    ok = ChatHandleToBuf(&ch, "part of sensor_temp", out, sizeof(out));
    TEST_ASSERT(ok == 1, "ChatHandleToBuf handled 'part of sensor_temp'");
    printf("   Answer: %s\n", out);
    TEST_ASSERT(strstr(out, "Telemetry_system") != NULL, "Answer contains 'Telemetry_system'");
    TEST_ASSERT(!ServerIsUnknown(out), "Answer is not unknown");

    /* Query 4: kit_pro CONTIENE -> modulo_wifi */
    memset(out, 0, sizeof(out));
    ok = ChatHandleToBuf(&ch, "que contiene kit_pro", out, sizeof(out));
    TEST_ASSERT(ok == 1, "ChatHandleToBuf handled 'que contiene kit_pro'");
    printf("   Answer: %s\n", out);
    TEST_ASSERT(strstr(out, "Modulo_wifi") != NULL, "Answer contains 'Modulo_wifi'");
    TEST_ASSERT(!ServerIsUnknown(out), "Answer is not unknown");

    /* Query 5: capital of francia -> paris */
    memset(out, 0, sizeof(out));
    ok = ChatHandleToBuf(&ch, "el capital de francia es", out, sizeof(out));
    TEST_ASSERT(ok == 1, "ChatHandleToBuf handled 'el capital de francia es'");
    printf("   Answer: %s\n", out);
    TEST_ASSERT(strstr(out, "Paris") != NULL, "Answer contains 'Paris'");
    TEST_ASSERT(!ServerIsUnknown(out), "Answer is not unknown");

    /* 5. Server completions pipeline test */
    char mapped[1024];
    ServerMapContent(out, mapped, sizeof(mapped));
    TEST_ASSERT(strcmp(mapped, "I don't know.") != 0, "ServerMapContent does not return 'I don't know.'");

    ChatDestroy(&ch);
    remove(bin_path);

    printf("\nAll generic relation tests PASSED with 100%% factual fidelity!\n");
    return 0;
}
