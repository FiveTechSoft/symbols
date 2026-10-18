/* ============================================================
   code_graph.c: The Code Knowledge Graph (AST, Calls, Types,
                 Dependencies & Impact Analysis / Blast Radius).
   Pure ISO C11, zero tensors, zero backprop, fail-closed verification.
   ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "code_graph.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

/* Helper to check valid C identifier start character */
static inline bool is_id_start(char c)
{
    return isalpha((unsigned char)c) || c == '_';
}

/* Helper to check valid C identifier continuation character */
static inline bool is_id_char(char c)
{
    return isalnum((unsigned char)c) || c == '_';
}

/* Helper to identify C control-flow keywords */
static bool is_control_keyword(const char *name)
{
    static const char *const keywords[] = {
        "if", "else", "while", "for", "do", "switch", "case",
        "default", "return", "sizeof", "break", "continue", "goto",
        NULL
    };
    for (int i = 0; keywords[i] != NULL; i++)
    {
        if (strcmp(name, keywords[i]) == 0)
            return true;
    }
    return false;
}

/* Helper to identify standard C types and qualifiers */
static bool is_type_keyword(const char *name)
{
    static const char *const types[] = {
        "void", "int", "char", "short", "long", "float", "double",
        "signed", "unsigned", "const", "static", "extern", "inline",
        "volatile", "auto", "register", "typedef", "struct", "union",
        "enum", "size_t", "bool", "uint8_t", "uint16_t", "uint32_t",
        "uint64_t", "int8_t", "int16_t", "int32_t", "int64_t",
        NULL
    };
    for (int i = 0; types[i] != NULL; i++)
    {
        if (strcmp(name, types[i]) == 0)
            return true;
    }
    return false;
}

/* Upper-case identifiers (TEST, TEST_ASSERT_EQUAL, CJSON_PUBLIC) are macros in C practice. The text lexer
   cannot expand them, so it must not take them for functions or calls. */
static bool is_macro_name(const char *name)
{
    size_t n = strlen(name);
    bool letter = false;
    if (n < 2)
        return false;
    for (size_t i = 0; i < n; i++)
    {
        char c = name[i];
        if (c >= 'A' && c <= 'Z')
            letter = true;
        else if (!((c >= '0' && c <= '9') || c == '_'))
            return false;
    }
    return letter;
}

static void cg_record_call(CODE_GRAPH *cg, SYMBOL_ID caller, SYMBOL_ID callee, SYMBOL_ID file, uint32_t line);
static int cg_file_unchanged(CODE_GRAPH *cg, const char *file_path, const char *text);

/* ============================================================
   Lifecycle API
   ============================================================ */

CODE_GRAPH *CodeGraphCreate(uint32_t symbol_capacity, uint32_t relation_capacity)
{
    CODE_GRAPH *cg = (CODE_GRAPH *)calloc(1, sizeof(CODE_GRAPH));
    if (!cg)
        return NULL;

    cg->graph = GraphCreate(symbol_capacity > 0 ? symbol_capacity : 4096,
                            relation_capacity > 0 ? relation_capacity : 8192);
    if (!cg->graph)
    {
        free(cg);
        return NULL;
    }

    /* Seed standard relational predicates */
    cg->rel_defines_func   = GraphAddSymbol(cg->graph, "defines_func");
    cg->rel_defines_struct = GraphAddSymbol(cg->graph, "defines_struct");
    cg->rel_calls          = GraphAddSymbol(cg->graph, "calls");
    cg->rel_called_by      = GraphAddSymbol(cg->graph, "called_by");
    cg->rel_includes       = GraphAddSymbol(cg->graph, "includes");
    cg->rel_included_by    = GraphAddSymbol(cg->graph, "included_by");
    cg->rel_uses_type      = GraphAddSymbol(cg->graph, "uses_type");
    cg->rel_used_by        = GraphAddSymbol(cg->graph, "used_by");
    cg->rel_has_field      = GraphAddSymbol(cg->graph, "has_field");
    cg->rel_field_of       = GraphAddSymbol(cg->graph, "field_of");
    cg->rel_in_file        = GraphAddSymbol(cg->graph, "in_file");
    /* Polyglot relational predicates */
    cg->rel_defines_class  = GraphAddSymbol(cg->graph, "defines_class");
    cg->rel_has_method     = GraphAddSymbol(cg->graph, "has_method");
    cg->rel_method_of      = GraphAddSymbol(cg->graph, "method_of");
    cg->rel_inherits_from  = GraphAddSymbol(cg->graph, "inherits_from");
    cg->rel_inherited_by   = GraphAddSymbol(cg->graph, "inherited_by");
    cg->rel_imports        = GraphAddSymbol(cg->graph, "imports");
    cg->rel_imported_by    = GraphAddSymbol(cg->graph, "imported_by");
    cg->rel_declares_func  = GraphAddSymbol(cg->graph, "declares_func");

    return cg;
}

static void cg_aux_free(CODE_GRAPH *cg);

void CodeGraphDestroy(CODE_GRAPH *cg)
{
    if (!cg)
        return;

    if (cg->graph)
        GraphDestroy(cg->graph);

    cg_aux_free(cg);
    free(cg->sites);
    free(cg);
}

/* ============================================================
   Ingestion / Parsing Engine: C/C++
   ============================================================ */

static int CodeGraphIngestC(CODE_GRAPH *cg, const char *file_path, const char *source_code)
{
    if (!cg || !cg->graph || !file_path || !source_code)
        return 0;

    SYMBOL_ID file_sym = GraphAddSymbol(cg->graph, file_path);
    cg->total_files++;

    const char *p = source_code;
    uint32_t line_number = 1;
    int brace_depth = 0;
    bool in_struct_def = false;

    char current_func[MAX_CODE_NAME]   = {0};
    char current_struct[MAX_CODE_NAME] = {0};
    bool prev_extern = false;      /* the last token was `extern` */
    bool extern_c_str = false;     /* extern "C" seen, waiting for its '{' */
    uint32_t extern_c_open = 0;    /* extern "C" { blocks that do not count as nesting */

    while (*p)
    {
        /* 1. Newline tracking */
        if (*p == '\n')
        {
            line_number++;
            p++;
            continue;
        }

        /* 2. Whitespace skipping */
        if (isspace((unsigned char)*p))
        {
            p++;
            continue;
        }

        /* 3. Single-line comment */
        if (p[0] == '/' && p[1] == '/')
        {
            p += 2;
            while (*p && *p != '\n')
                p++;
            continue;
        }

        /* 4. Multi-line block comment */
        if (p[0] == '/' && p[1] == '*')
        {
            p += 2;
            while (*p && !(p[0] == '*' && p[1] == '/'))
            {
                if (*p == '\n')
                    line_number++;
                p++;
            }
            if (*p)
                p += 2;
            continue;
        }

        /* 5. String literal */
        if (*p == '"')
        {
            if (prev_extern && p[1] == 'C' && p[2] == '"')
                extern_c_str = true;
            prev_extern = false;
            p++;
            while (*p && *p != '"')
            {
                if (*p == '\\' && p[1])
                    p += 2;
                else
                {
                    if (*p == '\n')
                        line_number++;
                    p++;
                }
            }
            if (*p == '"')
                p++;
            continue;
        }

        /* 6. Character literal */
        if (*p == '\'')
        {
            p++;
            while (*p && *p != '\'')
            {
                if (*p == '\\' && p[1])
                    p += 2;
                else
                {
                    if (*p == '\n')
                        line_number++;
                    p++;
                }
            }
            if (*p == '\'')
                p++;
            continue;
        }

        /* 7. Preprocessor directives */
        if (*p == '#')
        {
            p++;
            while (*p && isspace((unsigned char)*p) && *p != '\n')
                p++;

            if (strncmp(p, "include", 7) == 0)
            {
                p += 7;
                while (*p && isspace((unsigned char)*p) && *p != '\n')
                    p++;

                if (*p == '<' || *p == '"')
                {
                    char delim = (*p == '<') ? '>' : '"';
                    p++;
                    const char *inc_start = p;
                    while (*p && *p != delim && *p != '\n')
                        p++;

                    size_t inc_len = (size_t)(p - inc_start);
                    if (inc_len > 0 && inc_len < MAX_CODE_PATH)
                    {
                        char header_name[MAX_CODE_PATH];
                        memcpy(header_name, inc_start, inc_len);
                        header_name[inc_len] = '\0';

                        SYMBOL_ID h_sym = GraphAddSymbol(cg->graph, header_name);
                        GraphAddRelation(cg->graph, file_sym, cg->rel_includes, h_sym);
                        GraphAddRelation(cg->graph, h_sym, cg->rel_included_by, file_sym);
                    }
                    if (*p == delim)
                        p++;
                }
            }

            /* Skip remainder of preprocessor line */
            while (*p && *p != '\n')
            {
                if (*p == '\\' && p[1] == '\n')
                {
                    line_number++;
                    p += 2;
                }
                else
                {
                    p++;
                }
            }
            continue;
        }

        /* 8. Braces handling */
        if (*p == '{')
        {
            if (extern_c_str && brace_depth == 0)
            {
                /* extern "C" { ... } is linkage, not a body: keep the declarations inside at top level */
                extern_c_open++;
                extern_c_str = false;
                p++;
                continue;
            }
            extern_c_str = false;
            brace_depth++;
            p++;
            continue;
        }

        if (*p == '}')
        {
            if (brace_depth == 0 && extern_c_open > 0)
            {
                extern_c_open--;
                p++;
                continue;
            }
            if (brace_depth > 0)
                brace_depth--;

            if (brace_depth == 0)
            {
                if (in_struct_def)
                {
                    /* Check for typedef struct { ... } StructName; */
                    const char *q = p + 1;
                    while (*q && isspace((unsigned char)*q))
                    {
                        /* lookahead only: the main loop counts these newlines when it reaches them */
                        q++;
                    }
                    if (is_id_start(*q))
                    {
                        const char *id_start = q;
                        while (is_id_char(*q))
                            q++;
                        size_t id_len = (size_t)(q - id_start);
                        if (id_len > 0 && id_len < MAX_CODE_NAME && current_struct[0] == '\0')
                        {
                            memcpy(current_struct, id_start, id_len);
                            current_struct[id_len] = '\0';
                            SYMBOL_ID s_sym = GraphAddSymbol(cg->graph, current_struct);
                            GraphAddRelation(cg->graph, file_sym, cg->rel_defines_struct, s_sym);
                            GraphAddRelation(cg->graph, s_sym, cg->rel_in_file, file_sym);
                            cg->total_structs++;
                        }
                    }
                    in_struct_def = false;
                    current_struct[0] = '\0';
                }
                current_func[0] = '\0';
            }
            p++;
            continue;
        }

        /* 9. Identifiers, functions, and structs */
        if (is_id_start(*p))
        {
            char token[MAX_CODE_NAME];
            size_t len = 0;
            while (is_id_char(*p) && len < MAX_CODE_NAME - 1)
            {
                token[len++] = *p++;
            }
            token[len] = '\0';
            prev_extern = (strcmp(token, "extern") == 0);
            if (!prev_extern)
                extern_c_str = false;

            /* Struct detection */
            if (strcmp(token, "struct") == 0)
            {
                const char *q = p;
                while (*q && isspace((unsigned char)*q))
                    q++; /* lookahead only: the main loop counts these newlines */
                if (is_id_start(*q))
                {
                    const char *tag_start = q;
                    while (is_id_char(*q))
                        q++;
                    size_t tag_len = (size_t)(q - tag_start);
                    const char *after = q;
                    while (*after && isspace((unsigned char)*after))
                        after++;

                    if (*after == '{' && tag_len < MAX_CODE_NAME)
                    {
                        memcpy(current_struct, tag_start, tag_len);
                        current_struct[tag_len] = '\0';
                        in_struct_def = true;
                        SYMBOL_ID s_sym = GraphAddSymbol(cg->graph, current_struct);
                        GraphAddRelation(cg->graph, file_sym, cg->rel_defines_struct, s_sym);
                        GraphAddRelation(cg->graph, s_sym, cg->rel_in_file, file_sym);
                        cg->total_structs++;
                    }
                }
                else if (*q == '{')
                {
                    in_struct_def = true;
                    current_struct[0] = '\0';
                }
                continue;
            }

            /* Struct field detection */
            if (in_struct_def && brace_depth == 1 && current_struct[0] != '\0')
            {
                const char *q = p;
                while (*q && isspace((unsigned char)*q))
                    q++;

                if ((*q == ';' || *q == ',' || *q == '[') && !is_type_keyword(token))
                {
                    SYMBOL_ID s_sym = GraphAddSymbol(cg->graph, current_struct);
                    SYMBOL_ID f_sym = GraphAddSymbol(cg->graph, token);
                    GraphAddRelation(cg->graph, s_sym, cg->rel_has_field, f_sym);
                    GraphAddRelation(cg->graph, f_sym, cg->rel_field_of, s_sym);
                }
            }

            /* Top-level function definition detection */
            if (brace_depth == 0 && !in_struct_def)
            {
                const char *q = p;
                while (*q && isspace((unsigned char)*q))
                    q++; /* lookahead only: the main loop counts these newlines */

                if (*q == '(' && !is_control_keyword(token) && !is_type_keyword(token) &&
                    is_macro_name(token))
                    cg->macro_names_skipped++;
                else if (*q == '(' && !is_control_keyword(token) && !is_type_keyword(token))
                {
                    /* Scan matching closing parenthesis */
                    int p_depth = 1;
                    q++;
                    while (*q && p_depth > 0)
                    {
                        if (*q == '(')
                            p_depth++;
                        else if (*q == ')')
                            p_depth--;
                        else if (*q == '"')
                        {
                            q++;
                            while (*q && *q != '"')
                            {
                                if (*q == '\\' && q[1])
                                    q += 2;
                                else
                                    q++;
                            }
                        }
                        if (*q)
                            q++;
                    }

                    /* Check what follows the argument list */
                    while (*q && (isspace((unsigned char)*q) ||
                           (*q == '/' && (q[1] == '/' || q[1] == '*'))))
                    {
                        if (*q == '/' && q[1] == '/')
                        {
                            while (*q && *q != '\n')
                                q++;
                        }
                        else if (*q == '/' && q[1] == '*')
                        {
                            q += 2;
                            while (*q && !(q[0] == '*' && q[1] == '/'))
                                q++;
                            if (*q)
                                q += 2;
                        }
                        else
                        {
                            q++;
                        }
                    }

                    /* A prototype at top level: remember which file declares the name, so a call can be
                       tied to a header it can actually see. */
                    if (*q == ';')
                    {
                        SYMBOL_ID fn_sym = GraphAddSymbol(cg->graph, token);
                        GraphAddRelation(cg->graph, file_sym, cg->rel_declares_func, fn_sym);
                    }

                    /* If '{', this is a function definition */
                    if (*q == '{')
                    {
                        strncpy(current_func, token, MAX_CODE_NAME - 1);
                        current_func[MAX_CODE_NAME - 1] = '\0';
                        SYMBOL_ID fn_sym = GraphAddSymbol(cg->graph, current_func);
                        GraphAddRelation(cg->graph, file_sym, cg->rel_defines_func, fn_sym);
                        GraphAddRelation(cg->graph, fn_sym, cg->rel_in_file, file_sym);
                        cg->total_functions++;
                    }
                }
            }

            /* Function call detection within function body */
            if (brace_depth > 0 && current_func[0] != '\0')
            {
                const char *q = p;
                while (*q && isspace((unsigned char)*q))
                    q++;

                if (*q == '(' && !is_control_keyword(token) && !is_type_keyword(token) &&
                    is_macro_name(token))
                    cg->macro_names_skipped++;
                else if (*q == '(' && !is_control_keyword(token) && !is_type_keyword(token))
                {
                    SYMBOL_ID caller_sym = GraphAddSymbol(cg->graph, current_func);
                    SYMBOL_ID callee_sym = GraphAddSymbol(cg->graph, token);
                    GraphAddRelation(cg->graph, caller_sym, cg->rel_calls, callee_sym);
                    GraphAddRelation(cg->graph, callee_sym, cg->rel_called_by, caller_sym);
                    cg->total_calls++;
                    cg_record_call(cg, caller_sym, callee_sym, file_sym, line_number);
                }
            }

            continue;
        }

        /* 10. Default advance */
        p++;
    }

    return 1;
}

/* ============================================================
   Language Detection
   ============================================================ */

CODE_LANGUAGE CodeGraphDetectLanguage(const char *file_path)
{
    if (!file_path)
        return CODE_LANG_C;

    const char *dot = strrchr(file_path, '.');
    if (!dot)
        return CODE_LANG_C;

    if (strcmp(dot, ".py") == 0 || strcmp(dot, ".pyw") == 0)
        return CODE_LANG_PYTHON;

    if (strcmp(dot, ".ts") == 0 || strcmp(dot, ".tsx") == 0)
        return CODE_LANG_TYPESCRIPT;

    if (strcmp(dot, ".js") == 0 || strcmp(dot, ".jsx") == 0 ||
        strcmp(dot, ".mjs") == 0 || strcmp(dot, ".cjs") == 0)
        return CODE_LANG_JAVASCRIPT;

    return CODE_LANG_C;
}

/* ============================================================
   Ingestion / Parsing Engine: Python
   ============================================================ */

static inline bool is_py_id_start(char c)
{
    return isalpha((unsigned char)c) || c == '_';
}

static inline bool is_py_id_char(char c)
{
    return isalnum((unsigned char)c) || c == '_';
}

static bool is_py_keyword(const char *name)
{
    static const char *const kw[] = {
        "and", "as", "assert", "async", "await", "break", "class",
        "continue", "def", "del", "elif", "else", "except", "finally",
        "for", "from", "global", "if", "import", "in", "is", "lambda",
        "nonlocal", "not", "or", "pass", "raise", "return", "try",
        "while", "with", "yield", "match", "case", "True", "False", "None",
        NULL
    };
    for (int i = 0; kw[i]; i++)
    {
        if (strcmp(name, kw[i]) == 0)
            return true;
    }
    return false;
}

int CodeGraphIngestPython(CODE_GRAPH *cg, const char *file_path, const char *source_code)
{
    if (!cg || !cg->graph || !file_path || !source_code)
        return 0;

    SYMBOL_ID file_sym = GraphAddSymbol(cg->graph, file_path);
    cg->total_files++;

    const char *p = source_code;
    uint32_t line_number = 1;

    char current_class[MAX_CODE_NAME] = {0};
    int current_class_indent = -1;
    char current_func[MAX_CODE_NAME]  = {0};
    int current_func_indent = -1;

    bool at_line_start = true;
    int current_indent = 0;

    while (*p)
    {
        /* 1. Track newlines */
        if (*p == '\n')
        {
            line_number++;
            at_line_start = true;
            p++;
            continue;
        }

        /* 2. Skip carriage returns */
        if (*p == '\r')
        {
            p++;
            continue;
        }

        /* 3. Handle line indentation at start of line */
        if (at_line_start)
        {
            current_indent = 0;
            while (*p == ' ' || *p == '\t')
            {
                if (*p == ' ') current_indent++;
                else if (*p == '\t') current_indent += 4;
                p++;
            }

            if (*p == '\n' || *p == '\r')
                continue;

            if (*p == '#')
            {
                while (*p && *p != '\n')
                    p++;
                continue;
            }

            /* Non-empty line: adjust scopes according to indentation */
            if (*p != '\0')
            {
                if (current_func[0] && current_indent <= current_func_indent)
                {
                    current_func[0] = '\0';
                    current_func_indent = -1;
                }
                if (current_class[0] && current_indent <= current_class_indent)
                {
                    current_class[0] = '\0';
                    current_class_indent = -1;
                }
            }
            at_line_start = false;
        }

        /* 4. Single-line comment */
        if (*p == '#')
        {
            while (*p && *p != '\n')
                p++;
            continue;
        }

        /* 5. Triple-quoted docstrings """ or ''' */
        if (p[0] == '"' && p[1] == '"' && p[2] == '"')
        {
            p += 3;
            while (*p && !(p[0] == '"' && p[1] == '"' && p[2] == '"'))
            {
                if (*p == '\n') line_number++;
                p++;
            }
            if (*p) p += 3;
            continue;
        }
        if (p[0] == '\'' && p[1] == '\'' && p[2] == '\'')
        {
            p += 3;
            while (*p && !(p[0] == '\'' && p[1] == '\'' && p[2] == '\''))
            {
                if (*p == '\n') line_number++;
                p++;
            }
            if (*p) p += 3;
            continue;
        }

        /* 6. Standard strings "..." or '...' */
        if (*p == '"')
        {
            p++;
            while (*p && *p != '"' && *p != '\n')
            {
                if (*p == '\\' && p[1]) p += 2;
                else p++;
            }
            if (*p == '"') p++;
            continue;
        }
        if (*p == '\'')
        {
            p++;
            while (*p && *p != '\'' && *p != '\n')
            {
                if (*p == '\\' && p[1]) p += 2;
                else p++;
            }
            if (*p == '\'') p++;
            continue;
        }

        /* 7. Whitespace inside line */
        if (isspace((unsigned char)*p))
        {
            p++;
            continue;
        }

        /* 8. Python Identifiers and Keywords */
        if (is_py_id_start(*p))
        {
            char token[MAX_CODE_NAME] = {0};
            int len = 0;
            while (is_py_id_char(*p) && len < MAX_CODE_NAME - 1)
            {
                token[len++] = *p++;
            }
            token[len] = '\0';

            /* Handle async def */
            if (strcmp(token, "async") == 0)
            {
                const char *q = p;
                while (*q && isspace((unsigned char)*q) && *q != '\n')
                    q++;
                if (strncmp(q, "def", 3) == 0 && !is_py_id_char(q[3]))
                {
                    p = q + 3;
                    strcpy(token, "def");
                }
            }

            /* Function Definition: def func_name(...) */
            if (strcmp(token, "def") == 0)
            {
                while (*p && isspace((unsigned char)*p) && *p != '\n')
                    p++;

                if (is_py_id_start(*p))
                {
                    char fn_name[MAX_CODE_NAME] = {0};
                    int fn_len = 0;
                    while (is_py_id_char(*p) && fn_len < MAX_CODE_NAME - 1)
                    {
                        fn_name[fn_len++] = *p++;
                    }
                    fn_name[fn_len] = '\0';

                    SYMBOL_ID fn_sym = GraphAddSymbol(cg->graph, fn_name);
                    GraphAddRelation(cg->graph, file_sym, cg->rel_defines_func, fn_sym);
                    GraphAddRelation(cg->graph, fn_sym, cg->rel_in_file, file_sym);
                    cg->total_functions++;

                    if (current_class[0] != '\0')
                    {
                        SYMBOL_ID cls_sym = GraphAddSymbol(cg->graph, current_class);
                        GraphAddRelation(cg->graph, cls_sym, cg->rel_has_method, fn_sym);
                        GraphAddRelation(cg->graph, fn_sym, cg->rel_method_of, cls_sym);
                    }

                    strncpy(current_func, fn_name, MAX_CODE_NAME - 1);
                    current_func[MAX_CODE_NAME - 1] = '\0';
                    current_func_indent = current_indent;
                }
                continue;
            }

            /* Class Definition: class ClassName(Base): */
            if (strcmp(token, "class") == 0)
            {
                while (*p && isspace((unsigned char)*p) && *p != '\n')
                    p++;

                if (is_py_id_start(*p))
                {
                    char cls_name[MAX_CODE_NAME] = {0};
                    int cls_len = 0;
                    while (is_py_id_char(*p) && cls_len < MAX_CODE_NAME - 1)
                    {
                        cls_name[cls_len++] = *p++;
                    }
                    cls_name[cls_len] = '\0';

                    SYMBOL_ID cls_sym = GraphAddSymbol(cg->graph, cls_name);
                    GraphAddRelation(cg->graph, file_sym, cg->rel_defines_class, cls_sym);
                    GraphAddRelation(cg->graph, cls_sym, cg->rel_in_file, file_sym);
                    cg->total_classes++;

                    /* Check for inheritance */
                    const char *q = p;
                    while (*q && isspace((unsigned char)*q) && *q != '\n')
                        q++;

                    if (*q == '(')
                    {
                        q++;
                        while (*q && isspace((unsigned char)*q) && *q != '\n')
                            q++;
                        if (is_py_id_start(*q))
                        {
                            char base_name[MAX_CODE_NAME] = {0};
                            int base_len = 0;
                            while (is_py_id_char(*q) && base_len < MAX_CODE_NAME - 1)
                            {
                                base_name[base_len++] = *q++;
                            }
                            base_name[base_len] = '\0';
                            if (base_name[0] != '\0')
                            {
                                SYMBOL_ID base_sym = GraphAddSymbol(cg->graph, base_name);
                                GraphAddRelation(cg->graph, cls_sym, cg->rel_inherits_from, base_sym);
                                GraphAddRelation(cg->graph, base_sym, cg->rel_inherited_by, cls_sym);
                            }
                        }
                    }

                    strncpy(current_class, cls_name, MAX_CODE_NAME - 1);
                    current_class[MAX_CODE_NAME - 1] = '\0';
                    current_class_indent = current_indent;
                    current_func[0] = '\0';
                    current_func_indent = -1;
                }
                continue;
            }

            /* Import statements: import os, sys */
            if (strcmp(token, "import") == 0)
            {
                while (*p && *p != '\n')
                {
                    while (*p && (isspace((unsigned char)*p) || *p == ','))
                        p++;
                    if (*p == '\n' || *p == '\0')
                        break;
                    if (is_py_id_start(*p))
                    {
                        char mod_name[MAX_CODE_NAME] = {0};
                        int mod_len = 0;
                        while ((is_py_id_char(*p) || *p == '.') && mod_len < MAX_CODE_NAME - 1)
                        {
                            mod_name[mod_len++] = *p++;
                        }
                        mod_name[mod_len] = '\0';

                        if (strcmp(mod_name, "as") == 0)
                        {
                            while (*p && isspace((unsigned char)*p)) p++;
                            while (is_py_id_char(*p)) p++;
                            continue;
                        }

                        if (mod_name[0])
                        {
                            SYMBOL_ID mod_sym = GraphAddSymbol(cg->graph, mod_name);
                            GraphAddRelation(cg->graph, file_sym, cg->rel_imports, mod_sym);
                            GraphAddRelation(cg->graph, mod_sym, cg->rel_imported_by, file_sym);
                        }
                    }
                    else
                    {
                        p++;
                    }
                }
                continue;
            }

            /* From imports: from django.db import models */
            if (strcmp(token, "from") == 0)
            {
                while (*p && isspace((unsigned char)*p) && *p != '\n')
                    p++;
                char mod_name[MAX_CODE_NAME] = {0};
                int mod_len = 0;
                while ((is_py_id_char(*p) || *p == '.') && mod_len < MAX_CODE_NAME - 1)
                {
                    mod_name[mod_len++] = *p++;
                }
                mod_name[mod_len] = '\0';

                if (mod_name[0])
                {
                    SYMBOL_ID mod_sym = GraphAddSymbol(cg->graph, mod_name);
                    GraphAddRelation(cg->graph, file_sym, cg->rel_imports, mod_sym);
                    GraphAddRelation(cg->graph, mod_sym, cg->rel_imported_by, file_sym);
                }

                while (*p && *p != '\n')
                {
                    if (strncmp(p, "import", 6) == 0 && isspace((unsigned char)p[6]))
                    {
                        p += 6;
                        while (*p && *p != '\n')
                        {
                            while (*p && (isspace((unsigned char)*p) || *p == ',' || *p == '('))
                                p++;
                            if (*p == '\n' || *p == '\0' || *p == ')')
                                break;
                            if (is_py_id_start(*p))
                            {
                                char item_name[MAX_CODE_NAME] = {0};
                                int item_len = 0;
                                while (is_py_id_char(*p) && item_len < MAX_CODE_NAME - 1)
                                {
                                    item_name[item_len++] = *p++;
                                }
                                item_name[item_len] = '\0';
                                if (strcmp(item_name, "as") == 0)
                                {
                                    while (*p && isspace((unsigned char)*p)) p++;
                                    while (is_py_id_char(*p)) p++;
                                    continue;
                                }
                                if (item_name[0])
                                {
                                    SYMBOL_ID item_sym = GraphAddSymbol(cg->graph, item_name);
                                    GraphAddRelation(cg->graph, file_sym, cg->rel_uses_type, item_sym);
                                }
                            }
                            else
                            {
                                p++;
                            }
                        }
                        break;
                    }
                    p++;
                }
                continue;
            }

            /* Function / Method Calls: check if followed by '(' */
            const char *q = p;
            while (*q && isspace((unsigned char)*q) && *q != '\n')
                q++;

            if (*q == '(' && !is_py_keyword(token))
            {
                if (current_func[0] != '\0')
                {
                    SYMBOL_ID caller_sym = GraphAddSymbol(cg->graph, current_func);
                    SYMBOL_ID callee_sym = GraphAddSymbol(cg->graph, token);
                    GraphAddRelation(cg->graph, caller_sym, cg->rel_calls, callee_sym);
                    GraphAddRelation(cg->graph, callee_sym, cg->rel_called_by, caller_sym);
                    cg->total_calls++;
                }
            }

            continue;
        }

        p++;
    }

    return 1;
}

/* ============================================================
   Ingestion / Parsing Engine: TypeScript / JavaScript
   ============================================================ */

static inline bool is_ts_id_start(char c)
{
    return isalpha((unsigned char)c) || c == '_' || c == '$';
}

static inline bool is_ts_id_char(char c)
{
    return isalnum((unsigned char)c) || c == '_' || c == '$';
}

static bool is_ts_keyword(const char *name)
{
    static const char *const kw[] = {
        "if", "else", "while", "for", "do", "switch", "case", "default",
        "return", "break", "continue", "throw", "try", "catch", "finally",
        "typeof", "instanceof", "new", "delete", "void", "in", "of",
        "function", "class", "interface", "type", "import", "export",
        "const", "let", "var", "async", "await", "yield", NULL
    };
    for (int i = 0; kw[i]; i++)
    {
        if (strcmp(name, kw[i]) == 0)
            return true;
    }
    return false;
}

int CodeGraphIngestTypeScript(CODE_GRAPH *cg, const char *file_path, const char *source_code)
{
    if (!cg || !cg->graph || !file_path || !source_code)
        return 0;

    SYMBOL_ID file_sym = GraphAddSymbol(cg->graph, file_path);
    cg->total_files++;

    const char *p = source_code;
    uint32_t line_number = 1;
    int brace_depth = 0;

    char current_class[MAX_CODE_NAME] = {0};
    int class_brace_depth = -1;
    char current_func[MAX_CODE_NAME]  = {0};
    int func_brace_depth = -1;

    while (*p)
    {
        /* 1. Newline tracking */
        if (*p == '\n')
        {
            line_number++;
            p++;
            continue;
        }

        /* 2. Whitespace skipping */
        if (isspace((unsigned char)*p))
        {
            p++;
            continue;
        }

        /* 3. Single-line comment */
        if (p[0] == '/' && p[1] == '/')
        {
            p += 2;
            while (*p && *p != '\n')
                p++;
            continue;
        }

        /* 4. Multi-line block comment */
        if (p[0] == '/' && p[1] == '*')
        {
            p += 2;
            while (*p && !(p[0] == '*' && p[1] == '/'))
            {
                if (*p == '\n')
                    line_number++;
                p++;
            }
            if (*p)
                p += 2;
            continue;
        }

        /* 5. String literals (including template literals `...`) */
        if (*p == '"' || *p == '\'' || *p == '`')
        {
            char quote = *p++;
            while (*p && *p != quote)
            {
                if (*p == '\\' && p[1])
                    p += 2;
                else
                {
                    if (*p == '\n')
                        line_number++;
                    p++;
                }
            }
            if (*p == quote)
                p++;
            continue;
        }

        /* 6. Brace tracking */
        if (*p == '{')
        {
            brace_depth++;
            p++;
            continue;
        }

        if (*p == '}')
        {
            brace_depth--;
            if (current_func[0] && brace_depth < func_brace_depth)
            {
                current_func[0] = '\0';
                func_brace_depth = -1;
            }
            if (current_class[0] && brace_depth < class_brace_depth)
            {
                current_class[0] = '\0';
                class_brace_depth = -1;
            }
            p++;
            continue;
        }

        /* 7. Identifiers and keywords */
        if (is_ts_id_start(*p))
        {
            char token[MAX_CODE_NAME] = {0};
            int len = 0;
            while (is_ts_id_char(*p) && len < MAX_CODE_NAME - 1)
            {
                token[len++] = *p++;
            }
            token[len] = '\0';

            /* Import statement: import ... from 'module' */
            if (strcmp(token, "import") == 0)
            {
                while (*p && *p != '\n' && *p != ';')
                {
                    if (strncmp(p, "from", 4) == 0 && isspace((unsigned char)p[4]))
                    {
                        p += 4;
                        while (*p && isspace((unsigned char)*p))
                            p++;
                        if (*p == '\'' || *p == '"' || *p == '`')
                        {
                            char q = *p++;
                            char mod_name[MAX_CODE_NAME] = {0};
                            int mlen = 0;
                            while (*p && *p != q && mlen < MAX_CODE_NAME - 1)
                            {
                                mod_name[mlen++] = *p++;
                            }
                            mod_name[mlen] = '\0';
                            if (*p == q) p++;

                            if (mod_name[0])
                            {
                                SYMBOL_ID mod_sym = GraphAddSymbol(cg->graph, mod_name);
                                GraphAddRelation(cg->graph, file_sym, cg->rel_imports, mod_sym);
                                GraphAddRelation(cg->graph, mod_sym, cg->rel_imported_by, file_sym);
                            }
                        }
                        break;
                    }
                    p++;
                }
                continue;
            }

            /* Require call: require('module') */
            if (strcmp(token, "require") == 0)
            {
                while (*p && isspace((unsigned char)*p)) p++;
                if (*p == '(')
                {
                    p++;
                    while (*p && isspace((unsigned char)*p)) p++;
                    if (*p == '\'' || *p == '"')
                    {
                        char q = *p++;
                        char mod_name[MAX_CODE_NAME] = {0};
                        int mlen = 0;
                        while (*p && *p != q && mlen < MAX_CODE_NAME - 1)
                        {
                            mod_name[mlen++] = *p++;
                        }
                        mod_name[mlen] = '\0';
                        if (*p == q) p++;
                        if (mod_name[0])
                        {
                            SYMBOL_ID mod_sym = GraphAddSymbol(cg->graph, mod_name);
                            GraphAddRelation(cg->graph, file_sym, cg->rel_imports, mod_sym);
                            GraphAddRelation(cg->graph, mod_sym, cg->rel_imported_by, file_sym);
                        }
                    }
                }
                continue;
            }

            /* Class definition: class ClassName extends BaseClass */
            if (strcmp(token, "class") == 0)
            {
                while (*p && isspace((unsigned char)*p) && *p != '\n') p++;
                if (is_ts_id_start(*p))
                {
                    char cls_name[MAX_CODE_NAME] = {0};
                    int clen = 0;
                    while (is_ts_id_char(*p) && clen < MAX_CODE_NAME - 1)
                    {
                        cls_name[clen++] = *p++;
                    }
                    cls_name[clen] = '\0';

                    SYMBOL_ID cls_sym = GraphAddSymbol(cg->graph, cls_name);
                    GraphAddRelation(cg->graph, file_sym, cg->rel_defines_class, cls_sym);
                    GraphAddRelation(cg->graph, cls_sym, cg->rel_in_file, file_sym);
                    cg->total_classes++;

                    strncpy(current_class, cls_name, MAX_CODE_NAME - 1);
                    class_brace_depth = brace_depth + 1;

                    /* Check extends */
                    while (*p && isspace((unsigned char)*p) && *p != '\n') p++;
                    if (strncmp(p, "extends", 7) == 0 && isspace((unsigned char)p[7]))
                    {
                        p += 7;
                        while (*p && isspace((unsigned char)*p)) p++;
                        if (is_ts_id_start(*p))
                        {
                            char base_name[MAX_CODE_NAME] = {0};
                            int blen = 0;
                            while (is_ts_id_char(*p) && blen < MAX_CODE_NAME - 1)
                            {
                                base_name[blen++] = *p++;
                            }
                            base_name[blen] = '\0';
                            if (base_name[0])
                            {
                                SYMBOL_ID base_sym = GraphAddSymbol(cg->graph, base_name);
                                GraphAddRelation(cg->graph, cls_sym, cg->rel_inherits_from, base_sym);
                                GraphAddRelation(cg->graph, base_sym, cg->rel_inherited_by, cls_sym);
                            }
                        }
                    }
                }
                continue;
            }

            /* Interface definition: interface InterfaceName */
            if (strcmp(token, "interface") == 0)
            {
                while (*p && isspace((unsigned char)*p) && *p != '\n') p++;
                if (is_ts_id_start(*p))
                {
                    char iface_name[MAX_CODE_NAME] = {0};
                    int ilen = 0;
                    while (is_ts_id_char(*p) && ilen < MAX_CODE_NAME - 1)
                    {
                        iface_name[ilen++] = *p++;
                    }
                    iface_name[ilen] = '\0';

                    SYMBOL_ID iface_sym = GraphAddSymbol(cg->graph, iface_name);
                    GraphAddRelation(cg->graph, file_sym, cg->rel_defines_struct, iface_sym);
                    GraphAddRelation(cg->graph, iface_sym, cg->rel_in_file, file_sym);
                    cg->total_structs++;
                }
                continue;
            }

            /* Function definition: function funcName(...) */
            if (strcmp(token, "function") == 0)
            {
                while (*p && isspace((unsigned char)*p) && *p != '\n') p++;
                if (is_ts_id_start(*p))
                {
                    char fn_name[MAX_CODE_NAME] = {0};
                    int fn_len = 0;
                    while (is_ts_id_char(*p) && fn_len < MAX_CODE_NAME - 1)
                    {
                        fn_name[fn_len++] = *p++;
                    }
                    fn_name[fn_len] = '\0';

                    SYMBOL_ID fn_sym = GraphAddSymbol(cg->graph, fn_name);
                    GraphAddRelation(cg->graph, file_sym, cg->rel_defines_func, fn_sym);
                    GraphAddRelation(cg->graph, fn_sym, cg->rel_in_file, file_sym);
                    cg->total_functions++;

                    strncpy(current_func, fn_name, MAX_CODE_NAME - 1);
                    func_brace_depth = brace_depth + 1;
                }
                continue;
            }

            /* Arrow function: const funcName = (...) => { */
            if (strcmp(token, "const") == 0 || strcmp(token, "let") == 0 || strcmp(token, "var") == 0)
            {
                while (*p && isspace((unsigned char)*p) && *p != '\n') p++;
                if (is_ts_id_start(*p))
                {
                    char fn_name[MAX_CODE_NAME] = {0};
                    int fn_len = 0;
                    while (is_ts_id_char(*p) && fn_len < MAX_CODE_NAME - 1)
                    {
                        fn_name[fn_len++] = *p++;
                    }
                    fn_name[fn_len] = '\0';

                    const char *q = p;
                    while (*q && isspace((unsigned char)*q) && *q != '\n') q++;
                    if (*q == '=')
                    {
                        q++;
                        while (*q && isspace((unsigned char)*q) && *q != '\n') q++;
                        bool is_func = false;
                        if (strncmp(q, "function", 8) == 0 || strncmp(q, "async", 5) == 0)
                            is_func = true;
                        else if (*q == '(' || is_ts_id_start(*q))
                        {
                            const char *arr = q;
                            while (*arr && *arr != '\n' && *arr != ';')
                            {
                                if (arr[0] == '=' && arr[1] == '>')
                                {
                                    is_func = true;
                                    break;
                                }
                                arr++;
                            }
                        }

                        if (is_func)
                        {
                            SYMBOL_ID fn_sym = GraphAddSymbol(cg->graph, fn_name);
                            GraphAddRelation(cg->graph, file_sym, cg->rel_defines_func, fn_sym);
                            GraphAddRelation(cg->graph, fn_sym, cg->rel_in_file, file_sym);
                            cg->total_functions++;

                            strncpy(current_func, fn_name, MAX_CODE_NAME - 1);
                            func_brace_depth = brace_depth + 1;
                        }
                    }
                }
                continue;
            }

            /* Class method definition: inside class, methodName(...) { */
            if (current_class[0] != '\0' && brace_depth == class_brace_depth && !is_ts_keyword(token))
            {
                const char *q = p;
                while (*q && isspace((unsigned char)*q) && *q != '\n') q++;
                if (*q == '(')
                {
                    int p_depth = 1;
                    q++;
                    while (*q && p_depth > 0)
                    {
                        if (*q == '(') p_depth++;
                        else if (*q == ')') p_depth--;
                        q++;
                    }
                    while (*q && isspace((unsigned char)*q)) q++;
                    if (*q == ':')
                    {
                        while (*q && *q != '{' && *q != ';' && *q != '\n') q++;
                    }
                    if (*q == '{')
                    {
                        SYMBOL_ID fn_sym = GraphAddSymbol(cg->graph, token);
                        SYMBOL_ID cls_sym = GraphAddSymbol(cg->graph, current_class);
                        GraphAddRelation(cg->graph, file_sym, cg->rel_defines_func, fn_sym);
                        GraphAddRelation(cg->graph, fn_sym, cg->rel_in_file, file_sym);
                        GraphAddRelation(cg->graph, cls_sym, cg->rel_has_method, fn_sym);
                        GraphAddRelation(cg->graph, fn_sym, cg->rel_method_of, cls_sym);
                        cg->total_functions++;

                        strncpy(current_func, token, MAX_CODE_NAME - 1);
                        func_brace_depth = brace_depth + 1;
                        continue;
                    }
                }
            }

            /* Function call detection: inside function, token followed by '(' */
            if (current_func[0] != '\0' && !is_ts_keyword(token))
            {
                const char *q = p;
                while (*q && isspace((unsigned char)*q) && *q != '\n') q++;
                if (*q == '(')
                {
                    SYMBOL_ID caller_sym = GraphAddSymbol(cg->graph, current_func);
                    SYMBOL_ID callee_sym = GraphAddSymbol(cg->graph, token);
                    GraphAddRelation(cg->graph, caller_sym, cg->rel_calls, callee_sym);
                    GraphAddRelation(cg->graph, callee_sym, cg->rel_called_by, caller_sym);
                    cg->total_calls++;
                }
            }

            continue;
        }

        p++;
    }

    return 1;
}

/* ============================================================
   Polyglot Ingestion Dispatcher
   ============================================================ */

int CodeGraphIngestSource(CODE_GRAPH *cg, const char *file_path, const char *source_code)
{
    if (!cg || !file_path || !source_code)
        return 0;

    /* Same file, same text as before: nothing to learn, and a repeat would double the counters. */
    if (cg_file_unchanged(cg, file_path, source_code))
    {
        cg->files_skipped_unchanged++;
        return 1;
    }

    CODE_LANGUAGE lang = CodeGraphDetectLanguage(file_path);
    if (lang == CODE_LANG_PYTHON)
        return CodeGraphIngestPython(cg, file_path, source_code);
    else if (lang == CODE_LANG_TYPESCRIPT || lang == CODE_LANG_JAVASCRIPT)
        return CodeGraphIngestTypeScript(cg, file_path, source_code);
    else
        return CodeGraphIngestC(cg, file_path, source_code);
}

int CodeGraphIngestFile(CODE_GRAPH *cg, const char *file_path)
{
    if (!cg || !file_path)
        return 0;

    FILE *f = fopen(file_path, "rb");
    if (!f)
        return 0;

    if (fseek(f, 0, SEEK_END) != 0)
    {
        fclose(f);
        return 0;
    }

    long sz = ftell(f);
    if (sz <= 0 || sz > 10 * 1024 * 1024) /* 10 MB bound */
    {
        fclose(f);
        return 0;
    }

    if (fseek(f, 0, SEEK_SET) != 0)
    {
        fclose(f);
        return 0;
    }

    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf)
    {
        fclose(f);
        return 0;
    }

    size_t read_bytes = fread(buf, 1, (size_t)sz, f);
    buf[read_bytes] = '\0';
    fclose(f);

    int rc = CodeGraphIngestSource(cg, file_path, buf);
    free(buf);
    return rc;
}

/* ============================================================
   Query API
   ============================================================ */

bool CodeGraphHasSymbol(const CODE_GRAPH *cg, const char *symbol_name)
{
    if (!cg || !cg->graph || !symbol_name)
        return false;

    SYMBOL_ID id = SymbolFind(cg->graph->symbols, symbol_name);
    return (id != SYMBOL_INVALID);
}

uint32_t CodeGraphGetCallers(const CODE_GRAPH *cg, const char *func_name,
                            char results[][MAX_CODE_NAME], uint32_t max_results)
{
    if (!cg || !cg->graph || !func_name || !results || max_results == 0)
        return 0;

    SYMBOL_ID fn_id = SymbolFind(cg->graph->symbols, func_name);
    if (fn_id == SYMBOL_INVALID)
        return 0;

    RELATION *rel_ptrs[64];
    uint32_t n = GraphQuerySubjectRelation(cg->graph, fn_id, cg->rel_called_by,
                                           rel_ptrs, 64);
    uint32_t count = 0;
    for (uint32_t i = 0; i < n && count < max_results; i++)
    {
        const SYMBOL *s = SymbolGet(cg->graph->symbols, rel_ptrs[i]->object);
        if (s && s->name)
        {
            /* Avoid duplicates */
            bool dup = false;
            for (uint32_t j = 0; j < count; j++)
            {
                if (strcmp(results[j], s->name) == 0)
                {
                    dup = true;
                    break;
                }
            }
            if (!dup)
            {
                strncpy(results[count], s->name, MAX_CODE_NAME - 1);
                results[count][MAX_CODE_NAME - 1] = '\0';
                count++;
            }
        }
    }
    return count;
}

uint32_t CodeGraphGetCallees(const CODE_GRAPH *cg, const char *func_name,
                            char results[][MAX_CODE_NAME], uint32_t max_results)
{
    if (!cg || !cg->graph || !func_name || !results || max_results == 0)
        return 0;

    SYMBOL_ID fn_id = SymbolFind(cg->graph->symbols, func_name);
    if (fn_id == SYMBOL_INVALID)
        return 0;

    RELATION *rel_ptrs[64];
    uint32_t n = GraphQuerySubjectRelation(cg->graph, fn_id, cg->rel_calls,
                                           rel_ptrs, 64);
    uint32_t count = 0;
    for (uint32_t i = 0; i < n && count < max_results; i++)
    {
        const SYMBOL *s = SymbolGet(cg->graph->symbols, rel_ptrs[i]->object);
        if (s && s->name)
        {
            bool dup = false;
            for (uint32_t j = 0; j < count; j++)
            {
                if (strcmp(results[j], s->name) == 0)
                {
                    dup = true;
                    break;
                }
            }
            if (!dup)
            {
                strncpy(results[count], s->name, MAX_CODE_NAME - 1);
                results[count][MAX_CODE_NAME - 1] = '\0';
                count++;
            }
        }
    }
    return count;
}

uint32_t CodeGraphGetFileFunctions(const CODE_GRAPH *cg, const char *file_path,
                                  char results[][MAX_CODE_NAME], uint32_t max_results)
{
    if (!cg || !cg->graph || !file_path || !results || max_results == 0)
        return 0;

    SYMBOL_ID f_id = SymbolFind(cg->graph->symbols, file_path);
    if (f_id == SYMBOL_INVALID)
        return 0;

    RELATION *rel_ptrs[128];
    uint32_t n = GraphQuerySubjectRelation(cg->graph, f_id, cg->rel_defines_func,
                                           rel_ptrs, 128);
    uint32_t count = 0;
    for (uint32_t i = 0; i < n && count < max_results; i++)
    {
        const SYMBOL *s = SymbolGet(cg->graph->symbols, rel_ptrs[i]->object);
        if (s && s->name)
        {
            strncpy(results[count], s->name, MAX_CODE_NAME - 1);
            results[count][MAX_CODE_NAME - 1] = '\0';
            count++;
        }
    }
    return count;
}

uint32_t CodeGraphGetClasses(const CODE_GRAPH *cg, const char *file_path,
                            char results[][MAX_CODE_NAME], uint32_t max_results)
{
    if (!cg || !cg->graph || !file_path || !results || max_results == 0)
        return 0;

    SYMBOL_ID f_id = SymbolFind(cg->graph->symbols, file_path);
    if (f_id == SYMBOL_INVALID)
        return 0;

    RELATION *rel_ptrs[128];
    uint32_t n = GraphQuerySubjectRelation(cg->graph, f_id, cg->rel_defines_class,
                                           rel_ptrs, 128);
    uint32_t count = 0;
    for (uint32_t i = 0; i < n && count < max_results; i++)
    {
        const SYMBOL *s = SymbolGet(cg->graph->symbols, rel_ptrs[i]->object);
        if (s && s->name)
        {
            strncpy(results[count], s->name, MAX_CODE_NAME - 1);
            results[count][MAX_CODE_NAME - 1] = '\0';
            count++;
        }
    }
    return count;
}

uint32_t CodeGraphGetClassMethods(const CODE_GRAPH *cg, const char *class_name,
                                 char results[][MAX_CODE_NAME], uint32_t max_results)
{
    if (!cg || !cg->graph || !class_name || !results || max_results == 0)
        return 0;

    SYMBOL_ID c_id = SymbolFind(cg->graph->symbols, class_name);
    if (c_id == SYMBOL_INVALID)
        return 0;

    RELATION *rel_ptrs[128];
    uint32_t n = GraphQuerySubjectRelation(cg->graph, c_id, cg->rel_has_method,
                                           rel_ptrs, 128);
    uint32_t count = 0;
    for (uint32_t i = 0; i < n && count < max_results; i++)
    {
        const SYMBOL *s = SymbolGet(cg->graph->symbols, rel_ptrs[i]->object);
        if (s && s->name)
        {
            strncpy(results[count], s->name, MAX_CODE_NAME - 1);
            results[count][MAX_CODE_NAME - 1] = '\0';
            count++;
        }
    }
    return count;
}

uint32_t CodeGraphGetImports(const CODE_GRAPH *cg, const char *file_path,
                            char results[][MAX_CODE_NAME], uint32_t max_results)
{
    if (!cg || !cg->graph || !file_path || !results || max_results == 0)
        return 0;

    SYMBOL_ID f_id = SymbolFind(cg->graph->symbols, file_path);
    if (f_id == SYMBOL_INVALID)
        return 0;

    RELATION *rel_ptrs[128];
    uint32_t n = GraphQuerySubjectRelation(cg->graph, f_id, cg->rel_imports,
                                           rel_ptrs, 128);
    uint32_t count = 0;
    for (uint32_t i = 0; i < n && count < max_results; i++)
    {
        const SYMBOL *s = SymbolGet(cg->graph->symbols, rel_ptrs[i]->object);
        if (s && s->name)
        {
            strncpy(results[count], s->name, MAX_CODE_NAME - 1);
            results[count][MAX_CODE_NAME - 1] = '\0';
            count++;
        }
    }
    return count;
}

const char *CodeGraphGetBaseClass(const CODE_GRAPH *cg, const char *class_name)
{
    if (!cg || !cg->graph || !class_name)
        return NULL;

    SYMBOL_ID c_id = SymbolFind(cg->graph->symbols, class_name);
    if (c_id == SYMBOL_INVALID)
        return NULL;

    RELATION *rel_ptrs[4];
    uint32_t n = GraphQuerySubjectRelation(cg->graph, c_id, cg->rel_inherits_from,
                                           rel_ptrs, 4);
    if (n > 0)
    {
        const SYMBOL *s = SymbolGet(cg->graph->symbols, rel_ptrs[0]->object);
        if (s)
            return s->name;
    }
    return NULL;
}

uint32_t CodeGraphGetIncludes(const CODE_GRAPH *cg, const char *file_path,
                             char results[][MAX_CODE_NAME], uint32_t max_results)
{
    if (!cg || !cg->graph || !file_path || !results || max_results == 0)
        return 0;

    SYMBOL_ID f_id = SymbolFind(cg->graph->symbols, file_path);
    if (f_id == SYMBOL_INVALID)
        return 0;

    RELATION *rel_ptrs[64];
    uint32_t n = GraphQuerySubjectRelation(cg->graph, f_id, cg->rel_includes,
                                           rel_ptrs, 64);
    uint32_t count = 0;
    for (uint32_t i = 0; i < n && count < max_results; i++)
    {
        const SYMBOL *s = SymbolGet(cg->graph->symbols, rel_ptrs[i]->object);
        if (s && s->name)
        {
            strncpy(results[count], s->name, MAX_CODE_NAME - 1);
            results[count][MAX_CODE_NAME - 1] = '\0';
            count++;
        }
    }
    return count;
}

uint32_t CodeGraphGetStructFields(const CODE_GRAPH *cg, const char *struct_name,
                                 char results[][MAX_CODE_NAME], uint32_t max_results)
{
    if (!cg || !cg->graph || !struct_name || !results || max_results == 0)
        return 0;

    SYMBOL_ID s_id = SymbolFind(cg->graph->symbols, struct_name);
    if (s_id == SYMBOL_INVALID)
        return 0;

    RELATION *rel_ptrs[64];
    uint32_t n = GraphQuerySubjectRelation(cg->graph, s_id, cg->rel_has_field,
                                           rel_ptrs, 64);
    uint32_t count = 0;
    for (uint32_t i = 0; i < n && count < max_results; i++)
    {
        const SYMBOL *s = SymbolGet(cg->graph->symbols, rel_ptrs[i]->object);
        if (s && s->name)
        {
            strncpy(results[count], s->name, MAX_CODE_NAME - 1);
            results[count][MAX_CODE_NAME - 1] = '\0';
            count++;
        }
    }
    return count;
}

const char *CodeGraphGetFunctionFile(const CODE_GRAPH *cg, const char *func_name)
{
    if (!cg || !cg->graph || !func_name)
        return NULL;

    SYMBOL_ID fn_id = SymbolFind(cg->graph->symbols, func_name);
    if (fn_id == SYMBOL_INVALID)
        return NULL;

    RELATION *rel_ptrs[4];
    uint32_t n = GraphQuerySubjectRelation(cg->graph, fn_id, cg->rel_in_file,
                                           rel_ptrs, 4);
    if (n > 0)
    {
        const SYMBOL *s = SymbolGet(cg->graph->symbols, rel_ptrs[0]->object);
        if (s)
            return s->name;
    }
    return NULL;
}

/* ============================================================
   Impact Analysis / Blast Radius Calculation
   ============================================================ */

int CodeGraphComputeBlastRadius(const CODE_GRAPH *cg,
                                const char *target_symbol,
                                uint32_t max_depth,
                                CODE_BLAST_RADIUS *out_radius)
{
    if (!cg || !cg->graph || !target_symbol || !out_radius)
        return 0;

    memset(out_radius, 0, sizeof(*out_radius));
    strncpy(out_radius->target_symbol, target_symbol, MAX_CODE_NAME - 1);

    if (max_depth == 0)
        max_depth = 3;

    SYMBOL_ID target_id = SymbolFind(cg->graph->symbols, target_symbol);
    if (target_id == SYMBOL_INVALID)
        return 0;

    /* BFS queue */
    typedef struct
    {
        SYMBOL_ID sym_id;
        uint32_t  depth;
    } BFS_NODE;

    BFS_NODE queue[MAX_BLAST_ENTRIES];
    uint32_t head = 0;
    uint32_t tail = 0;

    SYMBOL_ID visited[MAX_BLAST_ENTRIES];
    uint32_t visited_count = 0;

    queue[tail++] = (BFS_NODE){ .sym_id = target_id, .depth = 0 };
    visited[visited_count++] = target_id;

    while (head < tail)
    {
        BFS_NODE curr = queue[head++];
        if (curr.depth >= max_depth)
            continue;

        /* 1. Transitive reverse callers: (curr, called_by, Caller) */
        RELATION *caller_rels[64];
        uint32_t n_callers = GraphQuerySubjectRelation(cg->graph, curr.sym_id,
                                                       cg->rel_called_by,
                                                       caller_rels, 64);
        /* Resolved callers (EXTRACTED/INFERRED, or no recorded site) first, then the legacy name-only edges
           that no include chain backs; those entries are flagged low confidence. */
        for (uint32_t pass = 0; pass < 2; pass++)
        for (uint32_t i = 0; i < n_callers; i++)
        {
            SYMBOL_ID caller_id = caller_rels[i]->object;
            bool low_conf = false;
            {
                const SYMBOL *cs = SymbolGet(cg->graph->symbols, caller_id);
                const SYMBOL *ts = SymbolGet(cg->graph->symbols, curr.sym_id);
                CODE_EDGE_LABEL lab;
                if (cs && cs->name && ts && ts->name &&
                    CodeGraphGetEdgeLabel(cg, cs->name, ts->name, &lab) &&
                    lab != CODE_EDGE_EXTRACTED && lab != CODE_EDGE_INFERRED)
                    low_conf = true;
            }
            if ((pass == 1) != low_conf)
                continue;
            bool already_visited = false;
            for (uint32_t v = 0; v < visited_count; v++)
            {
                if (visited[v] == caller_id)
                {
                    already_visited = true;
                    break;
                }
            }

            if (!already_visited && visited_count < MAX_BLAST_ENTRIES)
            {
                visited[visited_count++] = caller_id;
                const SYMBOL *sym = SymbolGet(cg->graph->symbols, caller_id);
                if (sym && sym->name && out_radius->entry_count < MAX_BLAST_ENTRIES)
                {
                    BLAST_RADIUS_ENTRY *ent = &out_radius->entries[out_radius->entry_count++];
                    strncpy(ent->symbol_name, sym->name, MAX_CODE_NAME - 1);
                    ent->kind = CODE_SYM_FUNCTION;
                    ent->depth = curr.depth + 1;
                    ent->low_confidence = low_conf;

                    const char *f = CodeGraphGetFunctionFile(cg, sym->name);
                    if (f)
                        strncpy(ent->file_path, f, MAX_CODE_PATH - 1);

                    out_radius->affected_functions_count++;
                    if (ent->depth > out_radius->max_depth_reached)
                        out_radius->max_depth_reached = ent->depth;
                }

                if (tail < MAX_BLAST_ENTRIES)
                    queue[tail++] = (BFS_NODE){ .sym_id = caller_id, .depth = curr.depth + 1 };
            }
        }

        /* 2. Files including this header: (curr, included_by, File) */
        RELATION *inc_rels[64];
        uint32_t n_inc = GraphQuerySubjectRelation(cg->graph, curr.sym_id,
                                                   cg->rel_included_by,
                                                   inc_rels, 64);
        for (uint32_t i = 0; i < n_inc; i++)
        {
            SYMBOL_ID file_id = inc_rels[i]->object;
            bool already_visited = false;
            for (uint32_t v = 0; v < visited_count; v++)
            {
                if (visited[v] == file_id)
                {
                    already_visited = true;
                    break;
                }
            }

            if (!already_visited && visited_count < MAX_BLAST_ENTRIES)
            {
                visited[visited_count++] = file_id;
                const SYMBOL *sym = SymbolGet(cg->graph->symbols, file_id);
                if (sym && sym->name && out_radius->entry_count < MAX_BLAST_ENTRIES)
                {
                    BLAST_RADIUS_ENTRY *ent = &out_radius->entries[out_radius->entry_count++];
                    strncpy(ent->symbol_name, sym->name, MAX_CODE_NAME - 1);
                    ent->kind = CODE_SYM_FILE;
                    ent->depth = curr.depth + 1;
                    strncpy(ent->file_path, sym->name, MAX_CODE_PATH - 1);

                    if (ent->depth > out_radius->max_depth_reached)
                        out_radius->max_depth_reached = ent->depth;
                }

                if (tail < MAX_BLAST_ENTRIES)
                    queue[tail++] = (BFS_NODE){ .sym_id = file_id, .depth = curr.depth + 1 };
            }
        }

        /* 3. Functions using this type or struct: (curr, used_by, Func) */
        RELATION *used_rels[64];
        uint32_t n_used = GraphQuerySubjectRelation(cg->graph, curr.sym_id,
                                                   cg->rel_used_by,
                                                   used_rels, 64);
        for (uint32_t i = 0; i < n_used; i++)
        {
            SYMBOL_ID user_id = used_rels[i]->object;
            bool already_visited = false;
            for (uint32_t v = 0; v < visited_count; v++)
            {
                if (visited[v] == user_id)
                {
                    already_visited = true;
                    break;
                }
            }

            if (!already_visited && visited_count < MAX_BLAST_ENTRIES)
            {
                visited[visited_count++] = user_id;
                const SYMBOL *sym = SymbolGet(cg->graph->symbols, user_id);
                if (sym && sym->name && out_radius->entry_count < MAX_BLAST_ENTRIES)
                {
                    BLAST_RADIUS_ENTRY *ent = &out_radius->entries[out_radius->entry_count++];
                    strncpy(ent->symbol_name, sym->name, MAX_CODE_NAME - 1);
                    ent->kind = CODE_SYM_FUNCTION;
                    ent->depth = curr.depth + 1;

                    const char *f = CodeGraphGetFunctionFile(cg, sym->name);
                    if (f)
                        strncpy(ent->file_path, f, MAX_CODE_PATH - 1);

                    out_radius->affected_functions_count++;
                    if (ent->depth > out_radius->max_depth_reached)
                        out_radius->max_depth_reached = ent->depth;
                }

                if (tail < MAX_BLAST_ENTRIES)
                    queue[tail++] = (BFS_NODE){ .sym_id = user_id, .depth = curr.depth + 1 };
            }
        }

        /* 4. Files importing this module/symbol: (curr, imported_by, File) */
        RELATION *imp_rels[64];
        uint32_t n_imp = GraphQuerySubjectRelation(cg->graph, curr.sym_id,
                                                   cg->rel_imported_by,
                                                   imp_rels, 64);
        for (uint32_t i = 0; i < n_imp; i++)
        {
            SYMBOL_ID file_id = imp_rels[i]->object;
            bool already_visited = false;
            for (uint32_t v = 0; v < visited_count; v++)
            {
                if (visited[v] == file_id)
                {
                    already_visited = true;
                    break;
                }
            }

            if (!already_visited && visited_count < MAX_BLAST_ENTRIES)
            {
                visited[visited_count++] = file_id;
                const SYMBOL *sym = SymbolGet(cg->graph->symbols, file_id);
                if (sym && sym->name && out_radius->entry_count < MAX_BLAST_ENTRIES)
                {
                    BLAST_RADIUS_ENTRY *ent = &out_radius->entries[out_radius->entry_count++];
                    strncpy(ent->symbol_name, sym->name, MAX_CODE_NAME - 1);
                    ent->kind = CODE_SYM_FILE;
                    ent->depth = curr.depth + 1;
                    strncpy(ent->file_path, sym->name, MAX_CODE_PATH - 1);

                    if (ent->depth > out_radius->max_depth_reached)
                        out_radius->max_depth_reached = ent->depth;
                }

                if (tail < MAX_BLAST_ENTRIES)
                    queue[tail++] = (BFS_NODE){ .sym_id = file_id, .depth = curr.depth + 1 };
            }
        }

        /* 5. Subclasses inheriting from this class: (curr, inherited_by, SubClass) */
        RELATION *inh_rels[64];
        uint32_t n_inh = GraphQuerySubjectRelation(cg->graph, curr.sym_id,
                                                   cg->rel_inherited_by,
                                                   inh_rels, 64);
        for (uint32_t i = 0; i < n_inh; i++)
        {
            SYMBOL_ID sub_id = inh_rels[i]->object;
            bool already_visited = false;
            for (uint32_t v = 0; v < visited_count; v++)
            {
                if (visited[v] == sub_id)
                {
                    already_visited = true;
                    break;
                }
            }

            if (!already_visited && visited_count < MAX_BLAST_ENTRIES)
            {
                visited[visited_count++] = sub_id;
                const SYMBOL *sym = SymbolGet(cg->graph->symbols, sub_id);
                if (sym && sym->name && out_radius->entry_count < MAX_BLAST_ENTRIES)
                {
                    BLAST_RADIUS_ENTRY *ent = &out_radius->entries[out_radius->entry_count++];
                    strncpy(ent->symbol_name, sym->name, MAX_CODE_NAME - 1);
                    ent->kind = CODE_SYM_CLASS;
                    ent->depth = curr.depth + 1;

                    const char *f = CodeGraphGetFunctionFile(cg, sym->name);
                    if (f)
                        strncpy(ent->file_path, f, MAX_CODE_PATH - 1);

                    if (ent->depth > out_radius->max_depth_reached)
                        out_radius->max_depth_reached = ent->depth;
                }

                if (tail < MAX_BLAST_ENTRIES)
                    queue[tail++] = (BFS_NODE){ .sym_id = sub_id, .depth = curr.depth + 1 };
            }
        }
    }

    /* Distinct affected files calculation */
    char distinct_files[MAX_BLAST_ENTRIES][MAX_CODE_PATH];
    uint32_t n_files = 0;
    for (uint32_t i = 0; i < out_radius->entry_count; i++)
    {
        if (out_radius->entries[i].file_path[0] != '\0')
        {
            bool exists = false;
            for (uint32_t f = 0; f < n_files; f++)
            {
                if (strcmp(distinct_files[f], out_radius->entries[i].file_path) == 0)
                {
                    exists = true;
                    break;
                }
            }
            if (!exists && n_files < MAX_BLAST_ENTRIES)
            {
                strncpy(distinct_files[n_files++], out_radius->entries[i].file_path,
                        MAX_CODE_PATH - 1);
            }
        }
    }
    out_radius->affected_files_count = n_files;

    /* Risk level assessment */
    if (out_radius->affected_functions_count <= 2 && out_radius->affected_files_count <= 1)
        out_radius->risk_level = RISK_LOW;
    else if (out_radius->affected_functions_count <= 6 && out_radius->affected_files_count <= 2)
        out_radius->risk_level = RISK_MEDIUM;
    else
        out_radius->risk_level = RISK_HIGH;

    return 1;
}

int CodeGraphFormatBlastRadius(const CODE_GRAPH *cg,
                               const CODE_BLAST_RADIUS *radius,
                               char *buffer,
                               size_t buffer_size)
{
    (void)cg;
    if (!radius || !buffer || buffer_size == 0)
        return 0;

    const char *risk_str = "LOW";
    if (radius->risk_level == RISK_MEDIUM)
        risk_str = "MEDIUM";
    else if (radius->risk_level == RISK_HIGH)
        risk_str = "HIGH";

    int offset = snprintf(buffer, buffer_size,
                          "### Impact Analysis & Blast Radius for '%s'\n"
                          "- **Risk Level**: %s (%u affected functions across %u files, max depth %u)\n",
                          radius->target_symbol, risk_str,
                          radius->affected_functions_count,
                          radius->affected_files_count,
                          radius->max_depth_reached);

    if (offset < 0 || (size_t)offset >= buffer_size)
        return 0;

    /* Breakdown by depth */
    for (uint32_t d = 1; d <= radius->max_depth_reached && (size_t)offset < buffer_size; d++)
    {
        int written = snprintf(buffer + offset, buffer_size - (size_t)offset,
                               "- **Depth %u**:\n", d);
        if (written > 0 && (size_t)(offset + written) < buffer_size)
            offset += written;

        for (uint32_t i = 0; i < radius->entry_count && (size_t)offset < buffer_size; i++)
        {
            if (radius->entries[i].depth == d)
            {
                written = snprintf(buffer + offset, buffer_size - (size_t)offset,
                                   "  - `%s` (%s)%s\n",
                                   radius->entries[i].symbol_name,
                                   radius->entries[i].file_path[0] ?
                                   radius->entries[i].file_path : "unlocated",
                                   radius->entries[i].low_confidence ? " [low confidence: name match only]" : "");
                if (written > 0 && (size_t)(offset + written) < buffer_size)
                    offset += written;
            }
        }
    }

    return 1;
}

/* ============================================================
   Directory Ingestion API
   ============================================================ */

bool CodeGraphShouldIgnoreName(const char *name)
{
    if (!name || name[0] == '\0')
        return true;

    /* Current and parent directories */
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
        return true;

    /* Version control */
    if (strcmp(name, ".git") == 0 || strcmp(name, ".github") == 0 ||
        strcmp(name, ".svn") == 0 || strcmp(name, ".hg") == 0)
        return true;

    /* Build artifacts */
    if (strcmp(name, "build") == 0 || strncmp(name, "build-", 6) == 0 ||
        strncmp(name, "cmake-build-", 12) == 0 ||
        strcmp(name, "bin") == 0 || strcmp(name, "obj") == 0 ||
        strcmp(name, "target") == 0 || strcmp(name, "dist") == 0 ||
        strcmp(name, "out") == 0)
        return true;

    /* Package and environment caches */
    if (strcmp(name, "node_modules") == 0 ||
        strcmp(name, "venv") == 0 || strcmp(name, ".venv") == 0 ||
        strcmp(name, "env") == 0 || strcmp(name, ".env") == 0 ||
        strcmp(name, "__pycache__") == 0 ||
        strcmp(name, ".pytest_cache") == 0 ||
        strcmp(name, ".mypy_cache") == 0 ||
        strcmp(name, ".ruff_cache") == 0)
        return true;

    /* IDE folders */
    if (strcmp(name, ".idea") == 0 || strcmp(name, ".vscode") == 0 ||
        strcmp(name, ".gemini") == 0)
        return true;

    return false;
}

bool CodeGraphIsSupportedFile(const char *path)
{
    if (!path)
        return false;

    const char *dot = strrchr(path, '.');
    if (!dot)
        return false;

    /* C / C++ */
    if (strcmp(dot, ".c") == 0 || strcmp(dot, ".h") == 0 ||
        strcmp(dot, ".cpp") == 0 || strcmp(dot, ".hpp") == 0 ||
        strcmp(dot, ".cc") == 0 || strcmp(dot, ".cxx") == 0)
        return true;

    /* Python */
    if (strcmp(dot, ".py") == 0 || strcmp(dot, ".pyw") == 0)
        return true;

    /* TypeScript / JavaScript */
    if (strcmp(dot, ".ts") == 0 || strcmp(dot, ".tsx") == 0 ||
        strcmp(dot, ".js") == 0 || strcmp(dot, ".jsx") == 0 ||
        strcmp(dot, ".mjs") == 0 || strcmp(dot, ".cjs") == 0)
        return true;

    return false;
}

#ifdef _WIN32
static uint32_t IngestDirectoryRec(CODE_GRAPH *cg, const char *dir, uint32_t depth)
{
    if (depth > 32)
        return 0;

    char search_pattern[MAX_CODE_PATH];
    snprintf(search_pattern, sizeof(search_pattern), "%s\\*", dir);

    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(search_pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE)
        return 0;

    uint32_t count = 0;
    do
    {
        const char *name = fd.cFileName;
        if (CodeGraphShouldIgnoreName(name))
            continue;

        char full_path[MAX_CODE_PATH];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir, name);

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            count += IngestDirectoryRec(cg, full_path, depth + 1);
        }
        else if (CodeGraphIsSupportedFile(name))
        {
            if (CodeGraphIngestFile(cg, full_path))
                count++;
        }
    } while (FindNextFileA(hFind, &fd));

    FindClose(hFind);
    return count;
}
#else
static uint32_t IngestDirectoryRec(CODE_GRAPH *cg, const char *dir, uint32_t depth)
{
    if (depth > 32)
        return 0;

    DIR *d = opendir(dir);
    if (!d)
        return 0;

    uint32_t count = 0;
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL)
    {
        const char *name = entry->d_name;
        if (CodeGraphShouldIgnoreName(name))
            continue;

        char full_path[MAX_CODE_PATH];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir, name);

        struct stat st;
        if (stat(full_path, &st) == 0)
        {
            if (S_ISDIR(st.st_mode))
            {
                count += IngestDirectoryRec(cg, full_path, depth + 1);
            }
            else if (S_ISREG(st.st_mode) && CodeGraphIsSupportedFile(name))
            {
                if (CodeGraphIngestFile(cg, full_path))
                    count++;
            }
        }
    }
    closedir(d);
    return count;
}
#endif

uint32_t CodeGraphIngestDirectory(CODE_GRAPH *cg, const char *root_dir)
{
    if (!cg || !root_dir || root_dir[0] == '\0')
        return 0;

    char clean_root[MAX_CODE_PATH];
    strncpy(clean_root, root_dir, sizeof(clean_root) - 1);
    clean_root[sizeof(clean_root) - 1] = '\0';
    size_t len = strlen(clean_root);
    while (len > 1 && (clean_root[len - 1] == '/' || clean_root[len - 1] == '\\'))
    {
        clean_root[len - 1] = '\0';
        len--;
    }

    return IngestDirectoryRec(cg, clean_root, 0);
}


/* ============================================================
   Call sites, provenance, file-aware resolution, path and explain
   ============================================================ */

#define CG_MAX_SITES      (4u * 1024u * 1024u)
#define CG_MAX_CANDIDATES 64
#define CG_MAX_CLOSURE    2048
#define CG_MAX_INCLUDES   256

typedef struct { uint32_t h; SYMBOL_ID file; } CG_BASE_ENTRY;
typedef struct { SYMBOL_ID file; uint64_t hash; } CG_FILE_HASH;

typedef struct
{
    /* (caller, callee) -> chain of site indexes */
    uint32_t *bucket;
    uint32_t  bucket_count;
    uint32_t *next;
    uint32_t  next_capacity;
    /* content hash per ingested file, for the unchanged-file skip */
    CG_FILE_HASH *hashes;
    uint32_t      hash_count, hash_capacity;
    /* resolver cache, valid while the relation count is unchanged */
    uint32_t       cache_relations;
    int            cache_valid;
    CG_BASE_ENTRY *base;           /* basename hash -> file, sorted by hash */
    uint32_t       base_count;
    SYMBOL_ID      closure_file;
    int            closure_valid;
    SYMBOL_ID     *closure;
    uint32_t       closure_count;
} CG_AUX;

static CG_AUX *cg_aux(CODE_GRAPH *cg)
{
    if (!cg->aux)
        cg->aux = calloc(1, sizeof(CG_AUX));
    return (CG_AUX *)cg->aux;
}

static void cg_aux_free(CODE_GRAPH *cg)
{
    CG_AUX *a = (CG_AUX *)cg->aux;
    if (!a)
        return;
    free(a->bucket);
    free(a->next);
    free(a->hashes);
    free(a->base);
    free(a->closure);
    free(a);
    cg->aux = NULL;
}

static uint32_t cg_pair_hash(SYMBOL_ID a, SYMBOL_ID b)
{
    uint64_t x = ((uint64_t)a * 0x9E3779B97F4A7C15ull) ^ ((uint64_t)b * 0xC2B2AE3D27D4EB4Full);
    x ^= x >> 29;
    return (uint32_t)(x ^ (x >> 32));
}

static int cg_index_rebuild(CODE_GRAPH *cg, uint32_t want_buckets)
{
    CG_AUX *a = cg_aux(cg);
    uint32_t nb = 1024, i;
    if (!a)
        return 0;
    while (nb < want_buckets)
        nb <<= 1;
    free(a->bucket);
    a->bucket = (uint32_t *)malloc((size_t)nb * sizeof(uint32_t));
    if (!a->bucket)
    {
        a->bucket_count = 0;
        return 0;
    }
    a->bucket_count = nb;
    for (i = 0; i < nb; i++)
        a->bucket[i] = UINT32_MAX;
    if (a->next_capacity < cg->site_capacity)
    {
        uint32_t *n2 = (uint32_t *)realloc(a->next, (size_t)cg->site_capacity * sizeof(uint32_t));
        if (!n2)
            return 0;
        a->next = n2;
        a->next_capacity = cg->site_capacity;
    }
    for (i = 0; i < cg->site_count; i++)
    {
        uint32_t b = cg_pair_hash(cg->sites[i].caller, cg->sites[i].callee) & (nb - 1);
        a->next[i] = a->bucket[b];
        a->bucket[b] = i;
    }
    return 1;
}

static void cg_record_call(CODE_GRAPH *cg, SYMBOL_ID caller, SYMBOL_ID callee, SYMBOL_ID file, uint32_t line)
{
    CG_AUX *a = cg_aux(cg);
    RELATION *r;
    uint32_t b;
    if (!a)
        return;
    /* Provenance on the name-level edge: the first call site's file (the line is in the site table). */
    r = GraphFindRelation(cg->graph, caller, cg->rel_calls, callee);
    if (r && r->source == 0)
        RelationSetSource(r, file);
    r = GraphFindRelation(cg->graph, callee, cg->rel_called_by, caller);
    if (r && r->source == 0)
        RelationSetSource(r, file);
    if (cg->site_count >= CG_MAX_SITES)
    {
        cg->sites_dropped++;
        return;
    }
    if (cg->site_count == cg->site_capacity)
    {
        uint32_t nc = cg->site_capacity ? cg->site_capacity * 2 : 4096;
        CODE_CALL_SITE *n = (CODE_CALL_SITE *)realloc(cg->sites, (size_t)nc * sizeof(CODE_CALL_SITE));
        if (!n)
        {
            cg->sites_dropped++;
            return;
        }
        cg->sites = n;
        cg->site_capacity = nc;
    }
    cg->sites[cg->site_count].caller = caller;
    cg->sites[cg->site_count].callee = callee;
    cg->sites[cg->site_count].file = file;
    cg->sites[cg->site_count].line = line;
    cg->site_count++;
    if (a->bucket_count == 0 || cg->site_count > a->bucket_count * 2 || a->next_capacity < cg->site_capacity)
    {
        if (!cg_index_rebuild(cg, a->bucket_count ? a->bucket_count * 2 : 1024))
            return;
        return; /* the rebuild indexed the site just added */
    }
    b = cg_pair_hash(caller, callee) & (a->bucket_count - 1);
    a->next[cg->site_count - 1] = a->bucket[b];
    a->bucket[b] = cg->site_count - 1;
}

/* FNV-1a 64 over the file text. */
static uint64_t cg_hash_text(const char *s)
{
    uint64_t h = 1469598103934665603ull;
    for (; *s; s++)
    {
        h ^= (unsigned char)*s;
        h *= 1099511628211ull;
    }
    return h;
}

/* Returns 1 when the file was ingested with exactly this text before (nothing to do). When the text
   changed, the file's old call sites are dropped; edges already in the graph stay (additive). */
static int cg_file_unchanged(CODE_GRAPH *cg, const char *file_path, const char *text)
{
    CG_AUX *a = cg_aux(cg);
    SYMBOL_ID fsym;
    uint64_t h;
    uint32_t i;
    if (!a)
        return 0;
    fsym = GraphAddSymbol(cg->graph, file_path);
    h = cg_hash_text(text);
    for (i = 0; i < a->hash_count; i++)
    {
        if (a->hashes[i].file != fsym)
            continue;
        if (a->hashes[i].hash == h)
            return 1;
        a->hashes[i].hash = h;
        {   /* drop this file's old sites */
            uint32_t w = 0, k;
            for (k = 0; k < cg->site_count; k++)
                if (cg->sites[k].file != fsym)
                    cg->sites[w++] = cg->sites[k];
            cg->site_count = w;
            cg_index_rebuild(cg, a->bucket_count ? a->bucket_count : 1024);
        }
        return 0;
    }
    if (a->hash_count == a->hash_capacity)
    {
        uint32_t nc = a->hash_capacity ? a->hash_capacity * 2 : 256;
        CG_FILE_HASH *n = (CG_FILE_HASH *)realloc(a->hashes, (size_t)nc * sizeof(CG_FILE_HASH));
        if (!n)
            return 0;
        a->hashes = n;
        a->hash_capacity = nc;
    }
    a->hashes[a->hash_count].file = fsym;
    a->hashes[a->hash_count].hash = h;
    a->hash_count++;
    return 0;
}

/* ---- resolver ---- */

static const char *cg_basename(const char *path)
{
    const char *b = path, *p;
    for (p = path; *p; p++)
        if (*p == '/' || *p == '\\')
            b = p + 1;
    return b;
}

static uint32_t cg_name_hash(const char *s)
{
    uint32_t h = 2166136261u;
    for (; *s; s++)
    {
        h ^= (unsigned char)*s;
        h *= 16777619u;
    }
    return h;
}

/* Hash of the base name without its extension: "include/agent_git.h" and "src/agent_git.c" agree. */
static uint32_t cg_stem_hash(const char *path)
{
    const char *b = cg_basename(path);
    const char *dot = strrchr(b, '.');
    uint32_t h = 2166136261u;
    const char *e = dot ? dot : b + strlen(b);
    for (; b < e; b++)
    {
        h ^= (unsigned char)*b;
        h *= 16777619u;
    }
    return h;
}

static int cg_base_cmp(const void *x, const void *y)
{
    const CG_BASE_ENTRY *a = (const CG_BASE_ENTRY *)x, *b = (const CG_BASE_ENTRY *)y;
    if (a->h != b->h)
        return a->h < b->h ? -1 : 1;
    return a->file < b->file ? -1 : (a->file > b->file);
}

static int cg_cache_build(CODE_GRAPH *cg)
{
    CG_AUX *a = cg_aux(cg);
    RELATION_TABLE *rt = cg->graph->relations;
    uint32_t i, n = 0, cap = 0;
    if (!a)
        return 0;
    if (a->cache_valid && a->cache_relations == rt->count)
        return 1;
    free(a->base);
    a->base = NULL;
    a->base_count = 0;
    a->closure_valid = 0;
    /* every symbol that is the subject of includes, defines_func or declares_func is a file */
    for (i = 0; i < rt->count; i++)
    {
        const RELATION *r = &rt->items[i];
        if (r->polarity != POLARITY_POSITIVE)
            continue;
        if (r->relation == cg->rel_includes || r->relation == cg->rel_defines_func ||
            r->relation == cg->rel_declares_func)
        {
            const SYMBOL *s = SymbolGet(cg->graph->symbols, r->subject);
            if (!s || !s->name)
                continue;
            if (n > 0 && a->base[n - 1].file == r->subject)
                continue;
            if (n == cap)
            {
                uint32_t nc = cap ? cap * 2 : 1024;
                CG_BASE_ENTRY *nb = (CG_BASE_ENTRY *)realloc(a->base, (size_t)nc * sizeof(CG_BASE_ENTRY));
                if (!nb)
                    return 0;
                a->base = nb;
                cap = nc;
            }
            a->base[n].h = cg_name_hash(cg_basename(s->name));
            a->base[n].file = r->subject;
            n++;
        }
    }
    qsort(a->base, n, sizeof(CG_BASE_ENTRY), cg_base_cmp);
    {   /* unique */
        uint32_t w = 0;
        for (i = 0; i < n; i++)
            if (w == 0 || a->base[w - 1].h != a->base[i].h || a->base[w - 1].file != a->base[i].file)
                a->base[w++] = a->base[i];
        n = w;
    }
    a->base_count = n;
    a->cache_relations = rt->count;
    a->cache_valid = 1;
    if (!a->closure)
        a->closure = (SYMBOL_ID *)malloc(CG_MAX_CLOSURE * sizeof(SYMBOL_ID));
    return a->closure != NULL;
}

static int cg_closure_has(const CG_AUX *a, SYMBOL_ID f)
{
    for (uint32_t i = 0; i < a->closure_count; i++)
        if (a->closure[i] == f)
            return 1;
    return 0;
}

/* Files whose declarations the call's file can see: itself plus everything reachable through #include,
   matched by base name (an include name is not a path). Over-approximates when two directories hold the
   same header name. */
static void cg_closure_build(CODE_GRAPH *cg, SYMBOL_ID file)
{
    CG_AUX *a = cg_aux(cg);
    uint32_t head = 0;
    if (a->closure_valid && a->closure_file == file)
        return;
    a->closure_count = 0;
    a->closure[a->closure_count++] = file;
    while (head < a->closure_count)
    {
        RELATION *inc[CG_MAX_INCLUDES];
        SYMBOL_ID cur = a->closure[head++];
        uint32_t n = GraphQuerySubjectRelation(cg->graph, cur, cg->rel_includes, inc, CG_MAX_INCLUDES), k;
        for (k = 0; k < n; k++)
        {
            const SYMBOL *hs = SymbolGet(cg->graph->symbols, inc[k]->object);
            uint32_t h, lo, hi;
            if (!hs || !hs->name)
                continue;
            h = cg_name_hash(cg_basename(hs->name));
            lo = 0;
            hi = a->base_count;
            while (lo < hi)
            {
                uint32_t mid = (lo + hi) / 2;
                if (a->base[mid].h < h)
                    lo = mid + 1;
                else
                    hi = mid;
            }
            for (; lo < a->base_count && a->base[lo].h == h; lo++)
            {
                const SYMBOL *fs = SymbolGet(cg->graph->symbols, a->base[lo].file);
                if (!fs || !fs->name || strcmp(cg_basename(fs->name), cg_basename(hs->name)) != 0)
                    continue;
                if (!cg_closure_has(a, a->base[lo].file) && a->closure_count < CG_MAX_CLOSURE)
                    a->closure[a->closure_count++] = a->base[lo].file;
            }
        }
    }
    a->closure_file = file;
    a->closure_valid = 1;
}

const char *CodeGraphEdgeLabelName(CODE_EDGE_LABEL label)
{
    switch (label)
    {
    case CODE_EDGE_EXTRACTED: return "EXTRACTED";
    case CODE_EDGE_INFERRED:  return "INFERRED";
    case CODE_EDGE_AMBIGUOUS: return "AMBIGUOUS";
    case CODE_EDGE_UNLINKED:  return "UNLINKED";
    case CODE_EDGE_EXTERNAL:  return "EXTERNAL";
    }
    return "UNKNOWN";
}

static void cg_set_str(char *dst, size_t n, const char *src)
{
    if (!src)
        src = "";
    snprintf(dst, n, "%s", src);
}

void CodeGraphResolveSite(const CODE_GRAPH *cgc, const CODE_CALL_SITE *site, CODE_CALL_EVIDENCE *out)
{
    CODE_GRAPH *cg = (CODE_GRAPH *)cgc; /* the cache is internal; documented as not thread safe */
    SYMBOL_ID cands[CG_MAX_CANDIDATES];
    RELATION *infile[CG_MAX_CANDIDATES];
    uint32_t nc = 0, i, k;
    const SYMBOL *fs;
    CG_AUX *a;
    if (!out)
        return;
    memset(out, 0, sizeof(*out));
    out->label = CODE_EDGE_EXTERNAL;
    if (!cg || !cg->graph || !site)
        return;
    fs = SymbolGet(cg->graph->symbols, site->file);
    cg_set_str(out->file, sizeof(out->file), fs ? fs->name : "");
    out->line = site->line;
    /* files that define the callee as a function */
    {
        uint32_t n = GraphQuerySubjectRelation(cg->graph, site->callee, cg->rel_in_file, infile, CG_MAX_CANDIDATES);
        for (i = 0; i < n; i++)
            if (GraphFindRelation(cg->graph, infile[i]->object, cg->rel_defines_func, site->callee))
                cands[nc++] = infile[i]->object;
    }
    if (nc == 0)
        return; /* EXTERNAL */
    for (i = 0; i < nc; i++)
        if (cands[i] == site->file)
        {
            out->label = CODE_EDGE_EXTRACTED;
            cg_set_str(out->target_file, sizeof(out->target_file), fs ? fs->name : "");
            return;
        }
    if (!cg_cache_build(cg))
    {
        out->label = CODE_EDGE_AMBIGUOUS;
        return;
    }
    a = (CG_AUX *)cg->aux;
    cg_closure_build(cg, site->file);
    {
        int declared = 0;
        SYMBOL_ID decl_files[64];
        uint32_t nd = 0;
        SYMBOL_ID inclos[CG_MAX_CANDIDATES];
        uint32_t ni = 0;
        for (k = 0; k < a->closure_count; k++)
        {
            SYMBOL_ID h = a->closure[k];
            if (GraphFindRelation(cg->graph, h, cg->rel_declares_func, site->callee) ||
                GraphFindRelation(cg->graph, h, cg->rel_defines_func, site->callee))
            {
                declared = 1;
                if (nd < 64)
                    decl_files[nd++] = h;
            }
        }
        if (!declared)
        {
            out->label = CODE_EDGE_UNLINKED;
            return;
        }
        for (i = 0; i < nc; i++)
            if (cg_closure_has(a, cands[i]))
                inclos[ni++] = cands[i];
        if (ni == 1)
        {
            const SYMBOL *ts = SymbolGet(cg->graph->symbols, inclos[0]);
            out->label = CODE_EDGE_INFERRED;
            cg_set_str(out->target_file, sizeof(out->target_file), ts ? ts->name : "");
            return;
        }
        {   /* the candidate sharing a stem with a header that declares the name */
            SYMBOL_ID pick = 0;
            uint32_t hits = 0;
            for (i = 0; i < nc; i++)
            {
                const SYMBOL *cs = SymbolGet(cg->graph->symbols, cands[i]);
                int match = 0;
                if (!cs || !cs->name)
                    continue;
                for (k = 0; k < nd && !match; k++)
                {
                    const SYMBOL *ds = SymbolGet(cg->graph->symbols, decl_files[k]);
                    if (ds && ds->name && cg_stem_hash(ds->name) == cg_stem_hash(cs->name))
                        match = 1;
                }
                if (match)
                {
                    pick = cands[i];
                    hits++;
                }
            }
            if (hits == 1)
            {
                const SYMBOL *ts = SymbolGet(cg->graph->symbols, pick);
                out->label = CODE_EDGE_INFERRED;
                cg_set_str(out->target_file, sizeof(out->target_file), ts ? ts->name : "");
                return;
            }
        }
        if (nc == 1)
        {
            const SYMBOL *ts = SymbolGet(cg->graph->symbols, cands[0]);
            out->label = CODE_EDGE_INFERRED;
            cg_set_str(out->target_file, sizeof(out->target_file), ts ? ts->name : "");
            return;
        }
        out->label = CODE_EDGE_AMBIGUOUS;
    }
}

/* ---- queries over sites ---- */

uint32_t CodeGraphGetCallEvidence(const CODE_GRAPH *cg, const char *caller, const char *callee,
                                  CODE_CALL_EVIDENCE *out, uint32_t max_results)
{
    CG_AUX *a;
    SYMBOL_ID c, d;
    uint32_t n = 0, i;
    if (!cg || !cg->graph || !caller || !callee || !out || max_results == 0 || !cg->aux)
        return 0;
    a = (CG_AUX *)cg->aux;
    if (a->bucket_count == 0)
        return 0;
    c = SymbolFind(cg->graph->symbols, caller);
    d = SymbolFind(cg->graph->symbols, callee);
    if (c == SYMBOL_INVALID || d == SYMBOL_INVALID)
        return 0;
    /* the chain is newest first; collect then reverse so results are in ingest order */
    for (i = a->bucket[cg_pair_hash(c, d) & (a->bucket_count - 1)]; i != UINT32_MAX; i = a->next[i])
        if (cg->sites[i].caller == c && cg->sites[i].callee == d)
        {
            if (n < max_results)
                CodeGraphResolveSite(cg, &cg->sites[i], &out[n]);
            n++;
        }
    if (n > max_results)
        n = max_results;
    for (i = 0; i < n / 2; i++)
    {
        CODE_CALL_EVIDENCE t = out[i];
        out[i] = out[n - 1 - i];
        out[n - 1 - i] = t;
    }
    return n;
}

bool CodeGraphGetEdgeLabel(const CODE_GRAPH *cg, const char *caller, const char *callee, CODE_EDGE_LABEL *label)
{
    CODE_CALL_EVIDENCE ev[32];
    uint32_t n = CodeGraphGetCallEvidence(cg, caller, callee, ev, 32), i;
    CODE_EDGE_LABEL best = CODE_EDGE_EXTERNAL;
    if (n == 0)
        return false;
    for (i = 0; i < n; i++)
        if (ev[i].label < best)
            best = ev[i].label;
    if (label)
        *label = best;
    return true;
}

static bool cg_resolved_edge(const CODE_GRAPH *cg, const char *caller, const char *callee)
{
    CODE_EDGE_LABEL l;
    return CodeGraphGetEdgeLabel(cg, caller, callee, &l) &&
           (l == CODE_EDGE_EXTRACTED || l == CODE_EDGE_INFERRED);
}

static uint32_t cg_resolved_list(const CODE_GRAPH *cg, const char *func_name, int callers,
                                 char results[][MAX_CODE_NAME], uint32_t max_results)
{
    char (*raw)[MAX_CODE_NAME];
    uint32_t n, i, count = 0;
    if (!cg || !func_name || !results || max_results == 0)
        return 0;
    raw = (char (*)[MAX_CODE_NAME])malloc((size_t)64 * MAX_CODE_NAME);
    if (!raw)
        return 0;
    n = callers ? CodeGraphGetCallers(cg, func_name, raw, 64) : CodeGraphGetCallees(cg, func_name, raw, 64);
    for (i = 0; i < n && count < max_results; i++)
    {
        bool ok = callers ? cg_resolved_edge(cg, raw[i], func_name) : cg_resolved_edge(cg, func_name, raw[i]);
        if (ok)
        {
            strncpy(results[count], raw[i], MAX_CODE_NAME - 1);
            results[count][MAX_CODE_NAME - 1] = '\0';
            count++;
        }
    }
    free(raw);
    return count;
}

uint32_t CodeGraphGetResolvedCallers(const CODE_GRAPH *cg, const char *func_name,
                                     char results[][MAX_CODE_NAME], uint32_t max_results)
{
    return cg_resolved_list(cg, func_name, 1, results, max_results);
}

uint32_t CodeGraphGetResolvedCallees(const CODE_GRAPH *cg, const char *func_name,
                                     char results[][MAX_CODE_NAME], uint32_t max_results)
{
    return cg_resolved_list(cg, func_name, 0, results, max_results);
}

uint32_t CodeGraphFindCallPath(const CODE_GRAPH *cg, const char *from, const char *to,
                               uint32_t max_depth, char path[][MAX_CODE_NAME], uint32_t max_nodes)
{
    enum { MAXN = 2048 };
    SYMBOL_ID *node, *parent;
    uint32_t *depth;
    uint32_t head = 0, tail = 0, i;
    SYMBOL_ID f, t;
    uint32_t found = UINT32_MAX;
    if (!cg || !cg->graph || !from || !to || !path || max_nodes == 0)
        return 0;
    f = SymbolFind(cg->graph->symbols, from);
    t = SymbolFind(cg->graph->symbols, to);
    if (f == SYMBOL_INVALID || t == SYMBOL_INVALID)
        return 0;
    node = (SYMBOL_ID *)malloc(MAXN * sizeof(SYMBOL_ID));
    parent = (SYMBOL_ID *)malloc(MAXN * sizeof(SYMBOL_ID));
    depth = (uint32_t *)malloc(MAXN * sizeof(uint32_t));
    if (!node || !parent || !depth)
    {
        free(node); free(parent); free(depth);
        return 0;
    }
    node[tail] = f; parent[tail] = UINT32_MAX; depth[tail] = 0; tail++;
    if (f == t)
        found = 0;
    while (found == UINT32_MAX && head < tail)
    {
        RELATION *rels[1024];
        const SYMBOL *cs = SymbolGet(cg->graph->symbols, node[head]);
        uint32_t n, k;
        if (depth[head] >= max_depth || !cs || !cs->name)
        {
            head++;
            continue;
        }
        n = GraphQuerySubjectRelation(cg->graph, node[head], cg->rel_calls, rels, 1024);
        for (k = 0; k < n && found == UINT32_MAX; k++)
        {
            const SYMBOL *ds = SymbolGet(cg->graph->symbols, rels[k]->object);
            uint32_t j;
            int seen = 0;
            if (!ds || !ds->name || !cg_resolved_edge(cg, cs->name, ds->name))
                continue;
            for (j = 0; j < tail; j++)
                if (node[j] == rels[k]->object)
                {
                    seen = 1;
                    break;
                }
            if (seen || tail >= MAXN)
                continue;
            node[tail] = rels[k]->object;
            parent[tail] = head;
            depth[tail] = depth[head] + 1;
            if (rels[k]->object == t)
                found = tail;
            tail++;
        }
        head++;
    }
    {
        uint32_t len = 0;
        if (found != UINT32_MAX)
        {
            uint32_t cur;
            for (cur = found; cur != UINT32_MAX; cur = parent[cur])
                len++;
            if (len <= max_nodes)
            {
                uint32_t w = len;
                for (cur = found; cur != UINT32_MAX; cur = parent[cur])
                {
                    const SYMBOL *s = SymbolGet(cg->graph->symbols, node[cur]);
                    w--;
                    strncpy(path[w], s ? s->name : "", MAX_CODE_NAME - 1);
                    path[w][MAX_CODE_NAME - 1] = '\0';
                }
            }
            else
                len = 0;
        }
        (void)i;
        free(node); free(parent); free(depth);
        return len;
    }
}

int CodeGraphExplainEdge(const CODE_GRAPH *cg, const char *caller, const char *callee,
                         char *buffer, size_t buffer_size)
{
    CODE_CALL_EVIDENCE ev[64];
    uint32_t n, i;
    size_t used = 0;
    if (!cg || !caller || !callee || !buffer || buffer_size == 0)
        return 0;
    buffer[0] = '\0';
    n = CodeGraphGetCallEvidence(cg, caller, callee, ev, 64);
    if (n == 0)
    {
        snprintf(buffer, buffer_size, "%s -> %s: no call site recorded", caller, callee);
        return 0;
    }
    used += (size_t)snprintf(buffer + used, buffer_size - used, "%s -> %s: %u call site(s)\n", caller, callee, n);
    for (i = 0; i < n && used < buffer_size; i++)
        used += (size_t)snprintf(buffer + used, buffer_size - used, "  %s:%u  %s%s%s\n", ev[i].file, ev[i].line,
                                 CodeGraphEdgeLabelName(ev[i].label), ev[i].target_file[0] ? " -> " : "",
                                 ev[i].target_file);
    if (used >= buffer_size)
        buffer[buffer_size - 1] = '\0';
    return 1;
}
