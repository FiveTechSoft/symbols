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

    /* Verify first hunk */
    const PATCH_HUNK *hunk = &plan->hunks[0];

    char full_needle[MAX_HUNK_TEXT * 3];
    int w = snprintf(full_needle, sizeof(full_needle), "%s%s%s",
                     hunk->context_before, hunk->target_content, hunk->context_after);
    if (w < 0 || (size_t)w >= sizeof(full_needle))
    {
        report->status = PATCH_CHECK_NOT_FOUND;
        report->is_applicable = false;
        snprintf(report->diagnostic, sizeof(report->diagnostic),
                 "Hunk needle exceeds internal buffer capacity.");
        return 0;
    }

    /* Count occurrences */
    size_t needle_len = strlen(full_needle);
    const char *p = source_buffer;
    const char *first_match = NULL;
    uint32_t occurrences = 0;

    while ((p = strstr(p, full_needle)) != NULL)
    {
        if (occurrences == 0)
            first_match = p;

        occurrences++;
        p += needle_len;
    }

    report->occurrences_found = occurrences;

    if (occurrences == 0)
    {
        report->status = PATCH_CHECK_NOT_FOUND;
        report->is_applicable = false;
        snprintf(report->diagnostic, sizeof(report->diagnostic),
                 "Target content not found in target buffer.");
        return 0;
    }

    if (occurrences > 1)
    {
        report->status = PATCH_CHECK_AMBIGUOUS;
        report->is_applicable = false;
        snprintf(report->diagnostic, sizeof(report->diagnostic),
                 "Target content found %u times; ambiguous. Provide additional context_before/after.",
                 occurrences);
        return 0;
    }

    /* Exactly 1 occurrence located */
    size_t match_offset = (size_t)(first_match - source_buffer);
    if (hunk->context_before[0] != '\0')
        match_offset += strlen(hunk->context_before);

    uint32_t line = 1;
    for (size_t i = 0; i < match_offset; i++)
    {
        if (source_buffer[i] == '\n')
            line++;
    }

    report->matched_line = line;
    report->is_applicable = true;

    if (hunk->expected_line > 0)
        report->line_drift = (int32_t)line - (int32_t)hunk->expected_line;
    else
        report->line_drift = 0;

    if (report->line_drift != 0)
    {
        report->status = PATCH_CHECK_OFFSET_DRIFT;
        snprintf(report->diagnostic, sizeof(report->diagnostic),
                 "Target uniquely matched at line %u (offset drift: %+d from expected line %u).",
                 line, report->line_drift, hunk->expected_line);
    }
    else
    {
        report->status = PATCH_CHECK_OK;
        snprintf(report->diagnostic, sizeof(report->diagnostic),
                 "Target uniquely matched at line %u.", line);
    }

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

    /* 1. Pre-flight verification */
    PATCH_VERIFY_REPORT report;
    if (!PatchVerifyPlan(plan, &report) || !report.is_applicable)
        return 0;

    /* 2. Read full original content for backup */
    size_t orig_sz = 0;
    char *orig_content = read_file_to_string(plan->target_file, &orig_sz);
    if (!orig_content)
        return 0;

    if (plan->backup_content)
        free(plan->backup_content);

    plan->backup_content = orig_content;
    plan->backup_size = orig_sz;

    /* 3. Apply hunk in-memory */
    const PATCH_HUNK *hunk = &plan->hunks[0];

    char needle[MAX_HUNK_TEXT * 3];
    snprintf(needle, sizeof(needle), "%s%s%s",
             hunk->context_before, hunk->target_content, hunk->context_after);

    char repl[MAX_HUNK_TEXT * 3];
    snprintf(repl, sizeof(repl), "%s%s%s",
             hunk->context_before, hunk->replacement, hunk->context_after);

    const char *match = strstr(plan->backup_content, needle);
    if (!match)
        return 0;

    size_t prefix_len = (size_t)(match - plan->backup_content);
    size_t needle_len = strlen(needle);
    size_t repl_len   = strlen(repl);
    size_t suffix_len = orig_sz - (prefix_len + needle_len);

    size_t new_sz = prefix_len + repl_len + suffix_len;
    char *new_content = (char *)malloc(new_sz + 1);
    if (!new_content)
        return 0;

    memcpy(new_content, plan->backup_content, prefix_len);
    memcpy(new_content + prefix_len, repl, repl_len);
    memcpy(new_content + prefix_len + repl_len,
           plan->backup_content + prefix_len + needle_len, suffix_len);
    new_content[new_sz] = '\0';

    /* 4. Write new content to disk */
    if (!write_string_to_file(plan->target_file, new_content, new_sz))
    {
        free(new_content);
        return 0;
    }

    free(new_content);
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

    const PATCH_HUNK *hunk = &plan->hunks[0];
    uint32_t match_line = (report && report->matched_line > 0) ?
                          report->matched_line : hunk->expected_line;
    if (match_line == 0)
        match_line = 1;

    uint32_t old_lines = count_lines(hunk->target_content);
    uint32_t new_lines = count_lines(hunk->replacement);

    int offset = snprintf(out_diff, max_size,
                          "--- a/%s\n"
                          "+++ b/%s\n"
                          "@@ -%u,%u +%u,%u @@\n",
                          plan->target_file, plan->target_file,
                          match_line, old_lines,
                          match_line, new_lines);

    if (offset < 0 || (size_t)offset >= max_size)
        return 0;

    /* Format target lines with '-' */
    const char *p = hunk->target_content;
    while (*p && (size_t)offset < max_size)
    {
        const char *nl = strchr(p, '\n');
        size_t line_len = nl ? (size_t)(nl - p) : strlen(p);

        int written = snprintf(out_diff + offset, max_size - (size_t)offset,
                               "-%.*s\n", (int)line_len, p);
        if (written > 0 && (size_t)(offset + written) < max_size)
            offset += written;

        if (nl)
            p = nl + 1;
        else
            break;
    }

    /* Format replacement lines with '+' */
    p = hunk->replacement;
    while (*p && (size_t)offset < max_size)
    {
        const char *nl = strchr(p, '\n');
        size_t line_len = nl ? (size_t)(nl - p) : strlen(p);

        int written = snprintf(out_diff + offset, max_size - (size_t)offset,
                               "+%.*s\n", (int)line_len, p);
        if (written > 0 && (size_t)(offset + written) < max_size)
            offset += written;

        if (nl)
            p = nl + 1;
        else
            break;
    }

    return 1;
}
