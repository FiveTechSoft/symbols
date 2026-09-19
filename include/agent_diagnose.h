/* ============================================================
   agent_diagnose.h: Compiler & Linter Error Abductive Engine.
   Pure ISO C11, zero tensors, zero backprop, fail-closed verification.

   Parses raw compiler/linter error streams (GCC, Clang, MSVC),
   extracts file/line/col, identifies causal failure classes,
   captures compiler "did you mean" suggestions, and synthesizes
   concrete STRIPS repair objectives.
   ============================================================ */

#ifndef AGENT_DIAGNOSE_H
#define AGENT_DIAGNOSE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "code_graph.h"
#include "agent_planner.h"

#define MAX_DIAGNOSTIC_ITEMS 32
#define MAX_DIAG_PATH        256
#define MAX_DIAG_SYMBOL      64
#define MAX_DIAG_MSG         256

/* Causal classification of build/compilation errors */
typedef enum
{
    DIAG_ERR_UNKNOWN = 0,
    DIAG_ERR_UNDECLARED_SYMBOL,     /* 'foo' undeclared / undefined reference to 'foo' */
    DIAG_ERR_MISSING_MEMBER,        /* struct has no member named 'x'; did you mean 'y'? */
    DIAG_ERR_ARITY_MISMATCH,        /* too few/many arguments to function; expected X, have Y */
    DIAG_ERR_TYPE_MISMATCH,         /* incompatible types / assignment from incompatible pointer */
    DIAG_ERR_MISSING_HEADER,        /* fatal error: foo.h: No such file or directory */
    DIAG_ERR_SYNTAX,                /* expected ';' before ... / syntax error */
    DIAG_ERR_REDEFINITION           /* redefinition of 'foo' / previous definition was here */
} DIAGNOSTIC_ERROR_TYPE;

/* Single structured error/warning diagnosis */
typedef struct
{
    char                  file[MAX_DIAG_PATH];
    uint32_t              line;
    uint32_t              col;
    DIAGNOSTIC_ERROR_TYPE type;
    char                  offending_symbol[MAX_DIAG_SYMBOL];
    char                  suggested_fix[MAX_DIAG_SYMBOL];   /* e.g. from 'did you mean' */
    uint32_t              expected_arity;
    uint32_t              actual_arity;
    char                  raw_message[MAX_DIAG_MSG];
    bool                  is_warning;
} DIAGNOSTIC_ITEM;

/* Comprehensive diagnostic audit report */
typedef struct
{
    DIAGNOSTIC_ITEM items[MAX_DIAGNOSTIC_ITEMS];
    uint32_t        item_count;
    uint32_t        error_count;
    uint32_t        warning_count;
    
    /* Primary root-cause focal point */
    char            root_file[MAX_DIAG_PATH];
    uint32_t        root_line;
    uint32_t        root_col;
    DIAGNOSTIC_ERROR_TYPE root_type;
    char            root_symbol[MAX_DIAG_SYMBOL];
    char            root_suggestion[MAX_DIAG_SYMBOL];
} DIAGNOSTIC_REPORT;

/* Parse raw compiler/linter error output (GCC / Clang / MSVC format) */
int DiagnosticParseOutput(const char *compiler_stderr, DIAGNOSTIC_REPORT *report);

/* Abductive reasoner: query CodeGraph to find candidate fix if compiler didn't provide one */
int DiagnosticAbduceRemedy(const DIAGNOSTIC_REPORT *report,
                           const CODE_GRAPH *graph,
                           char *out_remedy_summary,
                           size_t summary_size);

/* Translate diagnostic into STRIPS predicate flags and patch objectives */
uint32_t DiagnosticToStripsPredicates(const DIAGNOSTIC_REPORT *report);

/* Format diagnostic summary for senior engineer PR report */
int DiagnosticFormatReport(const DIAGNOSTIC_REPORT *report,
                           char *out_markdown,
                           size_t markdown_size);

#endif /* AGENT_DIAGNOSE_H */
