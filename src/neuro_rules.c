#include <string.h>

#include "neuro_rules.h"
#include "relation.h"

static NP_RULE rule_table[NPR_MAX_RULES];
static uint32_t rule_count = 0;
/* Fresh-slot allocator: renamed-apart rule copies need slots disjoint
   from everything live on the current proof path. Monotonic within a
   top-level query (siblings never share live frames), reset per query.
   Overflow (>16) fails that expansion gracefully (documented cap). */
static uint32_t var_base = 0;

void NPRulesClear(void)
{
    rule_count = 0;
}

int NPRuleDeclare(const NP_RULE *rule)
{
    uint32_t i;
    if (rule == NULL || rule->nbody == 0 || rule->nbody > NPR_MAX_BODY)
        return -1;
    if (rule->head.predicate == SYMBOL_INVALID)
        return -1;
    for (i = 0; i < rule->nbody; i++)
        if (rule->body[i].predicate == SYMBOL_INVALID)
            return -1;
    if (rule_count >= NPR_MAX_RULES)
        return -1;
    rule_table[rule_count] = *rule;
    return (int)(rule_count++);
}

/* ---- internals: SLD over frames; answers travel in pushed triples,
   never in caller frames (P4a convention: the caller reads solutions,
   not bindings). Rule nesting, cut state and NAF probes all thread
   through explicit contexts. Rule variables are renamed apart per
   expansion (fresh slots from a monotonic base); queries should use
   low slots. Single-threaded use. ---- */

/* Copy a rule with every variable slot shifted by base (renamed
   apart). Returns 0 when any slot would leave 0..15. */
static int RenameRule(const NP_RULE *rule, uint32_t base, NP_RULE *out)
{
    uint32_t i;
    *out = *rule;
    if (out->head.subject.type == NP_TERM_VARIABLE)
    {
        if (out->head.subject.id + base >= NP_MAX_VARS)
            return 0;
        out->head.subject.id += base;
    }
    if (out->head.object.type == NP_TERM_VARIABLE)
    {
        if (out->head.object.id + base >= NP_MAX_VARS)
            return 0;
        out->head.object.id += base;
    }
    for (i = 0; i < out->nbody; i++)
    {
        if (out->body[i].subject.type == NP_TERM_VARIABLE)
        {
            if (out->body[i].subject.id + base >= NP_MAX_VARS)
                return 0;
            out->body[i].subject.id += base;
        }
        if (out->body[i].object.type == NP_TERM_VARIABLE)
        {
            if (out->body[i].object.id + base >= NP_MAX_VARS)
                return 0;
            out->body[i].object.id += base;
        }
    }
    return 1;
}

/* Highest variable slot used by a rule (for base advancement). */
static uint32_t RuleMaxVar(const NP_RULE *rule)
{
    uint32_t m = 0, i;
    if (rule->head.subject.type == NP_TERM_VARIABLE &&
        rule->head.subject.id > m)
        m = rule->head.subject.id;
    if (rule->head.object.type == NP_TERM_VARIABLE &&
        rule->head.object.id > m)
        m = rule->head.object.id;
    for (i = 0; i < rule->nbody; i++)
    {
        if (rule->body[i].subject.type == NP_TERM_VARIABLE &&
            rule->body[i].subject.id > m)
            m = rule->body[i].subject.id;
        if (rule->body[i].object.type == NP_TERM_VARIABLE &&
            rule->body[i].object.id > m)
            m = rule->body[i].object.id;
    }
    return m;
}

typedef struct
{
    NP_SOLUTION *out;
    uint32_t     out_max;
    uint32_t     count;
    float        min_conf;
} SOL_ACC;

/* Dedup by triple keeping max confidence (P4a NPPushSolution pattern). */
static void PushSolution(SOL_ACC *acc, SYMBOL_ID s, SYMBOL_ID p,
                         SYMBOL_ID o, float conf, uint32_t depth,
                         uint8_t fuzzy)
{
    uint32_t i;
    if (acc == NULL || conf < acc->min_conf)
        return;
    for (i = 0; i < acc->count; i++)
    {
        if (acc->out[i].subject == s && acc->out[i].predicate == p &&
            acc->out[i].object == o)
        {
            if (conf > acc->out[i].confidence)
                acc->out[i].confidence = conf;
            return;
        }
    }
    if (acc->count >= acc->out_max)
        return;
    acc->out[acc->count].subject = s;
    acc->out[acc->count].predicate = p;
    acc->out[acc->count].object = o;
    acc->out[acc->count].confidence = conf;
    acc->out[acc->count].depth = (depth > 255) ? 255 : (uint8_t)depth;
    acc->out[acc->count].fuzzy = fuzzy;
    acc->count++;
}

/* The penguin line: a resolved triple with no exact positive fact but
   an explicit denial never surfaces, at top level or inside NAF
   probes alike. Fact-level contradiction stays with conflict policies. */
static int DeniedDrop(const GRAPH *graph, SYMBOL_ID s, SYMBOL_ID p,
                      SYMBOL_ID o)
{
    RELATION *pos;
    RELATION *neg;
    if (graph == NULL || graph->relations == NULL)
        return 0;
    pos = RelationFindPolar(graph->relations, s, p, o, POLARITY_POSITIVE);
    if (pos != NULL)
        return 0;
    neg = RelationFindPolar(graph->relations, s, p, o, POLARITY_NEGATIVE);
    return neg != NULL;
}

typedef int (*SOLCB)(NP_FRAME *frame, float conf,
                     SYMBOL_ID s, SYMBOL_ID p, SYMBOL_ID o,
                     uint32_t depth, uint8_t fuzzy, void *ctx);

static int ProveList(const GRAPH *graph, const NP_GOAL *goals,
                     uint32_t ngoals, uint32_t gi, uint32_t fire_at,
                     NP_FRAME *frame, float conf, uint32_t depth,
                     const NP_OPTIONS *opt, SOL_ACC *acc,
                     SOLCB cb, void *ctx);

typedef struct
{
    const GRAPH *graph;
    SOL_ACC     *acc;
} TOPCTX;

static int TopCb(NP_FRAME *frame, float conf,
                 SYMBOL_ID s, SYMBOL_ID p, SYMBOL_ID o,
                 uint32_t depth, uint8_t fuzzy, void *ctx)
{
    TOPCTX *t = (TOPCTX *)ctx;
    (void)frame;
    if (s == SYMBOL_INVALID || p == SYMBOL_INVALID || o == SYMBOL_INVALID)
        return 1; /* completion marker: nothing to push here */
    if (DeniedDrop(t->graph, s, p, o))
        return 1;
    PushSolution(t->acc, s, p, o, conf, depth, fuzzy);
    return 1;
}

/* NAF probe: count veto-aware solutions (graph needed for the veto). */
typedef struct
{
    const GRAPH *graph;
    uint32_t     count;
} PROBECTX;

static int ProbeCb(NP_FRAME *frame, float conf,
                   SYMBOL_ID s, SYMBOL_ID p, SYMBOL_ID o,
                   uint32_t depth, uint8_t fuzzy, void *ctx)
{
    PROBECTX *pb = (PROBECTX *)ctx;
    (void)frame;
    (void)conf;
    (void)depth;
    (void)fuzzy;
    if (s == SYMBOL_INVALID || p == SYMBOL_INVALID || o == SYMBOL_INVALID)
        return 1; /* completion marker: not a solution */
    if (!DeniedDrop(pb->graph, s, p, o))
        pb->count++;
    return 1;
}

/* Nested rule completion: resolve the head under the proving frame,
   unify it into the waiting outer goal, continue the outer list.
   Shallow cut: the first firing commits, the rest are dropped. */
typedef struct
{
    const GRAPH     *graph;
    const NP_GOAL   *goals;
    uint32_t         ngoals;
    uint32_t         gi;
    uint32_t         outer_fire_at;
    SOLCB            outer_cb;
    void            *outer_ctx;
    const NP_RULE   *rule;
    uint32_t         depth;
    const NP_OPTIONS *opt;
    SOL_ACC         *acc;
    int              fired;
} EXPAND;

/* Nested rule completion: resolve this rule's head under the proving
   frame (one decay per rule step), unify it into the waiting outer
   goal, and continue the outer list. If the outer callback is the top
   pusher, the head triple surfaces (veto-aware there). Shallow cut:
   the first firing commits, the rest are dropped. Non-ground heads
   are skipped (range-restricted answers only). */
static int ExpandCb(NP_FRAME *frame, float conf,
                    SYMBOL_ID s, SYMBOL_ID p, SYMBOL_ID o,
                    uint32_t depth, uint8_t fuzzy, void *ctx)
{
    EXPAND *e = (EXPAND *)ctx;
    SYMBOL_ID hs, ho;
    NP_FRAME next;
    const NP_GOAL *g;
    (void)s;
    (void)p;
    (void)o;
    (void)depth;
    (void)fuzzy;
    if (e->rule->cut && e->fired)
        return 1;
    e->fired = 1;
    hs = NPResolve(&e->rule->head.subject, frame);
    ho = NPResolve(&e->rule->head.object, frame);
    if (hs == SYMBOL_INVALID || ho == SYMBOL_INVALID)
        return 1;
    /* Denials poison every derivation through them (transitive penguin
       line), not just top-level answers. */
    if (DeniedDrop(e->graph, hs, e->rule->head.predicate, ho))
        return 1;
    /* NAF probes count veto-aware head triples directly: no outer
       list to continue (their single goal is this completion). */
    if (e->outer_cb == ProbeCb)
    {
        PROBECTX *pb = (PROBECTX *)e->outer_ctx;
        if (!DeniedDrop(e->graph, hs, e->rule->head.predicate, ho))
            pb->count++;
        return 1;
    }
    next = *frame;
    g = &e->goals[e->gi];
    if (g->predicate != SYMBOL_INVALID &&
        g->predicate != e->rule->head.predicate)
        return 1;
    if (!NPUnifyTerm(&g->subject, hs, &next))
        return 1;
    if (!NPUnifyTerm(&g->object, ho, &next))
        return 1;
    if (e->gi + 1 >= e->ngoals)
    {
        /* Outer list exhausted: the head triple IS the answer for
           this level (top pushes it veto-aware; a parent rule
           resolves its own head from these bindings). */
        return e->outer_cb(&next, conf * NP_HOP_DECAY, hs,
                           e->rule->head.predicate, ho, e->depth, 0,
                           e->outer_ctx);
    }
    return ProveList(e->graph, e->goals, e->ngoals,
                     e->gi + 1, e->outer_fire_at, &next,
                     conf * NP_HOP_DECAY, e->depth, e->opt,
                     e->acc, e->outer_cb, e->outer_ctx);
}

static int ProveList(const GRAPH *graph, const NP_GOAL *goals,
                     uint32_t ngoals, uint32_t gi, uint32_t fire_at,
                     NP_FRAME *frame, float conf, uint32_t depth,
                     const NP_OPTIONS *opt, SOL_ACC *acc,
                     SOLCB cb, void *ctx)
{
    const NP_GOAL *goal;
    uint32_t ri;
    if (acc->count >= acc->out_max)
        return 1;
    if (gi >= ngoals)
    {
        /* List exhausted: fire the continuation with a marker. Top
           ignores it (triples arrive per-solution); rule completions
           resolve their own head from the frame. */
        return cb(frame, conf, SYMBOL_INVALID, SYMBOL_INVALID,
                  SYMBOL_INVALID, depth, 0, ctx);
    }
    goal = &goals[gi];
    if (goal->negated)
    {
        /* NAF: succeeds iff the subgoal is unprovable. Never binds:
           scratch frame, veto-aware count. */
        NP_FRAME scratch = *frame;
        PROBECTX pb;
        NP_GOAL sub[1];
        pb.graph = graph;
        pb.count = 0;
        sub[0] = *goal;
        sub[0].negated = 0;
        ProveList(graph, sub, 1, 0, 1, &scratch, 1.0f, depth + 1, opt,
                  acc, ProbeCb, &pb);
        if (pb.count == 0)
            return ProveList(graph, goals, ngoals, gi + 1, fire_at,
                             frame, conf, depth, opt, acc, cb, ctx);
        return 1;
    }
    /* Fact level first (P4a: exact, identity, fuzzy). */
    {
        NP_QUERY sub;
        NP_SOLUTION sols[32];
        NP_OPTIONS subopt = *opt;
        uint32_t nsol, si;
        uint32_t remaining = (depth < opt->max_depth) ?
                             (opt->max_depth - depth) : 0;
        SYMBOL_ID sv = NPResolve(&goal->subject, frame);
        SYMBOL_ID ov = NPResolve(&goal->object, frame);
        sub.subject = (sv != SYMBOL_INVALID) ? NPConst(sv) :
                                                goal->subject;
        sub.predicate = goal->predicate;
        sub.object = (ov != SYMBOL_INVALID) ? NPConst(ov) : goal->object;
        subopt.max_depth = remaining;
        subopt.max_solutions = 32;
        nsol = NPProve(graph, &sub, &subopt, sols, 32);
        for (si = 0; si < nsol; si++)
        {
            NP_FRAME next = *frame;
            float cmin = conf < sols[si].confidence ?
                         conf : sols[si].confidence;
            if (!NPUnifyTerm(&goal->subject, sols[si].subject, &next))
                continue;
            if (!NPUnifyTerm(&goal->object, sols[si].object, &next))
                continue;
            if (gi + 1 >= fire_at)
            {
                if (!cb(&next, cmin, sols[si].subject,
                        sols[si].predicate, sols[si].object,
                        sols[si].depth, sols[si].fuzzy, ctx))
                    return 0;
            }
            else
            {
                if (!ProveList(graph, goals, ngoals, gi + 1, fire_at,
                               &next, cmin, depth, opt, acc, cb, ctx))
                    return 0;
            }
            if (acc->count >= acc->out_max)
                return 1;
        }
    }
    /* Rule level (capped nesting, declaration order). Each expansion
       works on a renamed-apart copy: fresh slots above everything the
       query and the live path use. */
    if (depth < opt->max_depth)
    {
        for (ri = 0; ri < rule_count; ri++)
        {
            const NP_RULE *rule = &rule_table[ri];
            NP_RULE rc;
            NP_FRAME next = *frame;
            SYMBOL_ID qs, qo;
            EXPAND e;
            if (rule->head.predicate != goal->predicate &&
                goal->predicate != SYMBOL_INVALID)
                continue;
            if (!RenameRule(rule, var_base, &rc))
                continue;
            var_base += RuleMaxVar(rule) + 1;
            rule = &rc;
            qs = NPResolve(&goal->subject, frame);
            qo = NPResolve(&goal->object, frame);
            if (rule->head.subject.type == NP_TERM_CONSTANT)
            {
                if (qs != SYMBOL_INVALID &&
                    qs != rule->head.subject.id)
                    continue;
                if (!NPUnifyTerm(&goal->subject, rule->head.subject.id,
                                 &next))
                    continue;
            }
            else if (qs != SYMBOL_INVALID &&
                     !NPUnifyTerm(&rule->head.subject, qs, &next))
                continue;
            if (rule->head.object.type == NP_TERM_CONSTANT)
            {
                if (qo != SYMBOL_INVALID &&
                    qo != rule->head.object.id)
                    continue;
                if (!NPUnifyTerm(&goal->object, rule->head.object.id,
                                 &next))
                    continue;
            }
            else if (qo != SYMBOL_INVALID &&
                     !NPUnifyTerm(&rule->head.object, qo, &next))
                continue;
            e.goals = goals;
            e.ngoals = ngoals;
            e.gi = gi;
            e.outer_fire_at = fire_at;
            e.graph = graph;
            e.outer_cb = cb;
            e.outer_ctx = ctx;
            e.rule = rule;
            e.depth = depth + 1;
            e.opt = opt;
            e.acc = acc;
            e.fired = 0;
            ProveList(graph, rule->body, rule->nbody, 0, rule->nbody,
                      &next, conf, depth + 1, opt, acc, ExpandCb, &e);
            if (acc->count >= acc->out_max)
                return 1;
        }
    }
    return 1;
}

uint32_t NPProveRules(const GRAPH *graph,
                      const NP_QUERY *query,
                      const NP_OPTIONS *opt,
                      NP_SOLUTION *out,
                      uint32_t out_max)
{
    SOL_ACC acc;
    TOPCTX tctx;
    NP_FRAME frame;
    NP_OPTIONS realopt;
    NP_GOAL top[1];
    if (graph == NULL || query == NULL || opt == NULL || out == NULL ||
        out_max == 0)
        return 0;
    memset(&frame, 0, sizeof(frame));
    realopt = *opt;
    if (realopt.max_solutions == 0 || realopt.max_solutions > out_max)
        realopt.max_solutions = out_max;
    /* Fresh slots start above the query's own: queries use low slots,
       renamed rules live above them on every proof path. */
    var_base = 0;
    if (query->subject.type == NP_TERM_VARIABLE &&
        query->subject.id + 1 > var_base)
        var_base = query->subject.id + 1;
    if (query->object.type == NP_TERM_VARIABLE &&
        query->object.id + 1 > var_base)
        var_base = query->object.id + 1;
    acc.out = out;
    acc.out_max = realopt.max_solutions;
    acc.count = 0;
    acc.min_conf = opt->min_conf;
    tctx.graph = graph;
    tctx.acc = &acc;
    top[0].subject = query->subject;
    top[0].predicate = query->predicate;
    top[0].object = query->object;
    top[0].negated = 0;
    ProveList(graph, top, 1, 0, 1, &frame, 1.0f, 0, &realopt, &acc,
              TopCb, &tctx);
    return acc.count;
}
