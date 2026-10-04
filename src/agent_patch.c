/* ============================================================
   agent_patch.c: Surgical Editing, Unified Diff & Atomic Rollback
                  Engine for Autonomous Agentic Coding.
   Pure ISO C11, zero tensors, zero backprop, fail-closed verification.
   ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif
#include "agent_patch.h"
#include "fs_read.h"
#include "fs_replace.h"

/* Helper to count newline-separated lines in a string */
static uint32_t count_lines(const char *text)
{
    if (!text || text[0] == '\0')
        return 0;

    uint32_t lines = 1;
    for (const char *p = text; *p; p++)
    {
        if (*p == '\n' && *(p + 1) != '\0')
            lines++;
    }
    return lines;
}

/* Helper to strip '\r' from a string, returning a newly allocated LF-only string */
static char *strip_cr(const char *src)
{
    if (!src)
        return NULL;

    size_t len = strlen(src);
    char *dst = (char *)malloc(len + 1);
    if (!dst)
        return NULL;

    size_t j = 0;
    for (size_t i = 0; i < len; i++)
    {
        if (src[i] != '\r')
            dst[j++] = src[i];
    }
    dst[j] = '\0';
    return dst;
}

/* Per-newline ending flags. work_buf is LF-normalised; flags[i] is 1 when
   work_buf[i] is a '\n' that the file wrote as "\r\n". Untouched lines keep
   the ending they had on disk (mixed files stay mixed). A lone '\r' that is
   not followed by '\n' is still dropped, as before (not measured here). */
static unsigned char *eol_flags_from(const char *orig, size_t *dflt_out)
{
    size_t len = strlen(orig);
    unsigned char *fl = (unsigned char *)calloc(len + 1, 1);
    if (!fl) return NULL;
    size_t j = 0;
    int have_dflt = 0;
    *dflt_out = 0;
    for (size_t i = 0; i < len; i++)
    {
        if (orig[i] == '\r') continue;
        if (orig[i] == '\n')
        {
            fl[j] = (i > 0 && orig[i - 1] == '\r') ? 1 : 0;
            if (!have_dflt) { *dflt_out = fl[j]; have_dflt = 1; }
        }
        j++;
    }
    return fl;
}

#ifdef AGENT_PATCH_EOL_MUTANT /* test-only: the old "any CRLF means all CRLF" rule */
#define EOL_SET(fl, i, len) (memchr((fl), 1, (len) + 1) != NULL)
#else
#define EOL_SET(fl, i, len) ((fl)[(i)])
#endif

/* Write src with CR inserted before every '\n' whose flag is set. */
static char *restore_eol(const char *src, const unsigned char *fl, size_t *out_size)
{
    if (!src)
        return NULL;
    size_t len = strlen(src), extra = 0;
    for (size_t i = 0; i < len; i++)
    {
        if (src[i] == '\n' && fl && EOL_SET(fl, i, len)) extra++;
    }
    char *dst = (char *)malloc(len + extra + 1);
    if (!dst) return NULL;
    size_t j = 0;
    for (size_t i = 0; i < len; i++)
    {
        if (src[i] == '\n' && fl && EOL_SET(fl, i, len)) dst[j++] = '\r';
        dst[j++] = src[i];
    }
    dst[j] = '\0';
    if (out_size) *out_size = j;
    return dst;
}

/* ============================================================
   Workspace-confined I/O on the Fs* API (M1-3).
   Every read and write of a target goes through a held workspace root:
   FsReadFile for reads, FsReplaceFile (exact expected bytes) for writes.
   Absolute targets are accepted only when they lie under the workspace.
   ".." escapes, symlinked or hard-linked targets, files over the Fs* 1 MiB
   limit and non-NTFS Windows volumes fail closed. There is no fopen fallback.
   ============================================================ */

#ifdef AGENT_PATCH_TEST_HOOK
void (*g_patch_test_before_replace)(void) = NULL;
#endif

#define PATCH_IO_MAX (1024u * 1024u) /* FS_READ_MAX */

static void set_diag(char *dst, const char *msg)
{
    if (dst)
        snprintf(dst, MAX_PATCH_DIAG, "%s", msg);
}

static const char *fs_status_text(FS_READ_STATUS s)
{
    switch (s)
    {
    case FS_READ_OK:          return "ok";
    case FS_READ_INVALID:     return "invalid path, outside workspace, or over the 1 MiB limit";
    case FS_READ_MISSING:     return "file missing";
    case FS_READ_DENIED:      return "denied: file changed, is a link, or an Fs transaction is pending";
    case FS_READ_UNSUPPORTED: return "unsupported on this filesystem (NTFS required on Windows)";
    case FS_READ_PENDING:     return "replace pending recovery";
    default:                  return "I/O error";
    }
}

static bool path_is_abs(const char *t)
{
    return t[0] == '/' || t[0] == '\\' ||
           (((t[0] >= 'A' && t[0] <= 'Z') || (t[0] >= 'a' && t[0] <= 'z')) && t[1] == ':');
}

static char norm_ch(char c)
{
#ifdef _WIN32
    if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
#endif
    return c == '\\' ? '/' : c;
}

/* Make `p` absolute: keep absolute paths, otherwise prefix the current directory. */
static bool make_abs(const char *p, char *out, size_t out_size)
{
    if (path_is_abs(p))
    {
        if (strlen(p) >= out_size) return false;
        strcpy(out, p);
        return true;
    }
    char cwd[MAX_PATCH_PATH * 2];
#ifdef _WIN32
    if (!_getcwd(cwd, (int)sizeof(cwd))) return false;
#else
    if (!getcwd(cwd, sizeof(cwd))) return false;
#endif
    int n = strcmp(p, ".") == 0 ? snprintf(out, out_size, "%s", cwd)
                                : snprintf(out, out_size, "%s/%s", cwd, p);
    return n > 0 && (size_t)n < out_size;
}

/* Derive the workspace-relative path of `target`. A relative target keeps its
   historical meaning (relative to the current directory). Either way the
   target must lie beneath the workspace; anything else is rejected. */
int PatchResolveTarget(const char *workspace, const char *target, char *rel, size_t rel_size)
{
    const char *ws = (workspace && workspace[0]) ? workspace : ".";
    char abs_ws[MAX_PATCH_PATH * 4], abs_t[MAX_PATCH_PATH * 4];
    if (!target || !target[0] || !rel || !make_abs(ws, abs_ws, sizeof(abs_ws)) ||
        !make_abs(target, abs_t, sizeof(abs_t)))
        return 0;
    size_t n = strlen(abs_ws);
    while (n > 1 && (abs_ws[n - 1] == '/' || abs_ws[n - 1] == '\\'))
        n--;
    for (size_t i = 0; i < n; i++)
        if (norm_ch(abs_ws[i]) != norm_ch(abs_t[i]))
            return 0;
    if (abs_t[n] != '/' && abs_t[n] != '\\')
        return 0;
    const char *t = abs_t + n + 1;
    while (t[0] == '.' && (t[1] == '/' || t[1] == '\\'))
        t += 2;
    if (!t[0] || strlen(t) >= rel_size)
        return 0;
    snprintf(rel, rel_size, "%s", t);
    for (char *q = rel; *q; q++)
        if (*q == '\\') *q = '/'; /* Fs* accepts '/' only */
    return 1;
}

static bool resolve_target(const PATCH_PLAN *plan, char *rel, size_t rel_size)
{
    return PatchResolveTarget(plan->workspace, plan->target_file, rel, rel_size) != 0;
}

static FS_READ_ROOT *open_root(const PATCH_PLAN *plan)
{
    FS_READ_ROOT *r = NULL;
    if (FsReadOpen(plan->workspace[0] ? plan->workspace : ".", &r) != FS_READ_OK)
        return NULL;
    return r;
}

/* Read the target as a NUL-terminated buffer. */
static char *read_target(const PATCH_PLAN *plan, size_t *out_size, char *diag)
{
    char rel[MAX_PATCH_PATH];
    FS_READ_ROOT *r;
    unsigned char *bytes = NULL;
    size_t n = 0;
    FS_READ_META meta;
    FS_READ_STATUS s;

    if (!resolve_target(plan, rel, sizeof(rel)))
    {
        set_diag(diag, "target is outside the workspace");
        return NULL;
    }
    r = open_root(plan);
    if (!r)
    {
        set_diag(diag, "cannot open workspace root");
        return NULL;
    }
    s = FsReadFile(r, rel, &bytes, &n, &meta);
    FsReadClose(r);
    if (s != FS_READ_OK)
    {
        set_diag(diag, fs_status_text(s));
        return NULL;
    }
    char *buf = (char *)malloc(n + 1);
    if (!buf)
    {
        free(bytes);
        set_diag(diag, "out of memory");
        return NULL;
    }
    if (n)
        memcpy(buf, bytes, n);
    buf[n] = '\0';
    free(bytes);
    if (out_size)
        *out_size = n;
    return buf;
}

/* Replace the target's exact `expected` bytes with `repl`. Returns true only
   when the target verifiably holds `repl` afterwards. FS_READ_PENDING is never
   treated as success: recovery runs, then the on-disk bytes decide. */
static bool replace_target(const PATCH_PLAN *plan, const char *expected, size_t elen,
                           const char *repl, size_t rlen, char *diag)
{
    char rel[MAX_PATCH_PATH];
    FS_READ_ROOT *r;
    FS_READ_STATUS s;
    bool ok = false;

    if (elen > PATCH_IO_MAX || rlen > PATCH_IO_MAX)
    {
        set_diag(diag, "file exceeds the 1 MiB Fs* limit");
        return false;
    }
    if (!resolve_target(plan, rel, sizeof(rel)))
    {
        set_diag(diag, "target is outside the workspace");
        return false;
    }
    r = open_root(plan);
    if (!r)
    {
        set_diag(diag, "cannot open workspace root");
        return false;
    }
#ifdef AGENT_PATCH_TEST_HOOK
    if (g_patch_test_before_replace)
        g_patch_test_before_replace();
#endif
    s = FsReplaceFile(r, rel, expected, elen, repl, rlen);
    if (s == FS_READ_OK)
        ok = true;
    else if (s == FS_READ_PENDING)
    {
        FS_READ_STATUS rs = FsReplaceRecover(r);
        unsigned char *now = NULL;
        size_t nn = 0;
        FS_READ_META meta;
        if (rs == FS_READ_OK && FsReadFile(r, rel, &now, &nn, &meta) == FS_READ_OK &&
            nn == rlen && (rlen == 0 || memcmp(now, repl, rlen) == 0))
            ok = true;
        else
            set_diag(diag, rs == FS_READ_OK
                     ? "replace was rolled back by recovery"
                     : "replace pending and recovery failed");
        free(now);
    }
    else
        set_diag(diag, fs_status_text(s));
    FsReadClose(r);
    return ok;
}

/* ============================================================
   Lifecycle API
   ============================================================ */

int PatchPlanInit(PATCH_PLAN *plan, const char *target_file)
{
    if (!plan || !target_file)
        return 0;

    memset(plan, 0, sizeof(*plan));
    strncpy(plan->target_file, target_file, MAX_PATCH_PATH - 1);
    return 1;
}

int PatchPlanSetWorkspace(PATCH_PLAN *plan, const char *workspace_dir)
{
    if (!plan || !workspace_dir || !workspace_dir[0] ||
        strlen(workspace_dir) >= MAX_PATCH_PATH)
        return 0;
    strcpy(plan->workspace, workspace_dir);
    return 1;
}

int PatchRecover(const PATCH_PLAN *plan)
{
    if (!plan)
        return 0;
    FS_READ_ROOT *r = open_root(plan);
    if (!r)
        return 0;
    FS_READ_STATUS s = FsReplaceRecover(r);
    FsReadClose(r);
    return s == FS_READ_OK;
}

void PatchPlanFree(PATCH_PLAN *plan)
{
    if (!plan)
        return;

    if (plan->backup_content)
    {
        free(plan->backup_content);
        plan->backup_content = NULL;
    }
    plan->backup_size = 0;
    if (plan->applied_content)
    {
        free(plan->applied_content);
        plan->applied_content = NULL;
    }
    plan->applied_size = 0;
    plan->is_applied = false;
    plan->hunk_count = 0;
}

/* ============================================================
   Hunk Formulation API
   ============================================================ */

int PatchPlanAddHunk(PATCH_PLAN *plan,
                      uint32_t expected_line,
                      const char *context_before,
                      const char *target_content,
                      const char *replacement,
                      const char *context_after)
{
    if (!plan || !target_content || target_content[0] == '\0')
        return 0;

    if (plan->hunk_count >= MAX_PATCH_HUNKS)
        return 0;

    PATCH_HUNK *hunk = &plan->hunks[plan->hunk_count];
    memset(hunk, 0, sizeof(*hunk));

    hunk->expected_line = expected_line;

    if (context_before)
        strncpy(hunk->context_before, context_before, MAX_HUNK_TEXT - 1);

    strncpy(hunk->target_content, target_content, MAX_HUNK_TEXT - 1);

    if (replacement)
        strncpy(hunk->replacement, replacement, MAX_HUNK_TEXT - 1);

    if (context_after)
        strncpy(hunk->context_after, context_after, MAX_HUNK_TEXT - 1);

    plan->hunk_count++;
    return 1;
}

/* ============================================================
   Pre-Flight Verification API
   ============================================================ */

int PatchVerifyAgainstBuffer(const PATCH_PLAN *plan,
                              const char *source_buffer,
                              PATCH_VERIFY_REPORT *report)
{
    if (!plan || !source_buffer || !report)
        return 0;

    memset(report, 0, sizeof(*report));

    if (plan->hunk_count == 0)
    {
        report->status = PATCH_CHECK_NOT_FOUND;
        report->is_applicable = false;
        snprintf(report->diagnostic, sizeof(report->diagnostic),
                 "Patch plan contains zero hunks.");
        return 0;
    }

    /* Normalize buffer to LF for cross-platform CRLF/LF resilience */
    char *norm_source = strip_cr(source_buffer);
    if (!norm_source)
    {
        report->status = PATCH_CHECK_IO_ERROR;
        report->is_applicable = false;
        return 0;
    }

    /* Verify each hunk */
    for (uint32_t h = 0; h < plan->hunk_count; h++)
    {
        const PATCH_HUNK *hunk = &plan->hunks[h];

        char raw_needle[MAX_HUNK_TEXT * 3];
        snprintf(raw_needle, sizeof(raw_needle), "%s%s%s",
                 hunk->context_before, hunk->target_content, hunk->context_after);

        char *needle = strip_cr(raw_needle);
        if (!needle)
        {
            free(norm_source);
            report->status = PATCH_CHECK_IO_ERROR;
            report->is_applicable = false;
            return 0;
        }

        /* Count occurrences */
        size_t needle_len = strlen(needle);
        const char *p = norm_source;
        const char *first_match = NULL;
        uint32_t occurrences = 0;

        while ((p = strstr(p, needle)) != NULL)
        {
            if (occurrences == 0)
                first_match = p;

            occurrences++;
            p += needle_len;
        }

        if (h == 0)
            report->occurrences_found = occurrences;

        if (occurrences == 0)
        {
            report->status = PATCH_CHECK_NOT_FOUND;
            report->is_applicable = false;
            snprintf(report->diagnostic, sizeof(report->diagnostic),
                     "Hunk %u: Target content not found in target buffer.", h + 1);
            free(needle);
            free(norm_source);
            return 0;
        }

        if (occurrences > 1)
        {
            report->status = PATCH_CHECK_AMBIGUOUS;
            report->is_applicable = false;
            snprintf(report->diagnostic, sizeof(report->diagnostic),
                     "Hunk %u: Target content found %u times; ambiguous. Provide additional context anchors.",
                     h + 1, occurrences);
            free(needle);
            free(norm_source);
            return 0;
        }

        /* Exactly 1 occurrence located */
        size_t match_offset = (size_t)(first_match - norm_source);
        char *norm_cb = strip_cr(hunk->context_before);
        if (norm_cb)
        {
            match_offset += strlen(norm_cb);
            free(norm_cb);
        }

        uint32_t line = 1;
        for (size_t i = 0; i < match_offset; i++)
        {
            if (norm_source[i] == '\n')
                line++;
        }

        if (h == 0)
        {
            report->matched_line = line;
            if (hunk->expected_line > 0)
                report->line_drift = (int32_t)line - (int32_t)hunk->expected_line;
            else
                report->line_drift = 0;
        }

        free(needle);
    }

    report->is_applicable = true;

    if (report->line_drift != 0)
    {
        report->status = PATCH_CHECK_OFFSET_DRIFT;
        snprintf(report->diagnostic, sizeof(report->diagnostic),
                 "Target uniquely matched at line %u (offset drift: %+d from expected line %u).",
                 report->matched_line, report->line_drift, plan->hunks[0].expected_line);
    }
    else
    {
        report->status = PATCH_CHECK_OK;
        snprintf(report->diagnostic, sizeof(report->diagnostic),
                 "Target uniquely matched at line %u.", report->matched_line);
    }

    free(norm_source);
    return 1;
}

int PatchVerifyPlan(const PATCH_PLAN *plan, PATCH_VERIFY_REPORT *report)
{
    if (!plan || !report)
        return 0;

    size_t sz = 0;
    char why[MAX_PATCH_DIAG] = {0};
    char *content = read_target(plan, &sz, why);
    if (!content)
    {
        memset(report, 0, sizeof(*report));
        report->status = PATCH_CHECK_IO_ERROR;
        report->is_applicable = false;
        snprintf(report->diagnostic, sizeof(report->diagnostic),
                 "Failed to read file '%.200s': %.200s.", plan->target_file, why);
        return 0;
    }

    int rc = PatchVerifyAgainstBuffer(plan, content, report);
    free(content);
    return rc;
}

/* ============================================================
   Atomic Execution & Rollback API
   ============================================================ */

int PatchApplyAtomic(PATCH_PLAN *plan)
{
    if (!plan || plan->hunk_count == 0)
        return 0;

    plan->io_diag[0] = '\0';

    /* 1. Pre-flight verification of all hunks */
    PATCH_VERIFY_REPORT report;
    if (!PatchVerifyPlan(plan, &report) || !report.is_applicable)
    {
        set_diag(plan->io_diag, report.diagnostic);
        return 0;
    }

    /* 2. Read full original content for backup */
    size_t orig_sz = 0;
    char *orig_content = read_target(plan, &orig_sz, plan->io_diag);
    if (!orig_content)
        return 0;


    if (plan->backup_content)
        free(plan->backup_content);

    plan->backup_content = orig_content;
    plan->backup_size = orig_sz;

    /* 3. Normalize working buffer to LF for seamless multi-hunk replacement */
    char *work_buf = strip_cr(plan->backup_content);
    if (!work_buf)
        return 0;
    size_t eol_dflt = 0;
    unsigned char *work_fl = eol_flags_from(plan->backup_content, &eol_dflt);
    if (!work_fl)
    {
        free(work_buf);
        return 0;
    }

    /* Sequentially apply all hunks */
    for (uint32_t h = 0; h < plan->hunk_count; h++)
    {
        const PATCH_HUNK *hunk = &plan->hunks[h];

        char raw_needle[MAX_HUNK_TEXT * 3];
        snprintf(raw_needle, sizeof(raw_needle), "%s%s%s",
                 hunk->context_before, hunk->target_content, hunk->context_after);
        char *needle = strip_cr(raw_needle);

        char raw_repl[MAX_HUNK_TEXT * 3];
        snprintf(raw_repl, sizeof(raw_repl), "%s%s%s",
                 hunk->context_before, hunk->replacement, hunk->context_after);
        char *repl = strip_cr(raw_repl);

        if (!needle || !repl)
        {
            free(needle);
            free(repl);
            free(work_buf);
            free(work_fl);
            return 0;
        }

        const char *match = strstr(work_buf, needle);
        if (!match)
        {
            free(needle);
            free(repl);
            free(work_buf);
            free(work_fl);
            return 0;
        }

        size_t prefix_len = (size_t)(match - work_buf);
        size_t needle_len = strlen(needle);
        size_t repl_len   = strlen(repl);
        size_t work_sz    = strlen(work_buf);
        size_t suffix_len = work_sz - (prefix_len + needle_len);

        size_t new_sz = prefix_len + repl_len + suffix_len;
        char *new_content = (char *)malloc(new_sz + 1);
        if (!new_content)
        {
            free(needle);
            free(repl);
            free(work_buf);
            free(work_fl);
            return 0;
        }

        unsigned char *new_fl = (unsigned char *)calloc(new_sz + 1, 1);
        if (!new_fl)
        {
            free(new_content);
            free(needle);
            free(repl);
            free(work_buf);
            free(work_fl);
            return 0;
        }
        memcpy(new_fl, work_fl, prefix_len);
        memcpy(new_fl + prefix_len + repl_len, work_fl + prefix_len + needle_len, suffix_len);
        {
            /* k-th newline of the replacement takes the k-th newline's ending
               of the replaced region; extra ones take its last, or the file's
               first ending when the region had no newline. */
            size_t k = 0, ncnt = 0;
            unsigned char last = (unsigned char)eol_dflt;
            for (size_t i = 0; i < needle_len; i++)
                if (needle[i] == '\n') ncnt++;
            for (size_t i = 0; i < repl_len; i++)
            {
                if (repl[i] != '\n') continue;
                if (ncnt)
                {
                    size_t seen = 0, pos = 0;
                    for (size_t q = 0; q < needle_len; q++)
                        if (needle[q] == '\n')
                        {
                            pos = q;
                            if (seen++ == k) break;
                        }
                    last = work_fl[prefix_len + pos];
                }
                new_fl[prefix_len + i] = last;
                k++;
            }
        }
        memcpy(new_content, work_buf, prefix_len);
        memcpy(new_content + prefix_len, repl, repl_len);
        memcpy(new_content + prefix_len + repl_len,
               work_buf + prefix_len + needle_len, suffix_len);
        new_content[new_sz] = '\0';

        free(needle);
        free(repl);
        free(work_buf);
        free(work_fl);
        work_buf = new_content;
        work_fl = new_fl;
    }

    /* 4. Restore original line ending convention before writing to disk */
    size_t final_sz = 0;
    char *final_content = restore_eol(work_buf, work_fl, &final_sz);
    free(work_buf);
    free(work_fl);

    if (!final_content)
        return 0;

    /* 5. Conditional replace: succeeds only if the file still holds exactly
       the bytes read in step 2 (drift since then is DENIED, not overwritten). */
    if (!replace_target(plan, plan->backup_content, plan->backup_size,
                        final_content, final_sz, plan->io_diag))
    {
        free(final_content);
        return 0;
    }

    free(plan->applied_content);
    plan->applied_content = final_content;
    plan->applied_size = final_sz;
    plan->is_applied = true;
    return 1;
}

int PatchRollback(PATCH_PLAN *plan)
{
    if (!plan || !plan->is_applied || !plan->backup_content)
        return 0;

    plan->io_diag[0] = '\0';
    if (!plan->applied_content)
        return 0;

    /* Restore only if the file still holds exactly what this plan wrote. */
    if (!replace_target(plan, plan->applied_content, plan->applied_size,
                        plan->backup_content, plan->backup_size, plan->io_diag))
        return 0;

    plan->is_applied = false;
    return 1;
}

/* ============================================================
   Unified Diff Formatting API
   ============================================================ */

int PatchFormatUnifiedDiff(const PATCH_PLAN *plan,
                            const PATCH_VERIFY_REPORT *report,
                            char *out_diff,
                            size_t max_size)
{
    if (!plan || !out_diff || max_size == 0)
        return 0;

    if (plan->hunk_count == 0)
        return 0;

    int offset = snprintf(out_diff, max_size,
                          "--- a/%s\n"
                          "+++ b/%s\n",
                          plan->target_file, plan->target_file);

    if (offset < 0 || (size_t)offset >= max_size)
        return 0;

    for (uint32_t h = 0; h < plan->hunk_count && (size_t)offset < max_size; h++)
    {
        const PATCH_HUNK *hunk = &plan->hunks[h];
        uint32_t match_line = (report && report->matched_line > 0 && h == 0) ?
                              report->matched_line : hunk->expected_line;
        if (match_line == 0)
            match_line = 1;

        uint32_t old_lines = count_lines(hunk->target_content);
        uint32_t new_lines = count_lines(hunk->replacement);

        int written = snprintf(out_diff + offset, max_size - (size_t)offset,
                               "@@ -%u,%u +%u,%u @@\n",
                               match_line, old_lines,
                               match_line, new_lines);
        if (written > 0 && (size_t)(offset + written) < max_size)
            offset += written;

        /* Format target lines with '-' */
        char *norm_target = strip_cr(hunk->target_content);
        const char *p = norm_target ? norm_target : hunk->target_content;
        while (*p && (size_t)offset < max_size)
        {
            const char *nl = strchr(p, '\n');
            size_t line_len = nl ? (size_t)(nl - p) : strlen(p);

            written = snprintf(out_diff + offset, max_size - (size_t)offset,
                               "-%.*s\n", (int)line_len, p);
            if (written > 0 && (size_t)(offset + written) < max_size)
                offset += written;

            if (nl)
                p = nl + 1;
            else
                break;
        }
        if (norm_target)
            free(norm_target);

        /* Format replacement lines with '+' */
        char *norm_repl = strip_cr(hunk->replacement);
        p = norm_repl ? norm_repl : hunk->replacement;
        while (*p && (size_t)offset < max_size)
        {
            const char *nl = strchr(p, '\n');
            size_t line_len = nl ? (size_t)(nl - p) : strlen(p);

            written = snprintf(out_diff + offset, max_size - (size_t)offset,
                               "+%.*s\n", (int)line_len, p);
            if (written > 0 && (size_t)(offset + written) < max_size)
                offset += written;

            if (nl)
                p = nl + 1;
            else
                break;
        }
        if (norm_repl)
            free(norm_repl);
    }

    return 1;
}
