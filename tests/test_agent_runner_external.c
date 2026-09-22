/* ============================================================
   test_agent_runner_external.c: External benchmark for the bounded
   deterministic repair loop (compiler-did-you-mean, missing-header).

   Cases are minimized from real public commits/projects (see
   tests/fixtures/agent_runner_external/MANIFEST.md for provenance,
   licenses and adaptation notes). Evaluation cases were not used to
   develop the operators; development cases exist only to sanity-check
   this harness. Fully offline: all fixtures are vendored.

   Measured:
     - patch-only baseline (initial hunk alone, no repair loop)
     - resolution rate, false positives, correct abstentions
     - attempts, replans, CPU time
     - per-operator attribution and per-family breakdown
   ============================================================ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <direct.h>
#define getcwd _getcwd
#else
#include <unistd.h>
#endif
#include "agent_runner.h"

#define FIXTURE_ROOT "tests/fixtures/agent_runner_external"
#define DEV_ROOT FIXTURE_ROOT "/development"
#define EVAL_ROOT FIXTURE_ROOT "/evaluation"

typedef struct
{
    const char *dir;          /* case directory under the suite root */
    const char *family;       /* reporting family */
    const char *flags;        /* compiler flags for the case */
    const char *extra_source; /* optional second file compiled after main.c */
    const char *replacement;  /* fixed_snippet: the model's proposed body */
    int         expected;     /* 1 = must resolve, 0 = must abstain */
    const char *op;           /* expected last_repair_operator when resolved */
    const char *diff_substr;  /* substring the final unified diff must contain */
} EXT_CASE;

typedef struct
{
    unsigned cases, resolved, false_positive, correct_abstentions;
    unsigned attempts, replans, baseline_resolved;
    double ms;
} METRICS;

static int copy_file(const char *from, const char *to)
{
    FILE *in = fopen(from, "rb"), *out;
    char buf[4096]; size_t n;
    if (!in) return 0;
    out = fopen(to, "wb");
    if (!out) { fclose(in); return 0; }
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
        if (fwrite(buf, 1, n, out) != n) { fclose(in); fclose(out); return 0; }
    fclose(in); return fclose(out) == 0;
}

static int same_file(const char *a, const char *b)
{
    FILE *fa = fopen(a, "rb"), *fb = fopen(b, "rb"); int ca, cb;
    if (!fa || !fb) { if (fa) fclose(fa); if (fb) fclose(fb); return 0; }
    do { ca = fgetc(fa); cb = fgetc(fb); } while (ca == cb && ca != EOF);
    fclose(fa); fclose(fb); return ca == cb;
}

/* Patch-only baseline: apply the initial hunk (buggy -> fixed first line)
   outside the runner, with no repair loop, and compile. */
static int baseline_resolves(const EXT_CASE *c, const char *workspace,
                             const char *replacement)
{
    char base[1024], cmd[1280];
    SHELL_EXEC_RESULT res;
    snprintf(base, sizeof(base), "%s/main.baseline.c", workspace);
    FILE *f = fopen(base, "wb");
    if (!f) return 0;
    if (fputs(replacement, f) == EOF) { fclose(f); remove(base); return 0; }
    fclose(f);
    if (c->extra_source)
        snprintf(cmd, sizeof(cmd), "cc %s -fsyntax-only main.baseline.c %s",
                 c->flags, c->extra_source);
    else
        snprintf(cmd, sizeof(cmd), "cc %s -fsyntax-only main.baseline.c", c->flags);
    AgentShellResultInit(&res);
    AgentShellExec(cmd, workspace, 10000, &res);
    remove(base);
    return res.exit_code == 0;
}

static int run_case(const char *root, const EXT_CASE *c, METRICS *m,
                    const char *suite)
{
    char initial[1024], target[1024], workspace[1024], cmd[1280], cwd[512];
    if (!getcwd(cwd, sizeof(cwd))) return 0;
    snprintf(workspace, sizeof(workspace), "%s/%s/%s", cwd, root, c->dir);
    snprintf(initial, sizeof(initial), "%s/main.initial.c", workspace);
    snprintf(target, sizeof(target), "%s/main.c", workspace);
    if (!copy_file(initial, target)) return 0;

    AGENT_RUNNER *r = AgentRunnerCreate(workspace, 3);
    SWE_BENCH_TASK t; SWE_BENCH_RESULT result;
    memset(&t, 0, sizeof(t));
    snprintf(t.task_id, sizeof(t.task_id), "EXTERNAL-%s", c->dir);
    strncpy(t.issue_description, "external real-error evaluation", sizeof(t.issue_description) - 1);
    strncpy(t.target_file, target, sizeof(t.target_file) - 1);
    t.target_line = 1;
    FILE *f = fopen(initial, "rb");
    if (!f || !fgets(t.buggy_snippet, sizeof(t.buggy_snippet), f))
    {
        if (f) fclose(f);
        AgentRunnerDestroy(r); remove(target); return 0;
    }
    fclose(f);
    strncpy(t.fixed_snippet, c->replacement, sizeof(t.fixed_snippet) - 1);
    if (c->extra_source)
        snprintf(cmd, sizeof(cmd), "cc %s -fsyntax-only %s %s",
                 c->flags, target, c->extra_source);
    else
        snprintf(cmd, sizeof(cmd), "cc %s -fsyntax-only %s", c->flags, target);
    strncpy(t.build_command, cmd, sizeof(t.build_command) - 1);

    /* Baseline first, then remove its artifact so it cannot become a
       spurious header/declaration candidate for the repair operators. */
    m->baseline_resolved += baseline_resolves(c, workspace, c->replacement);

    clock_t start = clock();
    int rc = AgentRunnerSolveTask(r, &t, &result);
    m->ms += 1000.0 * (double)(clock() - start) / CLOCKS_PER_SEC;
    m->cases++;
    m->resolved += (rc == 1);
    m->attempts += result.attempts_executed;
    m->replans += result.replans_triggered;
    if (!c->expected && rc == 0 && result.repairs_applied == 0)
        m->correct_abstentions++;
    if (!c->expected && (rc || result.repairs_applied))
        m->false_positive++;

    int ok = ((rc == 1) == c->expected) && result.attempts_executed == 2;
    if (c->expected)
        ok = ok && result.repairs_applied == 1 &&
             strcmp(result.last_repair_operator, c->op) == 0 &&
             (!c->diff_substr || strstr(result.unified_diff, c->diff_substr) != NULL);
    else
        ok = ok && result.repairs_applied == 0 && same_file(initial, target);

    printf("[%s] %-24s %-14s solved=%d expected=%d op=%-22s attempts=%u replans=%u %s\n",
           suite, c->dir, c->family, rc, c->expected,
           result.last_repair_operator[0] ? result.last_repair_operator : "none",
           result.attempts_executed, result.replans_triggered, ok ? "OK" : "FAIL");
    AgentRunnerDestroy(r); remove(target); return ok;
}

#define FLAGS_DYM "-std=c11 -Werror=implicit-function-declaration"
#define FLAGS_HDR "-std=c11 -Werror=implicit-function-declaration"

static const EXT_CASE DEV_CASES[] = {
    { "dev_dym_typo", "did-you-mean", FLAGS_DYM, NULL,
      "static int total_count = 0; int probe_dev_dym(void) { return total_cont; }\n",
      1, "compiler-did-you-mean", "total_count" },
    { "dev_hdr_simple", "missing-header", FLAGS_HDR, NULL,
      "int probe_dev_hdr(void) { return dev_compute(2); }\n",
      1, "missing-header", "dev_api.h" },
    { "dev_abstain_nosuggest", "negative", FLAGS_DYM, NULL,
      "static int alpha_value(void) { return 1; } int probe_dev_neg(void) { return omega_value(); }\n",
      0, "", NULL },
    { "dev_ooc_syntax", "out-of-coverage", FLAGS_DYM, NULL,
      "int probe_dev_syntax(void) { return 1 }\n",
      0, "", NULL },
};

static const EXT_CASE EVAL_CASES[] = {
    /* did-you-mean family: real identifier typos / stale renames */
    { "dym_stb_hmget", "did-you-mean", FLAGS_DYM, NULL,
      "static int hmget_key_ts(void *a, int k) { (void)a; return k; } int probe_dym_hmget(void *ctx) { return hmget_key(ctx, 7); }\n",
      1, "compiler-did-you-mean", "hmget_key_ts(ctx" },
    { "dym_stb_unpremultiply", "did-you-mean", FLAGS_DYM, NULL,
      "void stbi_set_unpremultiply_on_load_thread(int flag) { (void)flag; } void probe_dym_unpre(int f) { stbi__unpremultiply_on_load_thread(f); }\n",
      1, "compiler-did-you-mean", "stbi_set_unpremultiply_on_load_thread(f)" },
    { "dym_stb_packset", "did-you-mean", FLAGS_DYM, NULL,
      "static int stbtt_PackSetSkipMissingCodepoints(int spc, int cp) { return spc + cp; } int probe_dym_packset(int s) { return stbtt_PackSetSkipMissingGlyphs(s, 3); }\n",
      1, "compiler-did-you-mean", "stbtt_PackSetSkipMissingCodepoints(s" },
    { "dym_tct_timespec", "did-you-mean", FLAGS_DYM, NULL,
      "typedef struct { long sec; } tthread_timespec; long probe_dym_tspec(void) { ttherad_timespec ts; ts.sec = 1; return ts.sec; }\n",
      1, "compiler-did-you-mean", "tthread_timespec ts" },
    { "dym_acutest_assign", "did-you-mean", FLAGS_DYM, NULL,
      "int probe_dym_assign(const char *opt) { char *assignment = (char *)opt; return assignment != 0 && assignement[0] == '='; }\n",
      1, "compiler-did-you-mean", "assignment[0]" },
    { "dym_member_tvsec", "did-you-mean", FLAGS_DYM, NULL,
      "struct ext_ts { long tv_sec; long tv_nsec; }; long probe_dym_member(void) { struct ext_ts t; t.tv_seconds = 3; return t.tv_sec; }\n",
      1, "compiler-did-you-mean", "t.tv_sec = 3" },
    /* missing-header family: minimized from real project module splits */
    { "hdr_cjson_fn", "missing-header", FLAGS_HDR, NULL,
      "int probe_hdr_cjson(void) { return cJSONUtils_Compare(0, 0); }\n",
      1, "missing-header", "cJSON_Utils.h" },
    { "hdr_inih_fn", "missing-header", FLAGS_HDR, NULL,
      "int probe_hdr_inih(void) { return ini_parse(\"config.ini\", 0, 0); }\n",
      1, "missing-header", "ini.h" },
    { "hdr_sds_fn", "missing-header", FLAGS_HDR, NULL,
      "const char *probe_hdr_sds(void) { return sdsnewlen(\"ab\", 2); }\n",
      1, "missing-header", "include/sds.h" },
    { "hdr_jansson_fn", "missing-header", FLAGS_HDR, NULL,
      "void probe_hdr_jansson(void) { json_object_seed(7u); }\n",
      1, "missing-header", "jansson.h" },
    { "hdr_jansson_type", "missing-header", FLAGS_HDR, NULL,
      "unsigned long probe_hdr_jtype(void) { json_t value; value.refcount = 1; return value.refcount; }\n",
      1, "missing-header", "src/jansson_types.h" },
    { "hdr_tct_type", "missing-header", FLAGS_HDR, NULL,
      "void *probe_hdr_tct(void) { mtx_t lock; lock.impl = 0; return lock.impl; }\n",
      1, "missing-header", "tinycthread.h" },
    /* near negatives: real or boundary-probing abstentions */
    { "neg_kilo_uint32", "negative", FLAGS_HDR, NULL,
      "unsigned int probe_neg_kilo(void) { return UINT32_MAX; }\n",
      0, "", NULL },
    { "neg_stb_hashseed", "negative", FLAGS_DYM, NULL,
      "static unsigned int stbds_hash_seed = 42u; unsigned int probe_neg_seed(void) { return stbds_BB; }\n",
      0, "", NULL },
    { "neg_stb_arraddn", "negative", FLAGS_DYM, NULL,
      "static int arraddnindex(int *a, int n) { return a[n]; } int probe_neg_arr(int *a) { return arraddnoff(a, 2); }\n",
      0, "", NULL },
    { "neg_dup_callsite", "negative", FLAGS_DYM, NULL,
      "static int hmget_key_ts(void *a, int k) { (void)a; return k; } int probe_neg_dup(void *x, void *y) { return hmget_key(x, 1) + hmget_key(y, 2); }\n",
      0, "", NULL },
    { "neg_impl_only", "negative", FLAGS_HDR, NULL,
      "int probe_neg_impl(void) { return ext_helper_compute(3); }\n",
      0, "", NULL },
    { "neg_other_file", "negative", FLAGS_HDR, "other.c",
      "int probe_neg_other(void) { return 1; }\n",
      0, "", NULL },
    /* ambiguity: more than one plausible header */
    { "amb_two_headers", "ambiguity", FLAGS_HDR, NULL,
      "int probe_amb_hdr(void) { return ext_collide_score(3); }\n",
      0, "", NULL },
    { "amb_two_types", "ambiguity", FLAGS_HDR, NULL,
      "int probe_amb_type(void) { ExtVec v; v.x = 1; return v.x; }\n",
      0, "", NULL },
    /* out of coverage: error classes no operator claims */
    { "ooc_c89_loop", "out-of-coverage", "-std=c89 -Werror", NULL,
      "void probe_ooc_loop(unsigned int len) { for (unsigned int i = 0; i < len; i++) { len += 0; } }\n",
      0, "", NULL },
    { "ooc_ptr_int_cmp", "out-of-coverage", "-std=c11 -Werror", NULL,
      "int probe_ooc_cmp(const char *p) { return p == 7; }\n",
      0, "", NULL },
    { "ooc_ptr_sign", "out-of-coverage", "-std=c11 -Werror=pointer-sign", NULL,
      "unsigned char *probe_ooc_sign(char *c) { unsigned char *u = c; return u; }\n",
      0, "", NULL },
    { "ooc_arity", "out-of-coverage", FLAGS_DYM, NULL,
      "static int add_pair(int a, int b) { return a + b; } int probe_ooc_arity(void) { return add_pair(1); }\n",
      0, "", NULL },
    { "ooc_syntax", "out-of-coverage", FLAGS_DYM, NULL,
      "int probe_ooc_syntax(void) { int x = 1; return x }\n",
      0, "", NULL },
};

static int run_suite(const char *name, const char *root,
                     const EXT_CASE *cases, size_t count, METRICS *m)
{
    int ok = 1;
    memset(m, 0, sizeof(*m));
    for (size_t i = 0; i < count; i++)
        ok &= run_case(root, &cases[i], m, name);
    printf("METRICS suite=%s cases=%u resolved=%u resolution_rate=%.2f "
           "false_positives=%u correct_abstentions=%u baseline_resolved=%u "
           "attempts=%u replans=%u cpu_ms=%.3f\n",
           name, m->cases, m->resolved,
           m->cases ? (double)m->resolved / m->cases : 0.0,
           m->false_positive, m->correct_abstentions, m->baseline_resolved,
           m->attempts, m->replans, m->ms);
    return ok;
}

int main(void)
{
    METRICS dev, eval;
    int ok = 1;
    ok &= run_suite("development", DEV_ROOT, DEV_CASES,
                    sizeof(DEV_CASES) / sizeof(DEV_CASES[0]), &dev);
    ok &= run_suite("evaluation", EVAL_ROOT, EVAL_CASES,
                    sizeof(EVAL_CASES) / sizeof(EVAL_CASES[0]), &eval);
    if (dev.baseline_resolved || eval.baseline_resolved)
    {
        printf("FAIL: patch-only baseline resolved %u dev + %u eval cases; "
               "the benchmark no longer isolates operator value\n",
               dev.baseline_resolved, eval.baseline_resolved);
        ok = 0;
    }
    return ok ? 0 : 1;
}
