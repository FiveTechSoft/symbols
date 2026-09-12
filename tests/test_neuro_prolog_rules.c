#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "symbol.h"
#include "relation.h"
#include "graph.h"
#include "neuro_prolog.h"
#include "neuro_rules.h"

/* P4b Horn layer gate: declared rules, shallow cut, NAF, denial veto.
   Graph: PIOLIN ES CANARIO ES AVE; PINGUINO ES AVE (denied flight);
   MIMI/TOM ES GATO; GATO COME RATON. */

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

static NP_QUERY Q3(SYMBOL_ID s, SYMBOL_ID p, SYMBOL_ID o)
{
    NP_QUERY q;
    q.subject = NPConst(s);
    q.predicate = p;
    q.object = NPConst(o);
    return q;
}

static int HasObj(NP_SOLUTION *sol, uint32_t n, SYMBOL_ID o)
{
    uint32_t i;
    for (i = 0; i < n; i++)
        if (sol[i].object == o)
            return 1;
    return 0;
}

static int HasSubj(NP_SOLUTION *sol, uint32_t n, SYMBOL_ID s)
{
    uint32_t i;
    for (i = 0; i < n; i++)
        if (sol[i].subject == s)
            return 1;
    return 0;
}

int main(void)
{
    printf("========================================\n");
    printf("  NEURO-PROLOG RULES - GATE P4b        \n");
    printf("========================================\n\n");

    GRAPH *g = GraphCreate(64, 64);
    Check(g != NULL, "GraphCreate(64,64)");

    GraphAddRelation(g, GraphAddSymbol(g, "PIOLIN"), GraphAddSymbol(g, "ES"),
                     GraphAddSymbol(g, "CANARIO"));
    GraphAddRelation(g, Sy(g, "CANARIO"), Sy(g, "ES"),
                     GraphAddSymbol(g, "AVE"));
    GraphAddRelation(g, GraphAddSymbol(g, "PINGUINO"), Sy(g, "ES"),
                     Sy(g, "AVE"));
    GraphAddRelation(g, GraphAddSymbol(g, "MIMI"), Sy(g, "ES"),
                     GraphAddSymbol(g, "GATO"));
    GraphAddRelation(g, GraphAddSymbol(g, "TOM"), Sy(g, "ES"),
                     Sy(g, "GATO"));
    GraphAddRelation(g, Sy(g, "GATO"), GraphAddSymbol(g, "COME"),
                     GraphAddSymbol(g, "RATON"));

    SYMBOL_ID s_piolin = Sy(g, "PIOLIN");
    SYMBOL_ID s_pingu  = Sy(g, "PINGUINO");
    SYMBOL_ID s_mimi   = Sy(g, "MIMI");
    SYMBOL_ID s_tom    = Sy(g, "TOM");
    SYMBOL_ID s_es     = Sy(g, "ES");
    SYMBOL_ID s_ave    = Sy(g, "AVE");
    SYMBOL_ID s_vuela  = GraphAddSymbol(g, "VUELA");
    SYMBOL_ID s_cielo  = GraphAddSymbol(g, "CIELO");
    SYMBOL_ID s_canta  = GraphAddSymbol(g, "CANTA");
    SYMBOL_ID s_bosque = GraphAddSymbol(g, "BOSQUE");
    SYMBOL_ID s_vive   = GraphAddSymbol(g, "VIVE");
    SYMBOL_ID s_casa   = GraphAddSymbol(g, "CASA");
    SYMBOL_ID s_duerme = GraphAddSymbol(g, "DUERME");
    SYMBOL_ID s_sofa   = GraphAddSymbol(g, "SOFA");
    SYMBOL_ID s_respira = GraphAddSymbol(g, "RESPIRA");
    SYMBOL_ID s_aire   = GraphAddSymbol(g, "AIRE");
    SYMBOL_ID s_gato   = Sy(g, "GATO");
    SYMBOL_ID s_canario = Sy(g, "CANARIO");

    /* Explicit denial: PINGUINO does not fly (M1-style negative). */
    Check(GraphAddRelationPolar(g, s_pingu, s_vuela, s_cielo,
                                POLARITY_NEGATIVE,
                                CONFLICT_REJECT_NEW) != 0,
          "denial PINGUINO VUELA CIELO stored");

    NPRulesClear();

    /* R1: VUELA(X) :- ES(X, AVE). */
    {
        NP_RULE r;
        memset(&r, 0, sizeof(r));
        r.head.subject = NPVar(0);
        r.head.predicate = s_vuela;
        r.head.object = NPConst(s_cielo);
        r.body[0].subject = NPVar(0);
        r.body[0].predicate = s_es;
        r.body[0].object = NPConst(s_ave);
        r.body[0].negated = 0;
        r.nbody = 1;
        r.cut = 0;
        Check(NPRuleDeclare(&r) == 0, "declare R1 VUELA(X) :- ES(X,AVE)");
    }
    /* R2: CANTA(X) :- ES(X,AVE), NAF VUELA(X). */
    {
        NP_RULE r;
        memset(&r, 0, sizeof(r));
        r.head.subject = NPVar(0);
        r.head.predicate = s_canta;
        r.head.object = NPConst(s_bosque);
        r.body[0].subject = NPVar(0);
        r.body[0].predicate = s_es;
        r.body[0].object = NPConst(s_ave);
        r.body[0].negated = 0;
        r.body[1].subject = NPVar(0);
        r.body[1].predicate = s_vuela;
        r.body[1].object = NPConst(s_cielo);
        r.body[1].negated = 1;
        r.nbody = 2;
        r.cut = 0;
        Check(NPRuleDeclare(&r) == 1, "declare R2 with NAF");
    }
    /* R3: VIVE(X) :- ES(X,GATO), cut. R4: DUERME(X) :- ES(X,GATO). */
    {
        NP_RULE r;
        memset(&r, 0, sizeof(r));
        r.head.subject = NPVar(0);
        r.head.predicate = s_vive;
        r.head.object = NPConst(s_casa);
        r.body[0].subject = NPVar(0);
        r.body[0].predicate = s_es;
        r.body[0].object = NPConst(s_gato);
        r.body[0].negated = 0;
        r.nbody = 1;
        r.cut = 1;
        Check(NPRuleDeclare(&r) == 2, "declare R3 with cut");
        memset(&r, 0, sizeof(r));
        r.head.subject = NPVar(0);
        r.head.predicate = s_duerme;
        r.head.object = NPConst(s_sofa);
        r.body[0].subject = NPVar(0);
        r.body[0].predicate = s_es;
        r.body[0].object = NPConst(s_gato);
        r.body[0].negated = 0;
        r.nbody = 1;
        r.cut = 0;
        Check(NPRuleDeclare(&r) == 3, "declare R4 no cut");
    }
    /* R5: RESPIRA(X) :- VUELA(X) (rule-over-rule chain). */
    {
        NP_RULE r;
        memset(&r, 0, sizeof(r));
        r.head.subject = NPVar(0);
        r.head.predicate = s_respira;
        r.head.object = NPConst(s_aire);
        r.body[0].subject = NPVar(0);
        r.body[0].predicate = s_vuela;
        r.body[0].object = NPConst(s_cielo);
        r.body[0].negated = 0;
        r.nbody = 1;
        r.cut = 0;
        Check(NPRuleDeclare(&r) == 4, "declare R5 chain");
    }
    /* Malformed declarations rejected. */
    {
        NP_RULE r;
        NP_RULE bad;
        memset(&r, 0, sizeof(r));
        memset(&bad, 0, sizeof(bad));
        Check(NPRuleDeclare(NULL) < 0, "reject NULL rule");
        Check(NPRuleDeclare(&bad) < 0, "reject empty rule");
    }

    {
        NP_SOLUTION sol[32];
        NP_OPTIONS opt = {NP_MIN_CONF_DEF, 8, 32, 0, NP_FUZZY_GATE};
        NP_QUERY q;
        uint32_t n;

        /* 1. Exact fact still found through the rules API. */
        q = Q3(s_gato, Sy(g, "COME"), Sy(g, "RATON"));
        n = NPProveRules(g, &q, &opt, sol, 32);
        Check(n == 1 && sol[0].depth == 0, "fact via rules API");

        /* 2-3. Rule fires with inheritance; confidence decays once. */
        q.subject = NPVar(0);
        q.predicate = s_vuela;
        q.object = NPConst(s_cielo);
        n = NPProveRules(g, &q, &opt, sol, 32);
        Check(HasSubj(sol, n, s_piolin), "R1 derives PIOLIN VUELA");
        Check(!HasSubj(sol, n, s_pingu), "denial vetoes PINGUINO VUELA");
        {
            int ok = 0;
            uint32_t i;
            for (i = 0; i < n; i++)
                if (sol[i].subject == s_piolin &&
                    sol[i].confidence > 0.80f &&
                    sol[i].confidence < 0.82f)
                    ok = 1;
            Check(ok, "R1 confidence ~0.81 (0.9 rule x 0.9 Leibniz)");
        }

        /* 4-5. NAF: PIOLIN flies (blocked), PINGUINO cannot (fires). */
        q.subject = NPVar(0);
        q.predicate = s_canta;
        q.object = NPConst(s_bosque);
        n = NPProveRules(g, &q, &opt, sol, 32);
        Check(!HasObj(sol, n, s_piolin), "NAF blocks PIOLIN CANTA");
        Check(HasSubj(sol, n, s_pingu), "NAF fires PINGUINO CANTA");
        Check(n == 1 && sol[0].subject == s_pingu,
              "CANTA exactly PINGUINO");

        /* 6-7. Cut commits first; no-cut unions. Subjects asserted. */
        q.subject = NPVar(0);
        q.predicate = s_vive;
        q.object = NPConst(s_casa);
        n = NPProveRules(g, &q, &opt, sol, 32);
        Check(n == 1, "cut: VIVE exactly 1 despite MIMI+TOM");
        if (n == 1)
            Check(sol[0].subject == s_mimi || sol[0].subject == s_tom,
                  "cut: the one is MIMI or TOM");
        q.predicate = s_duerme;
        q.object = NPConst(s_sofa);
        n = NPProveRules(g, &q, &opt, sol, 32);
        Check(n == 2, "no cut: DUERME MIMI+TOM");
        if (n == 2)
            Check(HasSubj(sol, n, s_mimi) && HasSubj(sol, n, s_tom),
                  "DUERME subjects are MIMI+TOM");
    }

    {
        /* 8-10. Rule-over-rule chain, depth cap, honest unknown. */
        NP_SOLUTION sol[32];
        NP_OPTIONS opt = {NP_MIN_CONF_DEF, 8, 32, 0, NP_FUZZY_GATE};
        NP_QUERY q;
        uint32_t n, i;
        int ok;
        q.subject = NPVar(0);
        q.predicate = s_respira;
        q.object = NPConst(s_aire);
        n = NPProveRules(g, &q, &opt, sol, 32);
        Check(HasSubj(sol, n, s_piolin), "R5 chain derives PIOLIN RESPIRA");
        Check(!HasSubj(sol, n, s_pingu), "chain respects denial veto");
        ok = 0;
        for (i = 0; i < n; i++)
            if (sol[i].subject == s_piolin && sol[i].depth == 1)
                ok = 1;
        Check(ok, "R5 solution depth 1 (nesting level)");
        opt.max_depth = 0;
        n = NPProveRules(g, &q, &opt, sol, 32);
        Check(n == 0, "max_depth 0: no rule solutions");
        q.subject = NPVar(0);
        q.predicate = GraphAddSymbol(g, "NADA");
        q.object = NPVar(1);
        n = NPProveRules(g, &q, &opt, sol, 32);
        Check(n == 0, "honest unknown on empty predicate");
    }

    {
        /* 11. Cycle terminates (A:-B, B:-A) under the depth cap. */
        NP_SOLUTION sol[32];
        NP_OPTIONS opt = {NP_MIN_CONF_DEF, 8, 32, 0, NP_FUZZY_GATE};
        NP_QUERY q;
        NP_RULE r;
        uint32_t n;
        memset(&r, 0, sizeof(r));
        r.head.subject = NPVar(0);
        r.head.predicate = GraphAddSymbol(g, "CICLA");
        r.head.object = NPConst(s_cielo);
        r.body[0].subject = NPVar(0);
        r.body[0].predicate = GraphAddSymbol(g, "CICLB");
        r.body[0].object = NPConst(s_cielo);
        r.body[0].negated = 0;
        r.nbody = 1;
        Check(NPRuleDeclare(&r) == 5, "declare cyclic R6a");
        memset(&r, 0, sizeof(r));
        r.head.subject = NPVar(0);
        r.head.predicate = GraphAddSymbol(g, "CICLB");
        r.head.object = NPConst(s_cielo);
        r.body[0].subject = NPVar(0);
        r.body[0].predicate = GraphAddSymbol(g, "CICLA");
        r.body[0].object = NPConst(s_cielo);
        r.body[0].negated = 0;
        r.nbody = 1;
        Check(NPRuleDeclare(&r) == 6, "declare cyclic R6b");
        q.subject = NPVar(0);
        q.predicate = Sy(g, "CICLA");
        q.object = NPConst(s_cielo);
        n = NPProveRules(g, &q, &opt, sol, 32);
        Check(n == 0, "cycle terminates with no solutions");
    }

    GraphDestroy(g);
    printf("----------------------------------------\n");
    printf("checks=%d fails=%d\n", CHECKS, FAILS);
    return FAILS == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
