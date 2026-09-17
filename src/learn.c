#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "learn.h"

/* ============================================================
   Derivation rules keyed by the OBSERVED surface connective.
   No word is hardcoded in the structural layers above: these
   two tables are the only lexical knowledge, and they are
   consulted (never asserted) — same status as the template
   registry in schema.c.
   ============================================================ */

static int IsConnectiveTok(const char *tok)
{
    return strcmp(tok, "cong") == 0 || strcmp(tok, "proportional") == 0 ||
           strcmp(tok, "after") == 0 || strcmp(tok, "acting_on") == 0 ||
           strcmp(tok, "acting") == 0 || strcmp(tok, "isa") == 0 ||
           strcmp(tok, "sibling_of") == 0 || strcmp(tok, "father_of") == 0 ||
           strcmp(tok, "reigns") == 0 || strcmp(tok, "wife_of") == 0;
}

static const char *FamilyOfConnective(const char *conn)
{
    if (strcmp(conn, "cong") == 0)
        return "equivalence";
    if (strcmp(conn, "proportional") == 0)
        return "proportionality";
    if (strcmp(conn, "after") == 0)
        return "succession";
    if (strcmp(conn, "acting_on") == 0 || strcmp(conn, "acting") == 0)
        return "application";
    if (strcmp(conn, "isa") == 0)
        return "taxonomy";
    if (strcmp(conn, "sibling_of") == 0)
        return "sibling";
    if (strcmp(conn, "father_of") == 0)
        return "father";
    if (strcmp(conn, "reigns") == 0)
        return "reigns";
    if (strcmp(conn, "wife_of") == 0)
        return "wife";
    return NULL;
}

void LearnerInit(LEARNER *lr, SCHEMA_KB *kb, META_KB *mk)
{
    if (lr == NULL)
        return;
    memset(lr, 0, sizeof(*lr));
    lr->kb = kb;
    lr->mk = mk;
}

/* split on whitespace, lowercase; fills toks[]; returns count */
static uint32_t SplitTokens(const char *line, char toks[][32],
                            uint32_t max_toks)
{
    uint32_t n = 0;
    const char *p = line;
    while (*p && n < max_toks)
    {
        while (*p && isspace((unsigned char)*p))
            p++;
        if (!*p)
            break;
        const char *start = p;
        while (*p && !isspace((unsigned char)*p))
            p++;
        size_t len = (size_t)(p - start);
        if (len >= 32)
            len = 31;
        for (size_t i = 0; i < len; i++)
            toks[n][i] = (char)tolower((unsigned char)start[i]);
        toks[n][len] = '\0';
        n++;
    }
    return n;
}

int LearnerLearnLine(LEARNER *lr, const char *line)
{
    if (lr == NULL || line == NULL)
        return 0;
    char toks[8][32];
    uint32_t n = SplitTokens(line, toks, 8);
    if (n < 3 || n > 5)
        return 0;

    uint32_t subj_pos = 0, obj_pos = 0;

    if (n == 3)
    {
        if (!IsConnectiveTok(toks[1]))
            return 0;
        subj_pos = 0;
        obj_pos = 2;
    }
    else if (n == 4)
    {
        if (!IsConnectiveTok(toks[1]))
            return 0;
        subj_pos = 0;
        obj_pos = 3;
    }
    else /* n == 5: "S acting on O" */
    {
        if (!(strcmp(toks[1], "acting") == 0 && strcmp(toks[2], "on") == 0))
            return 0;
        subj_pos = 0;
        obj_pos = 4;
    }

    const char *conn = (n == 5) ? "acting_on" : toks[1];
    const char *family = FamilyOfConnective(conn);
    if (family == NULL)
        return 0;

    const char *ex_triple[3];
    ex_triple[0] = toks[subj_pos];
    ex_triple[1] = family;
    ex_triple[2] = toks[obj_pos];

    const char *sentence[5];
    for (uint32_t i = 0; i < n; i++)
        sentence[i] = toks[i];

    char obs_id[32];
    snprintf(obs_id, sizeof(obs_id), "obs-%u", lr->next_obs);
    lr->next_obs++;

    int taught = SchemaObserveExemplar(lr->kb, obs_id, ex_triple, 3,
                                       sentence, n);
    if (taught)
    {
        /* vocabulary + pair evidence from the exemplar itself */
        SchemaPresentPair(lr->kb, family, ex_triple[0], ex_triple[2]);
    }
    MetaObserve(lr->mk, family, ex_triple[0], ex_triple[2], obs_id);

    lr->last_was_exemplar = taught;
    strncpy(lr->last_family, family, SCHEMA_TOKEN_MAX - 1);
    lr->last_family[SCHEMA_TOKEN_MAX - 1] = '\0';
    return 1;
}

uint32_t LearnerDiscoverMeta(LEARNER *lr)
{
    if (lr == NULL)
        return 0;
    return MetaDiscover(lr->mk);
}

int LearnerPresentPair(LEARNER *lr, const char *family,
                       const char *subject, const char *object)
{
    if (lr == NULL)
        return 0;
    return SchemaPresentPair(lr->kb, family, subject, object);
}