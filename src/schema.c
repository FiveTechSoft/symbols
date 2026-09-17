#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "schema.h"

/* ============================================================
   Template registry: DERIVATION RULE keyed by observed surface
   connective. Never asserted; mirrors gm_template/3 exactly.
   ============================================================ */

static const char *ConnNormalize(const char *conn)
{
    if (strcmp(conn, "acting_on") == 0)
        return "acting on";
    return conn;
}

static int TemplateLookup(const char *conn, SCHEMA_ORDER *order)
{
    conn = ConnNormalize(conn);
    if (strcmp(conn, "cong") == 0)
    {
        *order = SCHEMA_ORDER_SYM;
        return 1;
    }
    if (strcmp(conn, "proportional") == 0)
    {
        *order = SCHEMA_ORDER_DEP_FIRST;
        return 1;
    }
    if (strcmp(conn, "after") == 0)
    {
        *order = SCHEMA_ORDER_CHAIN;
        return 1;
    }
    if (strcmp(conn, "acting on") == 0)
    {
        *order = SCHEMA_ORDER_OPER_FIRST;
        return 1;
    }
    if (strcmp(conn, "isa") == 0)
    {
        *order = SCHEMA_ORDER_CHAIN;
        return 1;
    }
    return 0;
}

/* ---- helpers ---- */

static RELATIONAL_SCHEMA *SchemaFind(SCHEMA_KB *kb, const char *family)
{
    for (uint32_t i = 0; i < kb->num_schemas; i++)
        if (strcmp(kb->schemas[i].family, family) == 0)
            return &kb->schemas[i];
    return NULL;
}

static int VocabKnown(const SCHEMA_KB *kb, const char *token)
{
    for (uint32_t i = 0; i < kb->num_vocab; i++)
        if (strcmp(kb->vocab[i], token) == 0)
            return 1;
    return 0;
}

static void VocabAdd(SCHEMA_KB *kb, const char *token)
{
    if (token == NULL || kb->num_vocab >= SCHEMA_VOCAB_MAX)
        return;
    if (!VocabKnown(kb, token))
    {
        strncpy(kb->vocab[kb->num_vocab], token, SCHEMA_TOKEN_MAX - 1);
        kb->vocab[kb->num_vocab][SCHEMA_TOKEN_MAX - 1] = '\0';
        kb->num_vocab++;
    }
}

static int RoleIs(const SCHEMA_KB *kb, const char *token, int which)
{
    for (uint32_t i = 0; i < kb->num_roles; i++)
    {
        const SCHEMA_ROLE *r = &kb->roles[i];
        if (strcmp(r->name, token) == 0)
        {
            switch (which)
            {
            case 0: return r->is_dependent;
            case 1: return r->is_independent;
            case 2: return r->is_operator;
            default: return r->is_patient;
            }
        }
    }
    return 0;
}

/* ---- lifecycle ---- */

void SchemaKBInit(SCHEMA_KB *kb)
{
    if (kb == NULL)
        return;
    memset(kb, 0, sizeof(*kb));
}

uint32_t SchemaCount(const SCHEMA_KB *kb)
{
    return kb == NULL ? 0 : kb->num_schemas;
}

const RELATIONAL_SCHEMA *SchemaGet(const SCHEMA_KB *kb, uint32_t i)
{
    if (kb == NULL || i >= kb->num_schemas)
        return NULL;
    return &kb->schemas[i];
}

const RELATIONAL_SCHEMA *SchemaFindFamily(const SCHEMA_KB *kb,
                                          const char *family)
{
    if (kb == NULL || family == NULL)
        return NULL;
    return SchemaFind((SCHEMA_KB *)kb, family);
}

int SchemaVocabKnown(const SCHEMA_KB *kb, const char *token)
{
    if (kb == NULL || token == NULL)
        return 0;
    return VocabKnown(kb, token);
}

static int PairEvidence(const SCHEMA_KB *kb, const char *family,
                        const char *s, const char *o);

int SchemaBuildSentence(const SCHEMA_KB *kb, const char *family,
                        const char *subject, const char *object,
                        char *out, size_t out_size)
{
    if (kb == NULL || out == NULL || out_size < 4)
        return 0;
    RELATIONAL_SCHEMA *s = SchemaFind((SCHEMA_KB *)kb, family);
    if (s == NULL || s->num_connectives == 0)
        return 0;
    char body[3 * SCHEMA_TOKEN_MAX + 2];
    snprintf(body, sizeof(body), "%s %s %s", subject, s->connectives[0],
             object);
    size_t blen = strlen(body);
    if (blen + 2 > out_size)
        return 0;
    memcpy(out, body, blen);
    if (out[0] >= 'a' && out[0] <= 'z')
        out[0] = (char)(out[0] - 32);
    out[blen] = '.';
    out[blen + 1] = '\0';
    return 1;
}

/* ---- roles ---- */

void SchemaDeclareRole(SCHEMA_KB *kb, const char *token,
                       int dependent, int independent,
                       int operator_, int patient)
{
    if (kb == NULL || token == NULL || kb->num_roles >= SCHEMA_ROLE_MAX)
        return;
    for (uint32_t i = 0; i < kb->num_roles; i++)
    {
        if (strcmp(kb->roles[i].name, token) == 0)
        {
            kb->roles[i].is_dependent = dependent;
            kb->roles[i].is_independent = independent;
            kb->roles[i].is_operator = operator_;
            kb->roles[i].is_patient = patient;
            return;
        }
    }
    SCHEMA_ROLE *r = &kb->roles[kb->num_roles++];
    strncpy(r->name, token, SCHEMA_TOKEN_MAX - 1);
    r->name[SCHEMA_TOKEN_MAX - 1] = '\0';
    r->is_dependent = dependent;
    r->is_independent = independent;
    r->is_operator = operator_;
    r->is_patient = patient;
}

/* ---- discovery ---- */

/* Extract the surface connective from the sentence tokens:
   3 tokens [S, Conn, O] -> middle; 4 tokens [S, C1, C2, O] ->
   two-word connective (joined by space). */
static int SentConnective(const char *const *sentence, uint32_t n,
                          char *conn, size_t conn_size)
{
    if (n == 3 && sentence[1] != NULL)
    {
        strncpy(conn, sentence[1], conn_size - 1);
        conn[conn_size - 1] = '\0';
        return 1;
    }
    if (n == 4 && sentence[1] != NULL && sentence[2] != NULL)
    {
        snprintf(conn, conn_size, "%s %s", sentence[1], sentence[2]);
        return 1;
    }
    return 0;
}

int SchemaObserveExemplar(SCHEMA_KB *kb, const char *exemplar_id,
                          const char *const *triple, uint32_t num_triple,
                          const char *const *sentence, uint32_t num_sentence)
{
    if (kb == NULL || exemplar_id == NULL || triple == NULL ||
        num_triple < 3 || sentence == NULL || num_sentence < 3)
        return 0;

    char conn[2 * SCHEMA_TOKEN_MAX];
    if (!SentConnective(sentence, num_sentence, conn, sizeof(conn)))
        return 0;

    SCHEMA_ORDER order;
    if (!TemplateLookup(conn, &order))
        return 0;

    const char *family = triple[1];
    RELATIONAL_SCHEMA *s = SchemaFind(kb, family);
    if (s == NULL)
    {
        if (kb->num_schemas >= SCHEMA_MAX)
            return 0;
        s = &kb->schemas[kb->num_schemas++];
        memset(s, 0, sizeof(*s));
        strncpy(s->family, family, SCHEMA_TOKEN_MAX - 1);
        s->family[SCHEMA_TOKEN_MAX - 1] = '\0';
        s->order = order;
        s->num_connectives = 1;
        strncpy(s->connectives[0], ConnNormalize(conn), SCHEMA_TOKEN_MAX - 1);
        s->connectives[0][SCHEMA_TOKEN_MAX - 1] = '\0';
        s->num_prov = 1;
        strncpy(s->prov[0], exemplar_id, SCHEMA_PROV_MAX - 1);
        s->prov[0][SCHEMA_PROV_MAX - 1] = '\0';
    }
    else
    {
        int present = 0;
        for (uint32_t i = 0; i < s->num_prov; i++)
            if (strcmp(s->prov[i], exemplar_id) == 0)
                present = 1;
        if (!present && s->num_prov < SCHEMA_PROV_MAX)
        {
            strncpy(s->prov[s->num_prov], exemplar_id, SCHEMA_PROV_MAX - 1);
            s->prov[s->num_prov][SCHEMA_PROV_MAX - 1] = '\0';
            s->num_prov++;
        }
        const char *nc = ConnNormalize(conn);
        int conn_known = 0;
        for (uint32_t i = 0; i < s->num_connectives; i++)
            if (strcmp(s->connectives[i], nc) == 0)
                conn_known = 1;
        if (!conn_known && s->num_connectives < SCHEMA_CONN_MAX)
        {
            strncpy(s->connectives[s->num_connectives], nc,
                    SCHEMA_TOKEN_MAX - 1);
            s->connectives[s->num_connectives][SCHEMA_TOKEN_MAX - 1] = '\0';
            s->num_connectives++;
        }
    }

    /* vocabulary from exemplar subject/object */
    VocabAdd(kb, triple[0]);
    VocabAdd(kb, triple[2]);
    return 1;
}

/* ---- presentation (domain B) ---- */

int SchemaPresentPair(SCHEMA_KB *kb, const char *family,
                      const char *subject, const char *object)
{
    if (kb == NULL || family == NULL || subject == NULL || object == NULL)
        return 0;
    if (SchemaFind(kb, family) == NULL)
        return 0; /* no schema: nothing to present against */
    /* idempotent: the same observed pair is one piece of evidence */
    if (PairEvidence(kb, family, subject, object))
        return 1;
    if (kb->num_pairs >= SCHEMA_PAIR_MAX)
        return 0;
    PAIR_EVID *p = &kb->pairs[kb->num_pairs++];
    strncpy(p->family, family, SCHEMA_TOKEN_MAX - 1);
    p->family[SCHEMA_TOKEN_MAX - 1] = '\0';
    strncpy(p->subject, subject, SCHEMA_TOKEN_MAX - 1);
    p->subject[SCHEMA_TOKEN_MAX - 1] = '\0';
    strncpy(p->object, object, SCHEMA_TOKEN_MAX - 1);
    p->object[SCHEMA_TOKEN_MAX - 1] = '\0';
    VocabAdd(kb, subject);
    VocabAdd(kb, object);
    return 1;
}

/* ---- order check, two modes ---- */

static int PairEvidence(const SCHEMA_KB *kb, const char *family,
                        const char *s, const char *o)
{
    for (uint32_t i = 0; i < kb->num_pairs; i++)
    {
        const PAIR_EVID *p = &kb->pairs[i];
        if (strcmp(p->family, family) == 0 &&
            strcmp(p->subject, s) == 0 && strcmp(p->object, o) == 0)
            return 1;
    }
    return 0;
}

static int OrderAdmits(const SCHEMA_KB *kb, const RELATIONAL_SCHEMA *s,
                       const char *subj, const char *obj)
{
    /* MODE 1: observed pair evidence admits only that direction */
    if (PairEvidence(kb, s->family, subj, obj))
        return 1;
    /* MODE 2: schema order term as constraint */
    switch (s->order)
    {
    case SCHEMA_ORDER_SYM:
        return 1;
    case SCHEMA_ORDER_DEP_FIRST:
        return RoleIs(kb, subj, 0) && RoleIs(kb, obj, 1);
    case SCHEMA_ORDER_OPER_FIRST:
        return RoleIs(kb, subj, 2) && RoleIs(kb, obj, 3);
    case SCHEMA_ORDER_CHAIN:
    default:
        return 0; /* fail-closed: no direction evidence */
    }
}

int SchemaRealize(const SCHEMA_KB *kb, const char *family,
                  const char *subject, const char *object,
                  char *out, size_t out_size)
{
    if (kb == NULL || out == NULL || out_size < 4)
        return 0;
    RELATIONAL_SCHEMA *s = SchemaFind((SCHEMA_KB *)kb, family);
    if (s == NULL)
        return 0;
    if (!OrderAdmits(kb, s, subject, object))
        return 0;
    if (!VocabKnown(kb, subject) || !VocabKnown(kb, object))
        return 0;
    if (s->num_connectives == 0)
        return 0;

    char body[3 * SCHEMA_TOKEN_MAX + 2];
    snprintf(body, sizeof(body), "%s %s %s", subject, s->connectives[0],
             object);

    size_t blen = strlen(body);
    if (blen + 2 > out_size)
        return 0;
    memcpy(out, body, blen);
    if (out[0] >= 'a' && out[0] <= 'z')
        out[0] = (char)(out[0] - 32);
    out[blen] = '.';
    out[blen + 1] = '\0';
    return 1;
}

/* ---- persistence: explicit schema facts only (schema_kb.pl analog) ----
   Text, one pair of lines per schema:
     schema(<family>,<order>,[conns]).
     prov(<family>,[ids]).
   Cold semantics: only schema+prov survive; pairs/vocab/roles are
   working state and are NOT saved. */

static const char *OrderName(SCHEMA_ORDER o)
{
    switch (o)
    {
    case SCHEMA_ORDER_SYM: return "sym";
    case SCHEMA_ORDER_DEP_FIRST: return "dependent_first";
    case SCHEMA_ORDER_OPER_FIRST: return "operator_first";
    default: return "chain";
    }
}

uint32_t SchemaKBSave(const SCHEMA_KB *kb, const char *filepath)
{
    if (kb == NULL || filepath == NULL)
        return 0;
    FILE *f = fopen(filepath, "w");
    if (f == NULL)
        return 0;
    uint32_t written = 0;
    for (uint32_t i = 0; i < kb->num_schemas; i++)
    {
        const RELATIONAL_SCHEMA *s = &kb->schemas[i];
        fprintf(f, "schema(%s,%s,[", s->family, OrderName(s->order));
        for (uint32_t c = 0; c < s->num_connectives; c++)
        {
            char tmp[2 * SCHEMA_TOKEN_MAX];
            strncpy(tmp, s->connectives[c], sizeof(tmp) - 1);
            tmp[sizeof(tmp) - 1] = '\0';
            for (char *p = tmp; *p; p++)
                if (*p == ' ')
                    *p = '_';
            fprintf(f, "%s%s", c ? "," : "", tmp);
        }
        fprintf(f, "]).\n");
        fprintf(f, "prov(%s,[", s->family);
        for (uint32_t p = 0; p < s->num_prov; p++)
            fprintf(f, "%s%s", p ? "," : "", s->prov[p]);
        fprintf(f, "]).\n");
        written++;
    }
    fclose(f);
    return written;
}

static SCHEMA_ORDER OrderParse(const char *tok)
{
    if (strcmp(tok, "sym") == 0)
        return SCHEMA_ORDER_SYM;
    if (strcmp(tok, "dependent_first") == 0)
        return SCHEMA_ORDER_DEP_FIRST;
    if (strcmp(tok, "operator_first") == 0)
        return SCHEMA_ORDER_OPER_FIRST;
    return SCHEMA_ORDER_CHAIN;
}

/* read bracket list "a,b,..." into string slots */
static uint32_t ParseIdList(const char *list, char slots[][SCHEMA_TOKEN_MAX],
                            uint32_t max_slots)
{
    uint32_t n = 0;
    const char *tok = list;
    while (*tok && n < max_slots)
    {
        const char *comma = strchr(tok, ',');
        size_t len = comma ? (size_t)(comma - tok) : strlen(tok);
        if (len >= SCHEMA_TOKEN_MAX)
            len = SCHEMA_TOKEN_MAX - 1;
        slots[n][0] = '\0';
        strncat(slots[n], tok, len);
        n++;
        tok = comma ? comma + 1 : tok + strlen(tok);
    }
    return n;
}

uint32_t SchemaKBLoad(SCHEMA_KB *kb, const char *filepath)
{
    if (kb == NULL || filepath == NULL)
        return 0;
    FILE *f = fopen(filepath, "r");
    if (f == NULL)
        return 0;
    SchemaKBInit(kb);

    char line[1024];
    char conns[SCHEMA_CONN_MAX][SCHEMA_TOKEN_MAX];
    char ids[SCHEMA_PROV_MAX][SCHEMA_TOKEN_MAX];
    uint32_t loaded = 0;
    int have_pending = 0;
    char pending_family[SCHEMA_TOKEN_MAX] = "";
    SCHEMA_ORDER pending_order = SCHEMA_ORDER_CHAIN;
    char pending_conns[SCHEMA_CONN_MAX][SCHEMA_TOKEN_MAX];
    uint32_t pending_nconns = 0;

    while (fgets(line, sizeof(line), f))
    {
        if (strncmp(line, "schema(", 7) == 0)
        {
            /* schema(FAMILY,ORDER,[C1,C2]). */
            char *open = strchr(line, '(');
            char *close = strrchr(line, ')');
            if (open == NULL || close == NULL || close < open)
                continue;
            *close = '\0';
            char body[512];
            strncpy(body, open + 1, sizeof(body) - 1);
            body[sizeof(body) - 1] = '\0';

            char *c1 = strchr(body, ',');
            char *c2 = c1 ? strchr(c1 + 1, ',') : NULL;
            if (c1 == NULL || c2 == NULL)
                continue;
            *c1 = '\0';
            *c2 = '\0';
            strncpy(pending_family, body, SCHEMA_TOKEN_MAX - 1);
            pending_family[SCHEMA_TOKEN_MAX - 1] = '\0';
            pending_order = OrderParse(c1 + 1);

            char *list = c2 + 1;
            char *lb = strchr(list, '[');
            char *rb = strchr(list, ']');
            if (lb == NULL || rb == NULL || rb < lb)
                continue;
            *rb = '\0';
            pending_nconns = 0;
            char *tok = lb + 1;
            while (*tok && pending_nconns < SCHEMA_CONN_MAX)
            {
                char *comma = strchr(tok, ',');
                size_t len = comma ? (size_t)(comma - tok) : strlen(tok);
                if (len >= SCHEMA_TOKEN_MAX)
                    len = SCHEMA_TOKEN_MAX - 1;
                pending_conns[pending_nconns][0] = '\0';
                strncat(pending_conns[pending_nconns], tok, len);
                for (char *p = pending_conns[pending_nconns]; *p; p++)
                    if (*p == '_')
                        *p = ' ';
                pending_nconns++;
                tok = comma ? comma + 1 : tok + strlen(tok);
            }
            have_pending = 1;
        }
        else if (strncmp(line, "prov(", 5) == 0 && have_pending)
        {
            char *open = strchr(line, '(');
            char *close = strrchr(line, ')');
            if (open == NULL || close == NULL || close < open)
                continue;
            *close = '\0';
            char body[512];
            strncpy(body, open + 1, sizeof(body) - 1);
            body[sizeof(body) - 1] = '\0';
            char *comma = strchr(body, ',');
            if (comma == NULL)
                continue;
            *comma = '\0';
            /* body now holds the family of the prov line; must match */
            if (strcmp(body, pending_family) != 0)
                continue;
            char *list = comma + 1;
            char *lb = strchr(list, '[');
            char *rb = strchr(list, ']');
            if (lb == NULL || rb == NULL || rb < lb)
                continue;
            *rb = '\0';
            uint32_t nids = ParseIdList(lb + 1, ids, SCHEMA_PROV_MAX);

            if (kb->num_schemas < SCHEMA_MAX)
            {
                RELATIONAL_SCHEMA *s = &kb->schemas[kb->num_schemas++];
                memset(s, 0, sizeof(*s));
                strncpy(s->family, pending_family, SCHEMA_TOKEN_MAX - 1);
                s->family[SCHEMA_TOKEN_MAX - 1] = '\0';
                s->order = pending_order;
                s->num_connectives = pending_nconns;
                for (uint32_t c = 0; c < pending_nconns; c++)
                    strcpy(s->connectives[c], pending_conns[c]);
                s->num_prov = nids;
                for (uint32_t p = 0; p < nids; p++)
                    strcpy(s->prov[p], ids[p]);
                loaded++;
            }
            have_pending = 0;
            pending_family[0] = '\0';
        }
    }
    fclose(f);
    return loaded;
}