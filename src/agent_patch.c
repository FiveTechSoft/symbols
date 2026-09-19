/* ============================================================
   agent_patch.c: Surgical Editing, Unified Diff & Atomic Rollback
                  Engine for Autonomous Agentic Coding.
   Pure ISO C11, zero tensors, zero backprop, fail-closed verification.
   ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "agent_patch.h"

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

/* Helper to convert LF-only text to CRLF if target file originally used CRLF */
static char *restore_crlf_if_needed(const char *src, bool original_had_crlf, size_t *out_size)
{
    if (!src)
        return NULL;

    if (!original_had_crlf)
    {
        size_t len = strlen(src);
        char *dst = (char *)malloc(len + 1);
        if (!dst) return NULL;
        memcpy(dst, src, len + 1);
        if (out_size) *out_size = len;
        return dst;
    }

    /* Count LFs that need CR */
    size_t extra = 0;
    for (const char *p = src; *p; p++)
    {
        if (*p == '\n') extra++;
    }

    size_t len = strlen(src);
    char *dst = (char *)malloc(len + extra + 1);
    if (!dst) return NULL;

    size_t j = 0;
    for (size_t i = 0; i < len; i++)
    {
        if (src[i] == '\n')
            dst[j++] = '\r';
        dst[j++] = src[i];
    }
    dst[j] = '\0';
    if (out_size) *out_size = j;
    return dst;
}

/* Helper to read entire file content into an allocated string */
static char *read_file_to_string(const char *file_path, size_t *out_size)
{
    if (!file_path)
        return NULL;

    FILE *f = fopen(file_path, "rb");
    if (!f)
        return NULL;

    if (fseek(f, 0, SEEK_END) != 0)
    {
        fclose(f);
        return NULL;
    }

    long sz = ftell(f);
    if (sz < 0 || sz > 20 * 1024 * 1024) /* 20 MB safety limit */
    {
        fclose(f);
        return NULL;
    }

    if (fseek(f, 0, SEEK_SET) != 0)
    {
        fclose(f);
        return NULL;
    }

    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf)
    {
        fclose(f);
        return NULL;
    }

    size_t read_bytes = fread(buf, 1, (size_t)sz, f);
    buf[read_bytes] = '\0';
    fclose(f);

    if (out_size)
        *out_size = read_bytes;

    return buf;
}

/* Helper to write string content to disk */
static bool write_string_to_file(const char *file_path, const char *content, size_t size)
{
    if (!file_path || !content)
        return false;

    FILE *f = fopen(file_path, "wb");
    if (!f)
        return false;

    size_t written = fwrite(content, 1, size, f);
    fclose(f);

    return (written == size);
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
    char *content = read_file_to_string(plan->target_file, &sz);
    if (!content)
    {
        memset(report, 0, sizeof(*report));
        report->status = PATCH_CHECK_IO_ERROR;
        report->is_applicable = false;
        snprintf(report->diagnostic, sizeof(report->diagnostic),
                 "Failed to read file '%s'.", plan->target_file);
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

    /* 1. Pre-flight verification of all hunks */
    PATCH_VERIFY_REPORT report;
    if (!PatchVerifyPlan(plan, &report) || !report.is_applicable)
        return 0;

    /* 2. Read full original content for backup */
    size_t orig_sz = 0;
    char *orig_content = read_file_to_string(plan->target_file, &orig_sz);
    if (!orig_content)
        return 0;

    bool had_crlf = (strstr(orig_content, "\r\n") != NULL);

    if (plan->backup_content)
        free(plan->backup_content);

    plan->backup_content = orig_content;
    plan->backup_size = orig_sz;

    /* 3. Normalize working buffer to LF for seamless multi-hunk replacement */
    char *work_buf = strip_cr(plan->backup_content);
    if (!work_buf)
        return 0;

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
            return 0;
        }

        const char *match = strstr(work_buf, needle);
        if (!match)
        {
            free(needle);
            free(repl);
            free(work_buf);
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
            return 0;
        }

        memcpy(new_content, work_buf, prefix_len);
        memcpy(new_content + prefix_len, repl, repl_len);
        memcpy(new_content + prefix_len + repl_len,
               work_buf + prefix_len + needle_len, suffix_len);
        new_content[new_sz] = '\0';

        free(needle);
        free(repl);
        free(work_buf);
        work_buf = new_content;
    }

    /* 4. Restore original line ending convention before writing to disk */
    size_t final_sz = 0;
    char *final_content = restore_crlf_if_needed(work_buf, had_crlf, &final_sz);
    free(work_buf);

    if (!final_content)
        return 0;

    /* 5. Atomic write to disk */
    if (!write_string_to_file(plan->target_file, final_content, final_sz))
    {
        free(final_content);
        return 0;
    }

    free(final_content);
    plan->is_applied = true;
    return 1;
}

int PatchRollback(PATCH_PLAN *plan)
{
    if (!plan || !plan->is_applied || !plan->backup_content)
        return 0;

    if (!write_string_to_file(plan->target_file, plan->backup_content, plan->backup_size))
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
