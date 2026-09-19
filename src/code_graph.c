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

    return cg;
}

void CodeGraphDestroy(CODE_GRAPH *cg)
{
    if (!cg)
        return;

    if (cg->graph)
        GraphDestroy(cg->graph);

    free(cg);
}

/* ============================================================
   Ingestion / Parsing Engine
   ============================================================ */

int CodeGraphIngestSource(CODE_GRAPH *cg, const char *file_path, const char *source_code)
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
            brace_depth++;
            p++;
            continue;
        }

        if (*p == '}')
        {
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
                        if (*q == '\n')
                            line_number++;
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

            /* Struct detection */
            if (strcmp(token, "struct") == 0)
            {
                const char *q = p;
                while (*q && isspace((unsigned char)*q))
                {
                    if (*q == '\n')
                        line_number++;
                    q++;
                }
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
                {
                    if (*q == '\n')
                        line_number++;
                    q++;
                }

                if (*q == '(' && !is_control_keyword(token) && !is_type_keyword(token))
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

                if (*q == '(' && !is_control_keyword(token) && !is_type_keyword(token))
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

        /* 10. Default advance */
        p++;
    }

    return 1;
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
        for (uint32_t i = 0; i < n_callers; i++)
        {
            SYMBOL_ID caller_id = caller_rels[i]->object;
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
                                   "  - `%s` (%s)\n",
                                   radius->entries[i].symbol_name,
                                   radius->entries[i].file_path[0] ?
                                   radius->entries[i].file_path : "unlocated");
                if (written > 0 && (size_t)(offset + written) < buffer_size)
                    offset += written;
            }
        }
    }

    return 1;
}
