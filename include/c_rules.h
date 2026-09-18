#ifndef C_RULES_H
#define C_RULES_H

#include <stddef.h>

#define MAX_C_RULES    128
#define MAX_NAME_LEN    32
#define MAX_EXPR_LEN    96

typedef enum {
    C_RULE_HEADER = 0,
    C_RULE_TYPE_SIG,
    C_RULE_INVARIANT,
    C_RULE_TEMPLATE,
    C_RULE_REPAIR
} CRuleType;

typedef struct {
    CRuleType type;
    char key[MAX_NAME_LEN];     /* ej. "safe_copy", "memcpy", "size_t" */
    char p1[MAX_EXPR_LEN];      /* ej. "size_t", "<string.h>", "sz > 0" */
    char p2[MAX_EXPR_LEN];      /* ej. "char *dst, const char *src, size_t sz" */
} CRule;

typedef struct {
    size_t count;
    CRule rules[MAX_C_RULES];   /* Tabla lineal fija acotada */
} CRulesTable;

int CRulesLoad(const char *filepath, CRulesTable *tbl);
const CRule* CRulesFind(const CRulesTable *tbl, CRuleType type, const char *key);

/* Global table for the synthesis loop (loaded once, idempotent).
   Returns the rule count (0 when the file is missing: fail-closed). */
int CRulesInit(void);
const CRulesTable *CRulesTableGet(void);

#endif /* C_RULES_H */
