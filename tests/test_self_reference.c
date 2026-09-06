#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "graph.h"
#include "parser.h"
#include "context.h"
#include "dialog.h"
#include "i18n.h"

static int fails = 0;

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            printf("FAIL: %s (line %d)\n", #cond, __LINE__); \
            fails++; \
        } \
    } while (0)

int main(void)
{
    GRAPH *graph = GraphCreate(64, 64);
    CHECK(graph != NULL);
    /* Same seeding the REPL does at startup. */
    SYMBOL_ID yo  = GraphAddSymbol(graph, "YO");
    SYMBOL_ID es  = GraphAddSymbol(graph, "ES");
    SYMBOL_ID ms  = GraphAddSymbol(graph, "MODELO_SIMBOLICO");
    SYMBOL_ID lc  = GraphAddSymbol(graph, "LLM_DE_CONVERSACION");
    CHECK(GraphAddRelation(graph, yo, es, ms) == 1);
    CHECK(GraphAddRelation(graph, yo, es, lc) == 1);

    /* 1) Conjugated copula + bare interrogative land on YO --ES--> ?,
       with or without punctuation. */
    QUESTION q = ParserDetectQuestion(graph, "¿quien eres?");
    CHECK(q.valid == 1);
    CHECK(strcmp(q.subject, "YO") == 0);
    CHECK(strcmp(q.relation, "ES") == 0);

    q = ParserDetectQuestion(graph, "quien soy yo");
    CHECK(q.valid == 1);
    CHECK(strcmp(q.subject, "YO") == 0);
    CHECK(strcmp(q.relation, "ES") == 0);

    char ans[256] = {0};
    CHECK(ParserAnswerQuestion(graph, &q, ans, sizeof(ans)) == 1);
    CHECK(strstr(ans, "MODELO_SIMBOLICO") != NULL);
    CHECK(strstr(ans, "LLM_DE_CONVERSACION") != NULL);

    /* 2) NLG self copula BEFORE any statement ingests a learned mold:
       identity statement with the localized "Soy/I am" prefix. */
    {
        char out[512];
        LangSet(LANG_ES);
        CHECK(DialogGenerateResponse(graph, NULL, "¿quien eres?",
                                     out, sizeof(out)) == 1);
        CHECK(strstr(out, "Soy ") != NULL);
        CHECK(strstr(out, "MODELO_SIMBOLICO") != NULL);

        LangSet(LANG_EN);
        CHECK(DialogGenerateResponse(graph, NULL, "¿quien eres?",
                                     out, sizeof(out)) == 1);
        CHECK(strstr(out, "I am ") != NULL);
        LangSet(LANG_EN);
    }

    /* 3) Questions never store: markers or '?' keep the line out of
       the graph (no QUIEN--SOY-->YO contamination). */
    CONTEXT *ctx = ContextCreate();
    uint32_t rels = RelationCount(graph->relations);
    uint32_t syms = SymbolCount(graph->symbols);
    CHECK(ParserIngestSentenceCtx(graph, ctx, "quien soy yo") == 0);
    CHECK(ParserIngestSentenceCtx(graph, ctx, "¿quien eres?") == 0);
    CHECK(ParserIngestSentenceCtx(graph, ctx, "tu eres un robot") == 0);
    CHECK(ParserIngestSentenceCtx(graph, ctx, "¿que hora es?") == 0);
    CHECK(ParserIngestSentenceCtx(graph, ctx, "who are you") == 0);
    CHECK(RelationCount(graph->relations) == rels);
    CHECK(SymbolCount(graph->symbols) == syms);
    CHECK(SymbolFind(graph->symbols, "QUIEN") == SYMBOL_INVALID);
    CHECK(SymbolFind(graph->symbols, "SOY") == SYMBOL_INVALID);
    CHECK(SymbolFind(graph->symbols, "ERES") == SYMBOL_INVALID);

    /* 4) A copula statement still learns: SOY is out of the marker set
       by design and roots YO --ES--> ? through the lemma table. */
    rels = RelationCount(graph->relations);
    CHECK(ParserIngestSentenceCtx(graph, ctx, "yo soy dron") == 1);
    CHECK(RelationCount(graph->relations) == rels + 1);
    CHECK(GraphFindRelation(graph, yo, es,
                            GraphAddSymbol(graph, "DRON")) != NULL);

    /* 5) Bare QUIEN remaps to self only when it IS the subject:
       "¿quien es el gato?" asks about GATO, never about YO. */
    SYMBOL_ID gato   = GraphAddSymbol(graph, "GATO");
    SYMBOL_ID animal = GraphAddSymbol(graph, "ANIMAL");
    CHECK(GraphAddRelation(graph, gato, es, animal) == 1);
    q = ParserDetectQuestion(graph, "¿quien es el gato?");
    CHECK(q.valid == 1);
    CHECK(strcmp(q.subject, "GATO") == 0);
    CHECK(ParserAnswerQuestion(graph, &q, ans, sizeof(ans)) == 1);
    CHECK(strstr(ans, "ANIMAL") != NULL);

    LangSet(LANG_EN);
    ContextDestroy(ctx);
    GraphDestroy(graph);

    if (fails == 0)
        printf("self-reference tests: all OK\n");
    else
        printf("self-reference tests: %d FAILURES\n", fails);
    return fails == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
