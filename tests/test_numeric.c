#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "model.h"
#include "graph.h"
#include "numeric.h"
#include "ingest.h"
#include "parser.h"

/* M3a numeric sidecar gate: parse unit cases + fixture ingest with
   typed sidecars + QA value lookup (same ParserDetectQuestion/Answer
   path as test_eval_qa) + V5 persist round-trip on a scratch bin.
   The tracked golden is never touched. Threshold 90%. */

#ifndef NUMERIC_QA_THRESHOLD
#define NUMERIC_QA_THRESHOLD 90
#endif

static int failures = 0;

static void Check(int condition, const char *msg)
{
    if (!condition)
    {
        printf("  FAIL: %s\n", msg);
        failures++;
    }
}

static void CheckDbl(double got, double want, const char *msg)
{
    if (fabs(got - want) > 1e-9)
    {
        printf("  FAIL: %s (got %f, want %f)\n", msg, got, want);
        failures++;
    }
}

static void TrimNL(char *s)
{
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r'))
        s[--n] = '\0';
}

int main(void)
{
    /* 1. Parse unit cases (documented Spanish-format heuristic). */
    printf("1. NumericParseMeasure cases:\n");
    {
        static const struct { const char *in; int ok; double v; const char *u; } cases[] = {
            {"505.990 KM²", 1, 505990.0, "KM²"},
            {"8,47",        1, 8.47,     ""},
            {"47.5",        1, 47.5,     ""},
            {"-3",          1, -3.0,     ""},
            {"1978",        1, 1978.0,   ""},
            {"1.234,56",    1, 1234.56,  ""},
            {"3 MILLONES",  1, 3.0,      "MILLONES"},
            {"667 M",       1, 667.0,    "M"},
            {"1.234",       1, 1234.0,   ""},
            {"12/10/1492",  0, 0.0,      ""},
            {"today",       0, 0.0,      ""},
            {"KM²",        0, 0.0,      ""},
            {"",            0, 0.0,      ""},
            {NULL, 0, 0.0, NULL}
        };
        int i;
        for (i = 0; cases[i].in != NULL; i++)
        {
            double v = 0.0;
            char u[NUMERIC_UNIT_MAX] = {0};
            int ok = NumericParseMeasure(cases[i].in, &v, u, sizeof(u));
            char msg[128];
            snprintf(msg, sizeof(msg), "parse '%s' ok", cases[i].in);
            Check(ok == cases[i].ok, msg);
            if (ok && cases[i].ok)
            {
                snprintf(msg, sizeof(msg), "parse '%s' value", cases[i].in);
                CheckDbl(v, cases[i].v, msg);
                snprintf(msg, sizeof(msg), "parse '%s' unit", cases[i].in);
                Check(strcmp(u, cases[i].u) == 0, msg);
            }
        }
        printf("   %d parse cases done\n", i);
    }

    /* 2. Fixture ingest: triples in, typed sidecars attached. */
    printf("2. Fixture ingest (tests/qa_numeric_fixture.tsv):\n");
    MODEL *m = ModelCreate(256, 256);
    Check(m != NULL && m->graph != NULL, "ModelCreate");
    {
        FILE *f = fopen("tests/qa_numeric_fixture.tsv", "r");
        char line[512];
        uint32_t nrows = 0;
        Check(f != NULL, "open fixture");
        if (f != NULL)
        {
            while (fgets(line, sizeof(line), f))
            {
                char *tab1, *tab2;
                TrimNL(line);
                if (line[0] == '\0' || line[0] == '#')
                    continue;
                tab1 = strchr(line, '\t');
                if (!tab1)
                    continue;
                *tab1 = '\0';
                tab2 = strchr(tab1 + 1, '\t');
                if (!tab2)
                    continue;
                *tab2 = '\0';
                Check(IngestTriple(m->graph, line, tab1 + 1, tab2 + 1) != 0,
                      "ingest row");
                nrows++;
            }
            fclose(f);
        }
        printf("   rows=%u relations=%u\n", nrows,
               RelationCount(m->graph->relations));
        Check(nrows == 20, "20 fixture rows");
        Check(RelationCount(m->graph->relations) == 20, "20 relations");
    }

    /* 3. Sidecar spot checks on stored symbols. */
    printf("3. Sidecar spot checks:\n");
    {
        static const struct { const char *sym; double v; const char *u; } spots[] = {
            {"505.990 KM²", 505990.0, "KM²"},
            {"3.479 M",     3479.0,   "M"},
            {"1.007 KM",    1007.0,   "KM"},
            {"48 MILLONES", 48.0,     "MILLONES"},
            {"1978",        1978.0,   ""},
            {"667 M",       667.0,    "M"},
            {NULL, 0.0, NULL}
        };
        int i;
        for (i = 0; spots[i].sym != NULL; i++)
        {
            SYMBOL_ID id = SymbolFind(m->graph->symbols, spots[i].sym);
            double v = 0.0;
            char u[NUMERIC_UNIT_MAX] = {0};
            char msg[160];
            snprintf(msg, sizeof(msg), "symbol '%s' present", spots[i].sym);
            Check(id != SYMBOL_INVALID, msg);
            snprintf(msg, sizeof(msg), "sidecar '%s' present", spots[i].sym);
            Check(NumericGet(m->graph->numerics, id, &v, u, sizeof(u)), msg);
            snprintf(msg, sizeof(msg), "sidecar '%s' value", spots[i].sym);
            CheckDbl(v, spots[i].v, msg);
            snprintf(msg, sizeof(msg), "sidecar '%s' unit", spots[i].sym);
            Check(strcmp(u, spots[i].u) == 0, msg);
        }
        /* Plain words carry no measure. */
        {
            SYMBOL_ID id = SymbolFind(m->graph->symbols, "ESPANA");
            Check(id != SYMBOL_INVALID &&
                  !NumericGet(m->graph->numerics, id, NULL, NULL, 0),
                  "ESPANA has no sidecar");
        }
    }

    /* 4. QA value lookup over the set (same path as test_eval_qa). */
    printf("4. QA lookup (tests/qa_eval_numeric.tsv):\n");
    {
        FILE *f = fopen("tests/qa_eval_numeric.tsv", "r");
        char line[1024];
        uint32_t total = 0, pass = 0;
        Check(f != NULL, "open eval set");
        if (f != NULL)
        {
            while (fgets(line, sizeof(line), f))
            {
                char *tab;
                QUESTION q;
                char answer[256] = {0};
                int found;
                TrimNL(line);
                if (line[0] == '\0' || line[0] == '#')
                    continue;
                tab = strchr(line, '\t');
                if (!tab || tab[1] == '\0')
                    continue;
                *tab = '\0';
                total++;
                q = ParserDetectQuestion(m->graph, line);
                found = (q.valid && q.is_question) ?
                    ParserAnswerQuestion(m->graph, &q, answer,
                                         sizeof(answer)) : 0;
                if (found && strstr(answer, tab + 1) != NULL)
                    pass++;
                else
                    printf("  MISS #%u: '%s' -> '%s' (esperaba '%s')\n",
                           total, line, found ? answer : "<sin respuesta>",
                           tab + 1);
            }
            fclose(f);
        }
        printf("   NUMERIC eval: %u/%u\n", pass, total);
        Check(total == 20, "20 eval rows");
        Check(total > 0 && (100 * pass / total) >= NUMERIC_QA_THRESHOLD,
              "numeric threshold 90");
    }

    /* 5. V5 persist round-trip on a scratch bin (ignored by git). */
    printf("5. V5 round-trip (numeric_test.bin):\n");
    Check(ModelSave(m, "numeric_test.bin") != 0, "ModelSave scratch");
    ModelDestroy(m);
    m = NULL;
    {
        MODEL *r = ModelLoad("numeric_test.bin");
        SYMBOL_ID id;
        double v = 0.0;
        char u[NUMERIC_UNIT_MAX] = {0};
        QUESTION q;
        char answer[256] = {0};
        int found;
        Check(r != NULL && r->graph != NULL, "ModelLoad scratch");
        if (r != NULL)
        {
            id = SymbolFind(r->graph->symbols, "505.990 KM²");
            Check(id != SYMBOL_INVALID &&
                  NumericGet(r->graph->numerics, id, &v, u, sizeof(u)) &&
                  fabs(v - 505990.0) < 1e-9 && strcmp(u, "KM²") == 0,
                  "round-trip sidecar");
            q = ParserDetectQuestion(r->graph, "la superficie de ESPANA es");
            found = (q.valid && q.is_question) ?
                ParserAnswerQuestion(r->graph, &q, answer,
                                     sizeof(answer)) : 0;
            Check(found && strstr(answer, "505.990") != NULL,
                  "round-trip QA");
            ModelDestroy(r);
        }
        remove("numeric_test.bin");
    }

    if (failures == 0)
        printf("NUMERIC gate: ALL GREEN\n");
    else
        printf("NUMERIC gate: %d FAILURES\n", failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
