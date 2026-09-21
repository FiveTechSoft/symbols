/* ============================================================
   agent_diagnose.c: Compiler & Linter Error Abductive Engine.
   Pure ISO C11, zero tensors, zero backprop, fail-closed verification.
   ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "agent_diagnose.h"

/* Quote pairs recognized around identifiers in compiler diagnostics.
   GCC and Clang wrap identifiers in ASCII quotes under the C locale and in
   UTF-8 curly quotes under UTF-8 locales (e.g. LC_ALL=C.UTF-8 on CI). */
typedef struct
{
    const char *open_seq;  /* Opening quote byte sequence */
    const char *close_seq; /* Matching closing quote byte sequence */
    size_t seq_len;        /* Byte length of each sequence */
} QUOTE_PAIR;

static const QUOTE_PAIR k_QuotePairs[] = {
    { "'",            "'",            1 }, /* ASCII single quote ' */
    { "\"",           "\"",           1 }, /* ASCII double quote " */
    { "\xE2\x80\x98", "\xE2\x80\x99", 3 }, /* U+2018/U+2019 curly single quotes */
    { "\xE2\x80\x9C", "\xE2\x80\x9D", 3 }, /* U+201C/U+201D curly double quotes */
};

/* Helper: extract a quoted identifier e.g. 'foo', "foo" or the UTF-8 curly
   quote forms above. Fail-closed: without a matching closing quote of the
   same pair, out stays empty and 0 is returned. */
static int ExtractQuotedIdentifier(const char *msg, const char *after_prefix, char *out, size_t out_size)
{
    if (!msg || !out || out_size == 0) return 0;
    out[0] = '\0';

    const char *start = msg;
    if (after_prefix)
    {
        start = strstr(msg, after_prefix);
        if (!start) return 0;
        start += strlen(after_prefix);
    }

    /* Find the earliest opening quote among the supported pairs */
    const char *q_start = NULL;
    const QUOTE_PAIR *pair = NULL;
    size_t i;
    for (i = 0; i < sizeof(k_QuotePairs) / sizeof(k_QuotePairs[0]); i++)
    {
        const char *q = strstr(start, k_QuotePairs[i].open_seq);
        if (q && (!q_start || q < q_start))
        {
            q_start = q;
            pair = &k_QuotePairs[i];
        }
    }

    if (!q_start || !pair) return 0;
    q_start += pair->seq_len; /* Move past opening quote */

    const char *q_end = strstr(q_start, pair->close_seq);
    if (!q_end) return 0;

    size_t len = (size_t)(q_end - q_start);
    if (len >= out_size) len = out_size - 1;
    strncpy(out, q_start, len);
    out[len] = '\0';
    return 1;
}

/* Helper: extract 'did you mean X?' */
static int ExtractDidYouMean(const char *msg, char *out, size_t out_size)
{
    const char *dym = strstr(msg, "did you mean");
    if (!dym) return 0;
    return ExtractQuotedIdentifier(dym, "did you mean", out, out_size);
}

/* Parse a single line from compiler stderr */
static bool ParseCompilerLine(const char *line, DIAGNOSTIC_ITEM *item)
{
    if (!line || !item) return false;
    memset(item, 0, sizeof(*item));

    const char *open_paren = strchr(line, '(');
    const char *close_paren = open_paren ? strchr(open_paren, ')') : NULL;
    const char *p = NULL;

    /* Check for MSVC format: path(line): error or path(line,col): error */
    if (open_paren && close_paren && close_paren > open_paren + 1 && *(close_paren + 1) == ':')
    {
        size_t path_len = (size_t)(open_paren - line);
        if (path_len >= sizeof(item->file)) path_len = sizeof(item->file) - 1;
        strncpy(item->file, line, path_len);
        item->file[path_len] = '\0';

        p = open_paren + 1;
        item->line = (uint32_t)strtoul(p, (char **)&p, 10);
        if (*p == ',')
        {
            p++;
            item->col = (uint32_t)strtoul(p, (char **)&p, 10);
        }
        p = close_paren + 2; /* Skip ): */
        while (*p == ' ' || *p == '\t') p++;

        if (strstr(p, "error") != NULL)
            item->is_warning = false;
        else if (strstr(p, "warning") != NULL)
            item->is_warning = true;
        else
            return false;

        const char *colon = strchr(p, ':');
        if (colon) p = colon + 1;
    }
    else
    {
        /* Check for GCC/Clang format: path:line:col: [error|warning|fatal error]: msg */
        const char *first_colon = strchr(line, ':');
        if (!first_colon) return false;

        /* Check if drive letter on Windows (e.g. C:\path\file.c:10:5:) */
        if (first_colon == line + 1 && isalpha((unsigned char)line[0]))
        {
            first_colon = strchr(first_colon + 1, ':');
            if (!first_colon) return false;
        }

        /* Extract file path */
        size_t path_len = (size_t)(first_colon - line);
        if (path_len >= sizeof(item->file)) path_len = sizeof(item->file) - 1;
        strncpy(item->file, line, path_len);
        item->file[path_len] = '\0';

        /* Parse line number */
        p = first_colon + 1;
        item->line = (uint32_t)strtoul(p, (char **)&p, 10);
        if (item->line == 0 || *p != ':') return false;
        p++; /* Skip ':' */

        /* Parse column number (optional in some formats) */
        if (isdigit((unsigned char)*p))
        {
            item->col = (uint32_t)strtoul(p, (char **)&p, 10);
            if (*p == ':') p++;
        }

        while (*p == ' ' || *p == '\t') p++;

        /* Check severity */
        if (strncmp(p, "error:", 6) == 0)
        {
            item->is_warning = false;
            p += 6;
        }
        else if (strncmp(p, "fatal error:", 12) == 0)
        {
            item->is_warning = false;
            p += 12;
        }
        else if (strncmp(p, "warning:", 8) == 0)
        {
            item->is_warning = true;
            p += 8;
        }
        else
        {
            return false;
        }
    }

    while (*p == ' ' || *p == '\t') p++;
    strncpy(item->raw_message, p, sizeof(item->raw_message) - 1);

    /* Classify error type and extract causal symbols */
    if (strstr(p, "has no member named") != NULL)
    {
        item->type = DIAG_ERR_MISSING_MEMBER;
        ExtractQuotedIdentifier(p, "has no member named", item->offending_symbol, sizeof(item->offending_symbol));
        ExtractDidYouMean(p, item->suggested_fix, sizeof(item->suggested_fix));
    }
    else if (strstr(p, "undeclared") != NULL || strstr(p, "undefined reference") != NULL)
    {
        item->type = DIAG_ERR_UNDECLARED_SYMBOL;
        ExtractQuotedIdentifier(p, NULL, item->offending_symbol, sizeof(item->offending_symbol));
        ExtractDidYouMean(p, item->suggested_fix, sizeof(item->suggested_fix));
    }
    else if (strstr(p, "too few arguments") != NULL || strstr(p, "too many arguments") != NULL)
    {
        item->type = DIAG_ERR_ARITY_MISMATCH;
        ExtractQuotedIdentifier(p, "to function", item->offending_symbol, sizeof(item->offending_symbol));
        
        const char *exp = strstr(p, "expected ");
        if (exp) item->expected_arity = (uint32_t)atoi(exp + 9);
        const char *hav = strstr(p, "have ");
        if (hav) item->actual_arity = (uint32_t)atoi(hav + 5);
    }
    else if (strstr(p, "incompatible type") != NULL || strstr(p, "incompatible pointer") != NULL)
    {
        item->type = DIAG_ERR_TYPE_MISMATCH;
        ExtractQuotedIdentifier(p, NULL, item->offending_symbol, sizeof(item->offending_symbol));
    }
    else if (strstr(p, "No such file or directory") != NULL || strstr(p, "cannot open include") != NULL)
    {
        item->type = DIAG_ERR_MISSING_HEADER;
        ExtractQuotedIdentifier(p, NULL, item->offending_symbol, sizeof(item->offending_symbol));
        if (item->offending_symbol[0] == '\0')
        {
            /* Check if header preceded colon e.g. fatal error: foo.h: No such file */
            const char *h_end = strstr(p, ": No such file");
            if (h_end)
            {
                const char *h_start = h_end;
                while (h_start > p && *(h_start - 1) != ' ') h_start--;
                size_t h_len = (size_t)(h_end - h_start);
                if (h_len < sizeof(item->offending_symbol))
                {
                    strncpy(item->offending_symbol, h_start, h_len);
                    item->offending_symbol[h_len] = '\0';
                }
            }
        }
    }
    else if (strstr(p, "redefinition of") != NULL)
    {
        item->type = DIAG_ERR_REDEFINITION;
        ExtractQuotedIdentifier(p, "redefinition of", item->offending_symbol, sizeof(item->offending_symbol));
    }
    else if (strstr(p, "expected ';'") != NULL || strstr(p, "syntax error") != NULL)
    {
        item->type = DIAG_ERR_SYNTAX;
    }
    else
    {
        item->type = DIAG_ERR_UNKNOWN;
    }

    return true;
}

int DiagnosticParseOutput(const char *compiler_stderr, DIAGNOSTIC_REPORT *report)
{
    if (!compiler_stderr || !report) return 0;
    memset(report, 0, sizeof(*report));

    char line_buf[1024];
    const char *p = compiler_stderr;

    while (*p != '\0' && report->item_count < MAX_DIAGNOSTIC_ITEMS)
    {
        const char *line_start = p;
        while (*p != '\0' && *p != '\n') p++;

        size_t len = (size_t)(p - line_start);
        if (len > 0 && line_start[len - 1] == '\r') len--;

        if (len > 0 && len < sizeof(line_buf))
        {
            memcpy(line_buf, line_start, len);
            line_buf[len] = '\0';

            DIAGNOSTIC_ITEM item;
            if (ParseCompilerLine(line_buf, &item))
            {
                report->items[report->item_count++] = item;
                if (item.is_warning)
                {
                    report->warning_count++;
                }
                else
                {
                    report->error_count++;
                    /* First error becomes root-cause candidate */
                    if (report->root_file[0] == '\0')
                    {
                        strncpy(report->root_file, item.file, sizeof(report->root_file) - 1);
                        report->root_line = item.line;
                        report->root_col = item.col;
                        report->root_type = item.type;
                        strncpy(report->root_symbol, item.offending_symbol, sizeof(report->root_symbol) - 1);
                        strncpy(report->root_suggestion, item.suggested_fix, sizeof(report->root_suggestion) - 1);
                    }
                }
            }
        }
        if (*p == '\n') p++;
    }

    return (report->error_count > 0 || report->warning_count > 0) ? 1 : 0;
}

int DiagnosticAbduceRemedy(const DIAGNOSTIC_REPORT *report,
                           const CODE_GRAPH *graph,
                           char *out_remedy_summary,
                           size_t summary_size)
{
    if (!report || !out_remedy_summary || summary_size == 0) return 0;
    out_remedy_summary[0] = '\0';

    if (report->error_count == 0)
    {
        snprintf(out_remedy_summary, summary_size, "No errors detected in diagnostic report.");
        return 1;
    }

    switch (report->root_type)
    {
        case DIAG_ERR_MISSING_MEMBER:
            if (report->root_suggestion[0] != '\0')
            {
                snprintf(out_remedy_summary, summary_size,
                         "Field mismatch in '%s' at line %u: replace invalid member '%s' with '%s'.",
                         report->root_file, report->root_line,
                         report->root_symbol, report->root_suggestion);
            }
            else
            {
                snprintf(out_remedy_summary, summary_size,
                         "Invalid struct member '%s' in '%s': inspect struct declaration in graph.",
                         report->root_symbol, report->root_file);
            }
            break;

        case DIAG_ERR_UNDECLARED_SYMBOL:
            if (report->root_suggestion[0] != '\0')
            {
                snprintf(out_remedy_summary, summary_size,
                         "Typo in symbol '%s' at line %u: replace with candidate '%s'.",
                         report->root_symbol, report->root_line, report->root_suggestion);
            }
            else if (graph)
            {
                /* Search if symbol exists elsewhere in the codebase */
                const char *decl_file = CodeGraphGetFunctionFile(graph, report->root_symbol);
                if (decl_file)
                {
                    snprintf(out_remedy_summary, summary_size,
                             "Symbol '%s' defined in '%s': add '#include \"%s\"' to '%s'.",
                             report->root_symbol, decl_file, decl_file, report->root_file);
                }
                else
                {
                    snprintf(out_remedy_summary, summary_size,
                             "Undeclared symbol '%s' at line %u: implement prototype or definition.",
                             report->root_symbol, report->root_line);
                }
            }
            else
            {
                snprintf(out_remedy_summary, summary_size,
                         "Undeclared symbol '%s' at line %u: definition missing.",
                         report->root_symbol, report->root_line);
            }
            break;

        case DIAG_ERR_ARITY_MISMATCH:
            snprintf(out_remedy_summary, summary_size,
                     "Call to '%s' at line %u has arity mismatch: expected %u arguments, provided %u.",
                     report->root_symbol, report->root_line,
                     report->items[0].expected_arity,
                     report->items[0].actual_arity);
            break;

        case DIAG_ERR_MISSING_HEADER:
            snprintf(out_remedy_summary, summary_size,
                     "Missing include '%s' in '%s': verify include path in build configuration.",
                     report->root_symbol, report->root_file);
            break;

        case DIAG_ERR_TYPE_MISMATCH:
            snprintf(out_remedy_summary, summary_size,
                     "Type mismatch at '%s':%u: insert appropriate cast or adjust variable signature.",
                     report->root_file, report->root_line);
            break;

        case DIAG_ERR_SYNTAX:
            snprintf(out_remedy_summary, summary_size,
                     "Syntax error at '%s':%u: check missing semicolon or unbalanced delimiter.",
                     report->root_file, report->root_line);
            break;

        default:
            snprintf(out_remedy_summary, summary_size,
                     "Compilation failure at '%s':%u: inspect raw message '%s'.",
                     report->root_file, report->root_line, report->items[0].raw_message);
            break;
    }

    return 1;
}

uint32_t DiagnosticToStripsPredicates(const DIAGNOSTIC_REPORT *report)
{
    if (!report || report->error_count == 0)
        return PRED_BUILD_VERIFIED;

    /* Retract build verified, assert error diagnosed */
    return PRED_ERROR_DIAGNOSED;
}

int DiagnosticFormatReport(const DIAGNOSTIC_REPORT *report,
                           char *out_markdown,
                           size_t markdown_size)
{
    if (!report || !out_markdown || markdown_size == 0) return 0;
    out_markdown[0] = '\0';

    char remedy[512];
    DiagnosticAbduceRemedy(report, NULL, remedy, sizeof(remedy));

    int w = snprintf(out_markdown, markdown_size,
                     "### Automated Diagnostic & Abductive Analysis\n\n"
                     "- **Status**: %u error(s), %u warning(s)\n"
                     "- **Root Cause File**: `%s` (line %u, col %u)\n"
                     "- **Classification**: `%s`\n"
                     "- **Abduced Remedy**: %s\n",
                     report->error_count, report->warning_count,
                     report->root_file[0] ? report->root_file : "unknown",
                     report->root_line, report->root_col,
                     report->root_type == DIAG_ERR_MISSING_MEMBER ? "MISSING_MEMBER" :
                     report->root_type == DIAG_ERR_UNDECLARED_SYMBOL ? "UNDECLARED_SYMBOL" :
                     report->root_type == DIAG_ERR_ARITY_MISMATCH ? "ARITY_MISMATCH" :
                     report->root_type == DIAG_ERR_TYPE_MISMATCH ? "TYPE_MISMATCH" :
                     report->root_type == DIAG_ERR_MISSING_HEADER ? "MISSING_HEADER" :
                     report->root_type == DIAG_ERR_SYNTAX ? "SYNTAX_ERROR" : "GENERAL_BUILD_ERROR",
                     remedy);

    return (w > 0 && (size_t)w < markdown_size) ? 1 : 0;
}
