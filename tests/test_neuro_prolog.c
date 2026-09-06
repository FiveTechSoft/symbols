#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "symbol.h"
#include "relation.h"
#include "graph.h"
#include "embedding.h"
#include "neuro_prolog.h"

static int CHECKS = 0;
static int FAILS = 0;

static void Check(int cond, const char *msg)
{
    CHECKS++;
    if (!cond)
    {
        FAILS++;
        printf("FAIL (#%d): %s\n", CHECKS, msg);
        return;
    }
    printf("ok   (#%d): %s\n", CHECKS, msg);
}

static SYMBOL_ID Sy(GRAPH *g, const char *name)
{
    return SymbolFind(g->symbols, name);
}

static NP_QUERY QN(SYMBOL_ID s, SYMBOL_ID p, SYMBOL_ID o)
{
    NP_QUERY q;
    q.subject   = NPConst(s);
    q.predicate = p; /* 0 = predicado libre */
    q.object    = NPConst(o);
    return q;
}

int main(void)
{
    printf("========================================\n");
    printf("  NEURO-PROLOG - BENCHMARK P4a         \n");
    printf("========================================\n\n");

    /* ---------------------------------------------------------- */
    /* Grafico principal: encadenado y composicion de identidad  */
    /* ---------------------------------------------------------- */
    GRAPH *g = GraphCreate(64, 64);
    Check(g != NULL, "GraphCreate(64,64)");

    GraphAddRelation(g, GraphAddSymbol(g, "MIMI"),
                     GraphAddSymbol(g, "ES"),
                     GraphAddSymbol(g, "GATO"));
    GraphAddRelation(g, Sy(g, "GATO"),
                     GraphAddSymbol(g, "COME"),
                     GraphAddSymbol(g, "RATON"));
    GraphAddRelation(g, Sy(g, "GATO"), Sy(g, "ES"),
                     GraphAddSymbol(g, "ANIMAL"));
    GraphAddRelation(g, GraphAddSymbol(g, "ALFA"), Sy(g, "ES"),
                     GraphAddSymbol(g, "BETA"));
    GraphAddRelation(g, Sy(g, "BETA"), Sy(g, "ES"),
                     GraphAddSymbol(g, "GAMA"));
    GraphAddRelation(g, Sy(g, "GAMA"), GraphAddSymbol(g, "VIVE"),
                     GraphAddSymbol(g, "CASA"));
    GraphAddRelation(g, Sy(g, "GAMA"), Sy(g, "VIVE"),
                     GraphAddSymbol(g, "BARRIO"));
    GraphAddRelation(g, Sy(g, "RATON"), Sy(g, "ES"),
                     GraphAddSymbol(g, "RATON_AZUL"));

    SYMBOL_ID s_mimi  = Sy(g, "MIMI");
    SYMBOL_ID s_gato  = Sy(g, "GATO");
    SYMBOL_ID s_raton = Sy(g, "RATON");
    SYMBOL_ID s_es    = Sy(g, "ES");
    SYMBOL_ID s_come  = Sy(g, "COME");
    SYMBOL_ID s_vive  = Sy(g, "VIVE");
    SYMBOL_ID s_alfa  = Sy(g, "ALFA");
    SYMBOL_ID s_ratz  = Sy(g, "RATON_AZUL");

    NP_SOLUTION sol[32];
    NP_OPTIONS  opt;

    /* 1. Hecho directo: GATO COME ?X -> RATON */
    opt = (NP_OPTIONS){ NP_MIN_CONF_DEF, 8, 32, 1, NP_FUZZY_GATE };
    NP_QUERY q = QN(s_gato, s_come, s_gato);
    q.object = NPVar(0);
    uint32_t n = NPProve(g, &q, &opt, sol, 32);
    Check(n == 1, "directo: GATO COME ?X = 1 solucion");
    if (n == 1)
    {
        Check(sol[0].object == s_raton, "directo: objeto RATON");
        Check(sol[0].depth == 0, "directo: profundidad 0");
        Check(sol[0].confidence >= 0.99f, "directo: confianza ~1.0");
    }
    else { Check(0, "sin solucion para afirmar detalles"); }

    /* 2. Enumeracion por sujeto libre: ? COME RATON -> GATO */
    q = QN(0, 0, 0);
    q.subject = NPVar(0); q.predicate = s_come;
    q.object = NPConst(s_raton);
    n = NPProve(g, &q, &opt, sol, 32);
    Check(n == 1 && sol[0].subject == s_gato, "enumerar: ? COME RATON -> GATO");

    /* 3. Predicado libre: GATO ?P ?X -> 2 soluciones */
    q = QN(0, 0, 0);
    q.subject = NPConst(s_gato); q.predicate = SYMBOL_INVALID;
    q.object = NPVar(0);
    n = NPProve(g, &q, &opt, sol, 32);
    Check(n == 2, "predicado libre: GATO ?P ?X = 2 soluciones");

    /* 4. Leibniz 1 salto: MIMI COME ?X -> RATON (MIMI ES GATO) */
    q = QN(0, 0, 0);
    q.subject = NPConst(s_mimi); q.predicate = s_come;
    q.object = NPVar(0);
    n = NPProve(g, &q, &opt, sol, 32);
    Check(n == 1 && sol[0].object == s_raton, "leibniz: MIMI COME ?X -> RATON");
    if (n == 1)
    {
        Check(sol[0].depth == 1, "leibniz: 1 salto");
        Check(sol[0].confidence > 0.89f && sol[0].confidence < 0.91f,
              "leibniz: confianza 0.9^1");
    }
    else { Check(0, "sin solucion leibniz (detalle)"); }

    /* 5. Composicion por objeto: GATO COME RATON_AZUL via RATON ES RATON_AZUL */
    q = QN(s_gato, s_come, s_ratz);
    n = NPProve(g, &q, &opt, sol, 32);
    Check(n == 1 && sol[0].depth == 1, "objeto: GATO COME RATON_AZUL (1 salto)");
    if (n == 1)
        Check(sol[0].confidence > 0.89f && sol[0].confidence < 0.91f,
              "objeto: confianza 0.9^1");

    /* 6. Cadena de 2 saltos: ALFA VIVE ?X -> CASA + BARRIO a 0.9^2 */
    q = QN(0, 0, 0);
    q.subject = NPConst(s_alfa); q.predicate = s_vive;
    q.object = NPVar(0);
    n = NPProve(g, &q, &opt, sol, 32);
    Check(n == 2, "cadenas 2: ALFA VIVE ?X = 2 soluciones");
    if (n == 2)
    {
        Check(sol[0].depth == 2 && sol[1].depth == 2, "cadenas 2: profundidad 2");
        Check(sol[0].confidence >= 0.80f && sol[0].confidence <= 0.82f,
              "cadenas 2: confianza 0.81");
    }

    /* 7. Ambos lados: MIMI COME RATON_AZUL a 0.9^2 */
    q = QN(s_mimi, s_come, s_ratz);
    n = NPProve(g, &q, &opt, sol, 32);
    Check(n == 1 && sol[0].depth == 2, "ambos lados: MIMI COME RATON_AZUL (2 saltos)");
    if (n == 1)
        Check(sol[0].confidence >= 0.80f && sol[0].confidence <= 0.82f,
              "ambos lados: confianza 0.81");

    /* 8. Puerta de confianza: min_conf 0.82 excluye 0.81 */
    NP_OPTIONS strict = opt;
    strict.min_conf = 0.82f;
    q = QN(s_alfa, s_vive, 0);
    q.object = NPVar(0);
    n = NPProve(g, &q, &strict, sol, 32);
    Check(n == 0, "min_conf 0.82: 0 soluciones (0.81 excluido)");
    NP_OPTIONS lax = opt;
    lax.min_conf = 0.80f;
    n = NPProve(g, &q, &lax, sol, 32);
    Check(n == 2, "min_conf 0.80: 2 soluciones");

    /* 9. Limitador de profundidad: max_depth 1 corta 2 saltos */
    NP_OPTIONS shallow = opt;
    shallow.max_depth = 1;
    n = NPProve(g, &q, &shallow, sol, 32);
    Check(n == 0, "max_depth 1: ALFA VIVE ?X = 0 (GAMA esta a 2)");

    /* 10. Variable de predicado: GATO ?P ?X -> 2 hechos */
    q = QN(0, 0, 0);
    q.subject = NPConst(s_gato); q.predicate = SYMBOL_INVALID;
    q.object = NPVar(0);
    n = NPProve(g, &q, &opt, sol, 32);
    Check(n == 2, "variable P: GATO ?P ?X = 2 soluciones");
    if (n == 2)
    {
        int has_come = 0, has_es = 0;
        for (uint32_t i = 0; i < n; i++)
        {
            if (sol[i].predicate == s_come) has_come = 1;
            if (sol[i].predicate == s_es) has_es = 1;
        }
        Check(has_come && has_es, "variable P: pred. COME y ES enlazados");
    }

    /* 11. Unidad de frame: enlace + identidad estricta */
    NP_FRAME fr;
    memset(&fr, 0, sizeof(fr));
    NP_TERM tv = NPVar(3);
    int ok = NPUnifyTerm(&tv, 42, &fr);
    SYMBOL_ID rv = NPResolve(&tv, &fr);
    int bad = NPUnifyTerm(&tv, 43, &fr);
    Check(ok && rv == 42 && bad == 0, "frame: enlazar/leer/rechazar");

    /* 12. Desconocido honesto: MIMI NADA ?X -> 0 */
    SYMBOL_ID s_nada = GraphAddSymbol(g, "NADA");
    q = QN(s_mimi, s_nada, s_mimi);
    q.object = NPVar(0);
    n = NPProve(g, &q, &opt, sol, 32);
    Check(n == 0, "honesto: MIMI NADA ?X = 0 soluciones");

    /* ---------------------------------------------------------------- */
    /* 13. Ciclo de identidad: A ES B, B ES A (termina, sin duplis)     */
    /* ---------------------------------------------------------------- */
    GRAPH *gc = GraphCreate(32, 32);
    SYMBOL_ID ca = GraphAddSymbol(gc, "A");
    SYMBOL_ID cb = GraphAddSymbol(gc, "B");
    SYMBOL_ID ce = GraphAddSymbol(gc, "ES");
    SYMBOL_ID ck = GraphAddSymbol(gc, "CONOCE");
    GraphAddRelation(gc, ca, ce, cb);
    GraphAddRelation(gc, cb, ce, ca);
    GraphAddRelation(gc, ca, ck, GraphAddSymbol(gc, "MAR"));
    GraphAddRelation(gc, cb, ck, GraphAddSymbol(gc, "LUNA"));
    q = QN(ca, ck, 0);
    q.object = NPVar(0);
    n = NPProve(gc, &q, &opt, sol, 32);
    Check(n == 2, "ciclo: A CONOCE ?X = 2 (MAR directo, LUNA a traves B)");
    if (n == 2)
    {
        int has_mar = 0, has_luna = 0;
        for (uint32_t i = 0; i < n; i++)
        {
            if (sol[i].depth == 0) has_mar = 1;
            if (sol[i].depth == 1) has_luna = 1;
        }
        Check(has_mar && has_luna, "ciclo: MAR d0 + LUNA d1");
    }
    GraphDestroy(gc);

    /* ---------------------------------------------------------------- */
    /* 14. Pingüino (limitacion documentada: sin NAF, P4b)             */
    /*      AVE VUELA CIELO + PENGUINO ES AVE => iniere que vuela.     */
    /* ---------------------------------------------------------------- */
    GRAPH *gp = GraphCreate(32, 32);
    SYMBOL_ID pv = GraphAddSymbol(gp, "PENGUINO");
    SYMBOL_ID a  = GraphAddSymbol(gp, "AVE");
    SYMBOL_ID e2 = GraphAddSymbol(gp, "ES");
    SYMBOL_ID v  = GraphAddSymbol(gp, "VUELA");
    GraphAddRelation(gp, pv, e2, a);
    GraphAddRelation(gp, a, v, GraphAddSymbol(gp, "CIELO"));
    q = QN(pv, v, 0);
    q.object = NPVar(0);
    n = NPProve(gp, &q, &opt, sol, 32);
    Check(n == 1 && sol[0].depth == 1, "pinguino: iniere VUELA (limitacion NAF anotada)");
    GraphDestroy(gp);

    /* ---------------------------------------------------------------- */
    /* 15. Puente fuzzy gamma: GATO SER ANIMAL, pregunta por ES        */
    /* ---------------------------------------------------------------- */
    GRAPH *gf = GraphCreate(32, 32);
    EMBEDDING_TABLE *emb = EmbeddingTableCreate(32);
    GraphSetEmbeddingTable(gf, emb);
    SYMBOL_ID fg = GraphAddSymbol(gf, "GATO");
    SYMBOL_ID fer = GraphAddSymbol(gf, "ES");
    SYMBOL_ID fser = GraphAddSymbol(gf, "SER");
    SYMBOL_ID fan = GraphAddSymbol(gf, "ANIMAL");
    GraphAddRelation(gf, fg, fser, fan);

    float v_es[32], v_ser[32];
    memset(v_es, 0, sizeof(v_es));
    memset(v_ser, 0, sizeof(v_ser));
    v_es[0] = 1.0f;
    v_ser[0] = 0.9f;
    v_ser[1] = 0.1f;
    EmbeddingSetVector(emb, fer, v_es);
    EmbeddingSetVector(emb, fser, v_ser);
    float gamma = EmbeddingCosineSimilarity(v_es, v_ser);
    printf("gamma(ES,SER) = %.4f\n", gamma);
    Check(gamma > 0.99f && gamma < 0.999f, "fuzzy: gamma(ES,SER) ~0.994");

    q = QN(fg, fer, 0);
    q.object = NPVar(0);
    n = NPProve(gf, &q, &opt, sol, 32);
    Check(n == 1, "fuzzy: GATO ES ?X resuelve por SER");
    if (n == 1)
    {
        Check(sol[0].fuzzy == 1, "fuzzy: marcada fuzzy=1");
        Check(sol[0].predicate == fser, "fuzzy: predicado usado SER");
        Check(sol[0].object == fan, "fuzzy: objeto ANIMAL");
        Check(sol[0].confidence > 0.95f, "fuzzy: confianza ~gamma");
    }

    NP_OPTIONS off = opt;
    off.allow_fuzzy = 0;
    q = QN(fg, fer, 0);
    q.object = NPVar(0);
    n = NPProve(gf, &q, &off, sol, 32);
    Check(n == 0, "fuzzy: desactivado -> 0");
    NP_OPTIONS tight = opt;
    tight.fuzzy_gate = 0.9999f;
    n = NPProve(gf, &q, &tight, sol, 32);
    Check(n == 0, "fuzzy: puerta 0.9999 -> 0");

    EmbeddingTableDestroy(emb);
    GraphDestroy(gf);

    /* ---------------------------------------------------------------- */
    /* 16. Rendimiento bruto (no bloqueo; solo medicion)               */
    /* ---------------------------------------------------------------- */
    const int ITER = 1000;
    q = QN(s_mimi, s_come, s_ratz);
    clock_t t0 = clock();
    for (int i = 0; i < ITER; i++)
        (void)NPProve(g, &q, &opt, sol, 32);
    clock_t t1 = clock();
    double us = (double)(t1 - t0) * 1e6 / (double)ITER / (double)CLOCKS_PER_SEC;
    printf("medicion: %d NPProve (2 saltos) en %.1f us/cada\n", ITER, us);
    Check(us < 1000.0, "rendimiento: < 1 ms por prueba");

    GraphDestroy(g);

    printf("\n========================================\n");
    printf("neuro-Prolog P4a: %d/%d problemas OK\n", CHECKS - FAILS, CHECKS);
    printf("========================================\n");
    return FAILS ? EXIT_FAILURE : EXIT_SUCCESS;
}
