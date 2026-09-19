/* ============================================================
   code_graph.h: The Code Knowledge Graph (AST, Calls, Types,
                 Dependencies & Impact Analysis / Blast Radius).
   Pure ISO C11, zero tensors, zero backprop, fail-closed verification.

   Cognitive Capabilities:
     1. SYMBOL EXTRACTION: Ingests C source files and headers,
        extracting functions, structs, fields, call graphs, and
        inclusion dependencies into a high-performance relational graph.
     2. BIDIRECTIONAL TRAVERSAL: O(1) indexed lookups for forward
        callees, reverse callers, struct members, and module dependencies.
     3. IMPACT ANALYSIS (BLAST RADIUS): Transitive closure expansion
        computing affected callers, enclosing files, and regression
        risk before an agent makes surgical code modifications.
     4. AGENTIC INTEGRATION: Formats structured impact diagnostics
        for OpenCode tool planning and working memory integration.
   ============================================================ */

#ifndef CODE_GRAPH_H
#define CODE_GRAPH_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "graph.h"

#define MAX_CODE_NAME     128
#define MAX_CODE_PATH     260
#define MAX_BLAST_ENTRIES 256

/* Kind of symbol represented in the code knowledge graph */
typedef enum
{
    CODE_SYM_UNKNOWN = 0,
    CODE_SYM_FILE,
    CODE_SYM_FUNCTION,
    CODE_SYM_STRUCT,
    CODE_SYM_FIELD,
    CODE_SYM_HEADER,
    CODE_SYM_CLASS,
    CODE_SYM_METHOD,
    CODE_SYM_INTERFACE,
    CODE_SYM_MODULE
} CODE_SYMBOL_KIND;

/* Programming language classification for polyglot AST parsing */
typedef enum
{
    CODE_LANG_C = 0,
    CODE_LANG_PYTHON,
    CODE_LANG_TYPESCRIPT,
    CODE_LANG_JAVASCRIPT
} CODE_LANGUAGE;

/* Risk assessment levels based on blast radius */
typedef enum
{
    RISK_LOW = 0,     /* <= 2 callers, localized within 1 file */
    RISK_MEDIUM,      /* 3-6 callers or 2 files */
    RISK_HIGH         /* > 6 callers or > 2 files */
} IMPACT_RISK_LEVEL;

/* An entry in the computed blast radius */
typedef struct
{
    char             symbol_name[MAX_CODE_NAME];
    CODE_SYMBOL_KIND kind;
    uint32_t         depth;        /* 1 = direct caller/user, 2 = 2-hop, etc. */
    char             file_path[MAX_CODE_PATH];
    uint32_t         line_number;
} BLAST_RADIUS_ENTRY;

/* Blast radius calculation result */
typedef struct
{
    char                target_symbol[MAX_CODE_NAME];
    BLAST_RADIUS_ENTRY  entries[MAX_BLAST_ENTRIES];
    uint32_t            entry_count;
    uint32_t            affected_functions_count;
    uint32_t            affected_files_count;
    uint32_t            max_depth_reached;
    IMPACT_RISK_LEVEL   risk_level;
} CODE_BLAST_RADIUS;

/* The Code Knowledge Graph state */
typedef struct
{
    GRAPH     *graph;
    SYMBOL_ID  rel_defines_func;
    SYMBOL_ID  rel_defines_struct;
    SYMBOL_ID  rel_calls;
    SYMBOL_ID  rel_called_by;
    SYMBOL_ID  rel_includes;
    SYMBOL_ID  rel_included_by;
    SYMBOL_ID  rel_uses_type;
    SYMBOL_ID  rel_used_by;
    SYMBOL_ID  rel_has_field;
    SYMBOL_ID  rel_field_of;
    SYMBOL_ID  rel_in_file;
    /* Polyglot relations: Python & TypeScript/JavaScript */
    SYMBOL_ID  rel_defines_class;
    SYMBOL_ID  rel_has_method;
    SYMBOL_ID  rel_method_of;
    SYMBOL_ID  rel_inherits_from;
    SYMBOL_ID  rel_inherited_by;
    SYMBOL_ID  rel_imports;
    SYMBOL_ID  rel_imported_by;
    uint32_t   total_files;
    uint32_t   total_functions;
    uint32_t   total_structs;
    uint32_t   total_classes;
    uint32_t   total_calls;
} CODE_GRAPH;

/* ============================================================
   Lifecycle API
   ============================================================ */

/* Create a new code knowledge graph with specified initial capacity */
CODE_GRAPH *CodeGraphCreate(uint32_t symbol_capacity, uint32_t relation_capacity);

/* Destroy the code knowledge graph and free all associated memory */
void        CodeGraphDestroy(CODE_GRAPH *cg);

/* ============================================================
   Ingestion / Parsing API
   ============================================================ */

/* Detect programming language from file extension */
CODE_LANGUAGE CodeGraphDetectLanguage(const char *file_path);

/* Parse and ingest source code from an in-memory buffer (auto-detects language) */
int  CodeGraphIngestSource(CODE_GRAPH *cg, const char *file_path, const char *source_code);

/* Parse and ingest Python source code specifically */
int  CodeGraphIngestPython(CODE_GRAPH *cg, const char *file_path, const char *source_code);

/* Parse and ingest TypeScript / JavaScript source code specifically */
int  CodeGraphIngestTypeScript(CODE_GRAPH *cg, const char *file_path, const char *source_code);

/* Parse and ingest a source or header file from disk (auto-detects language) */
int  CodeGraphIngestFile(CODE_GRAPH *cg, const char *file_path);

/* Check whether a file or directory name should be ignored (e.g. .git, build, node_modules) */
bool CodeGraphShouldIgnoreName(const char *name);

/* Check whether a file path has an ingestable code extension */
bool CodeGraphIsSupportedFile(const char *path);

/* Parse and ingest an entire directory recursively (auto-filters ignored folders and detects languages) */
uint32_t CodeGraphIngestDirectory(CODE_GRAPH *cg, const char *root_dir);

/* ============================================================
   Query API
   ============================================================ */

/* Return names of all functions that directly call func_name */
uint32_t CodeGraphGetCallers(const CODE_GRAPH *cg, const char *func_name,
                            char results[][MAX_CODE_NAME], uint32_t max_results);

/* Return names of all functions directly called by func_name */
uint32_t CodeGraphGetCallees(const CODE_GRAPH *cg, const char *func_name,
                            char results[][MAX_CODE_NAME], uint32_t max_results);

/* Return names of all functions defined in file_path */
uint32_t CodeGraphGetFileFunctions(const CODE_GRAPH *cg, const char *file_path,
                                  char results[][MAX_CODE_NAME], uint32_t max_results);

/* Return names of all classes defined in file_path (Python / TypeScript) */
uint32_t CodeGraphGetClasses(const CODE_GRAPH *cg, const char *file_path,
                            char results[][MAX_CODE_NAME], uint32_t max_results);

/* Return names of all methods defined within class_name */
uint32_t CodeGraphGetClassMethods(const CODE_GRAPH *cg, const char *class_name,
                                 char results[][MAX_CODE_NAME], uint32_t max_results);

/* Return names of all modules imported by file_path */
uint32_t CodeGraphGetImports(const CODE_GRAPH *cg, const char *file_path,
                            char results[][MAX_CODE_NAME], uint32_t max_results);

/* Return the base class inherited by class_name, or NULL if none */
const char *CodeGraphGetBaseClass(const CODE_GRAPH *cg, const char *class_name);

/* Return names of all headers directly included by file_path (C/C++) */
uint32_t CodeGraphGetIncludes(const CODE_GRAPH *cg, const char *file_path,
                             char results[][MAX_CODE_NAME], uint32_t max_results);

/* Return names of all fields defined within struct_name */
uint32_t CodeGraphGetStructFields(const CODE_GRAPH *cg, const char *struct_name,
                                 char results[][MAX_CODE_NAME], uint32_t max_results);

/* Return the file path where func_name is defined, or NULL if unknown */
const char *CodeGraphGetFunctionFile(const CODE_GRAPH *cg, const char *func_name);

/* Check whether symbol exists in the code graph */
bool CodeGraphHasSymbol(const CODE_GRAPH *cg, const char *symbol_name);

/* ============================================================
   Impact Analysis / Blast Radius API
   ============================================================ */

/* Compute transitive closure of affected symbols and files up to max_depth */
int  CodeGraphComputeBlastRadius(const CODE_GRAPH *cg,
                                 const char *target_symbol,
                                 uint32_t max_depth,
                                 CODE_BLAST_RADIUS *out_radius);

/* Format a computed blast radius into a structured markdown report */
int  CodeGraphFormatBlastRadius(const CODE_GRAPH *cg,
                                const CODE_BLAST_RADIUS *radius,
                                char *buffer,
                                size_t buffer_size);

#endif /* CODE_GRAPH_H */
