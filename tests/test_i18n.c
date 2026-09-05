#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "graph.h"
#include "symbol.h"
#include "i18n.h"
#include "nlg.h"

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
    /* Default language is English */
    CHECK(LangGet() == LANG_EN);

    /* Code lookup (case-insensitive) */
    CHECK(LangFindByCode("EN") == LANG_EN);
    CHECK(LangFindByCode("es") == LANG_ES);
    CHECK(LangFindByCode("Fr") == LANG_FR);
    CHECK(LangFindByCode("XX") == LANG_COUNT);
    CHECK(LangFindByCode(NULL) == LANG_COUNT);
    CHECK(strcmp(LangShortName(LANG_ES), "ES") == 0);

    /* LangSet respects bounds */
    LangSet(LANG_FR);
    CHECK(LangGet() == LANG_FR);
    LangSet((LANG_ID)99);
    CHECK(LangGet() == LANG_FR);
    LangSet(LANG_EN);

    /* Variant counts */
    CHECK(LangVariantCount(I18N_OPENER) == 6);
    CHECK(LangVariantCount(I18N_AND) == 1);
    CHECK(LangVariantCount((I18N_KEY)999) == 0);

    /* Every key x language has a non-empty template; variants wrap */
    for (int k = 0; k < I18N_COUNT; k++)
    {
        int n = LangVariantCount((I18N_KEY)k);
        for (int l = 0; l < LANG_COUNT; l++)
        {
            for (int v = 0; v < n; v++)
            {
                const char *t = LangTemplate((I18N_KEY)k, (LANG_ID)l, v);
                CHECK(t != NULL && strlen(t) > 0);
                /* ASCII-only: hygiene gate rejects mojibake bytes */
                for (const char *p = t; *p; p++)
                    CHECK((unsigned char)*p < 128);
            }
            /* Invalid language id falls back to EN */
            CHECK(strcmp(LangTemplate((I18N_KEY)k, (LANG_ID)99, 0),
                         LangTemplate((I18N_KEY)k, LANG_EN, 0)) == 0);
        }
        if (n > 1)
            CHECK(strcmp(LangTemplate((I18N_KEY)k, LANG_EN, n + 1),
                         LangTemplate((I18N_KEY)k, LANG_EN, 1)) == 0);
    }

    /* Languages actually differ where it matters */
    CHECK(strcmp(LangTemplate(I18N_AND, LANG_EN, 0),
                 LangTemplate(I18N_AND, LANG_ES, 0)) != 0);
    CHECK(strcmp(LangTemplate(I18N_AND, LANG_ES, 0),
                 LangTemplate(I18N_AND, LANG_FR, 0)) != 0);

    /* NLG output is localized but keeps raw symbol names (eval gate:
       strstr(answer, expected) must survive localization). */
    GRAPH *graph = GraphCreate(16, 32);
    CHECK(graph != NULL);

    SYMBOL_ID francia = GraphAddSymbol(graph, "FRANCIA");
    SYMBOL_ID capital = GraphAddSymbol(graph, "CAPITAL");
    SYMBOL_ID paris   = GraphAddSymbol(graph, "PARIS");
    CHECK(francia != SYMBOL_INVALID && capital != SYMBOL_INVALID
          && paris != SYMBOL_INVALID);
    CHECK(GraphAddRelation(graph, francia, capital, paris) == 1);

    RELATION *res[4];
    uint32_t n = GraphQuerySubjectRelation(graph, francia, capital, res, 4);
    CHECK(n == 1);

    char out[NLG_MAX_RESPONSE];

    LangSet(LANG_EN);
    NLGGenerateDirect(graph, francia, capital, res, n, out, sizeof(out));
    CHECK(strstr(out, "PARIS") != NULL);
    CHECK(strstr(out, " and ") == NULL);

    LangSet(LANG_ES);
    NLGGenerateDirect(graph, francia, capital, res, n, out, sizeof(out));
    CHECK(strstr(out, "PARIS") != NULL);
    {
        int es_ok = 0;
        for (int v = 0; v < LangVariantCount(I18N_POSITIVE); v++)
        {
            const char *pref = LangTemplate(I18N_POSITIVE, LANG_ES, v);
            if (strncmp(out, pref, strlen(pref)) == 0)
            {
                es_ok = 1;
                break;
            }
        }
        CHECK(es_ok);
    }

    /* Multi-object join uses the language connector */
    SYMBOL_ID lisboa = GraphAddSymbol(graph, "LISBOA");
    CHECK(GraphAddRelation(graph, francia, capital, lisboa) == 1);
    uint32_t n2 = GraphQuerySubjectRelation(graph, francia, capital, res, 4);
    CHECK(n2 == 2);

    LangSet(LANG_EN);
    NLGGenerateDirect(graph, francia, capital, res, n2, out, sizeof(out));
    CHECK(strstr(out, " and ") != NULL);

    LangSet(LANG_FR);
    NLGGenerateDirect(graph, francia, capital, res, n2, out, sizeof(out));
    CHECK(strstr(out, " et ") != NULL);

    LangSet(LANG_EN);
    GraphDestroy(graph);

    if (fails == 0)
        printf("i18n tests: all OK (%d languages x %d keys)\n",
               LANG_COUNT, I18N_COUNT);
    else
        printf("i18n tests: %d FAILURES\n", fails);

    return fails == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
