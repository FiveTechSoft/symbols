#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "metaschema.h"

/* ---- lifecycle ---- */

void MetaKBInit(META_KB *mk)
{
    if (mk == NULL)
        return;
    memset(mk, 0, sizeof(*mk));
}

uint32_t MetaCount(const META_KB *mk)
{
    return mk == NULL ? 0 : mk->num_metas;
}

uint32_t MetaRuleCount(const META_KB *mk)
{
    return mk == NULL ? 0 : mk->num_rules;
}

/* ---- observation ---- */

static META_SCHEMA *MetaFind(META_KB *mk, const char *family)
{
    for (uint32_t i = 0; i < mk->num_metas; i++)
        if (strcmp(mk->metas[i].family, family) == 0)
            return &mk->metas[i];
    return NULL;
}

int MetaObserve(META_KB *mk, const char *family, const char *subject,
                const char *object, const char *obs_id)
{
    if (mk == NULL || family == NULL || subject == NULL || object == NULL ||
        obs_id == NULL)
        return 0;
    /* idempotent: the same (family,S,O) repetition is one observation;
       obs_id (provenance) stays that of the FIRST sighting */
    for (uint32_t i = 0; i < mk->num_obs; i++)
    {
        const META_OBS *p = &mk->obs[i];
        if (strcmp(p->family, family) == 0 &&
            strcmp(p->subject, subject) == 0 &&
            strcmp(p->object, object) == 0)
            return 1;
    }
    if (mk->num_obs >= META_OBS_MAX)
        return 0;
    META_OBS *o = &mk->obs[mk->num_obs];
    strncpy(o->family, family, SCHEMA_TOKEN_MAX - 1);
    o->family[SCHEMA_TOKEN_MAX - 1] = '\0';
    strncpy(o->subject, subject, SCHEMA_TOKEN_MAX - 1);
    o->subject[SCHEMA_TOKEN_MAX - 1] = '\0';
    strncpy(o->object, object, SCHEMA_TOKEN_MAX - 1);
    o->object[SCHEMA_TOKEN_MAX - 1] = '\0';
    strncpy(o->obs_id, obs_id, SCHEMA_TOKEN_MAX - 1);
    o->obs_id[SCHEMA_TOKEN_MAX - 1] = '\0';
    mk->num_obs++;
    return 1;
}

/* ---- discovery: structural only ---- */

/* ObsEqual on subject+object with object wildcard support for
   the transitive scan (matches any object) */
static int ObsEqual(const META_OBS *o, const char *family,
                    const char *s, const char *ob)
{
    if (strcmp(o->family, family) != 0 || strcmp(o->subject, s) != 0)
        return 0;
    if (strcmp(ob, "*") == 0)
        return 1;
    return strcmp(o->object, ob) == 0;
}

/* family F observed as (A,B) and (B,A) with A!=B -> symmetric */
static int FamilySymmetricEvidence(const META_KB *mk, const char *family,
                                   const char **prov1, const char **prov2)
{
    for (uint32_t i = 0; i < mk->num_obs; i++)
    {
        const META_OBS *o1 = &mk->obs[i];
        if (strcmp(o1->family, family) != 0)
            continue;
        if (strcmp(o1->subject, o1->object) == 0)
            continue; /* (A,A) says nothing about symmetry */
        for (uint32_t j = 0; j < mk->num_obs; j++)
        {
            const META_OBS *o2 = &mk->obs[j];
            if (j == i)
                continue;
            if (ObsEqual(o2, family, o1->object, o1->subject))
            {
                *prov1 = o1->obs_id;
                *prov2 = o2->obs_id;
                return 1;
            }
        }
    }
    return 0;
}

/* family F observed as (A,B) AND (B,C) with A!=C -> transitive.
   Both links must be distinct observations; B never A or C. */
static int FamilyTransitiveEvidence(const META_KB *mk, const char *family,
                                    const char **prov1, const char **prov2)
{
    for (uint32_t i = 0; i < mk->num_obs; i++)
    {
        const META_OBS *link1 = &mk->obs[i];
        if (strcmp(link1->family, family) != 0)
            continue;
        if (strcmp(link1->subject, link1->object) == 0)
            continue; /* (A,A) is a degenerate link */
        for (uint32_t j = 0; j < mk->num_obs; j++)
        {
            const META_OBS *link2 = &mk->obs[j];
            if (j == i)
                continue;
            if (!ObsEqual(link2, family, link1->object, "*"))
                continue; /* link2 = (B,C): middle must match link1.object */
            if (strcmp(link2->object, link2->subject) == 0)
                continue; /* (B,B) degenerate */
            if (strcmp(link2->object, link1->subject) == 0)
                continue; /* (A,B)+(B,A) is symmetry evidence, not a chain */
            *prov1 = link1->obs_id;
            *prov2 = link2->obs_id;
            return 1;
        }
    }
    return 0;
}

uint32_t MetaDiscover(META_KB *mk)
{
    if (mk == NULL)
        return 0;
    uint32_t found = 0;
    /* one classification pass per distinct family: a family can
       hold several structural properties (real corpora show
       symmetric AND transitive evidence in the same links), so
       the already-classified guard is per (family, property),
       not per family. */
    for (int pass = 0; pass < 2; pass++)
    {
        for (uint32_t i = 0; i < mk->num_obs; i++)
        {
            const char *family = mk->obs[i].family;
            META_PROPERTY wanted = (pass == 0) ? META_PROP_SYMMETRIC
                                               : META_PROP_TRANSITIVE;
            if (MetaHasProperty(mk, family, wanted))
                continue; /* this property already classified */

            const char *p1 = NULL, *p2 = NULL;
            META_PROPERTY prop = META_PROP_NONE;
            if (wanted == META_PROP_SYMMETRIC)
            {
                if (FamilySymmetricEvidence(mk, family, &p1, &p2))
                    prop = META_PROP_SYMMETRIC;
            }
            else if (FamilyTransitiveEvidence(mk, family, &p1, &p2))
            {
                prop = META_PROP_TRANSITIVE;
            }
            if (prop == META_PROP_NONE)
                continue; /* absence of evidence: no hypothesis */

            if (mk->num_metas >= META_MAX)
                break;
            META_SCHEMA *m = &mk->metas[mk->num_metas++];
            memset(m, 0, sizeof(*m));
            strncpy(m->family, family, SCHEMA_TOKEN_MAX - 1);
            m->family[SCHEMA_TOKEN_MAX - 1] = '\0';
            m->property = prop;
            m->num_prov = 2;
            strncpy(m->prov[0], p1, SCHEMA_TOKEN_MAX - 1);
            m->prov[0][SCHEMA_TOKEN_MAX - 1] = '\0';
            strncpy(m->prov[1], p2, SCHEMA_TOKEN_MAX - 1);
            m->prov[1][SCHEMA_TOKEN_MAX - 1] = '\0';
            found++;
        }
        /* NO early break: pass 0 (SYMMETRIC) may find nothing while
            pass 1 (TRANSITIVE) still has work (a pure chain corpus
            has no 2-cycles). Idempotency comes from the per
            (family, property) guard above, not from the break. */
    }
    return found;
}

/* ---- query ---- */

int MetaHasProperty(const META_KB *mk, const char *family,
                    META_PROPERTY prop)
{
    if (mk == NULL || family == NULL || prop == META_PROP_NONE)
        return 0;
    for (uint32_t i = 0; i < mk->num_metas; i++)
    {
        const META_SCHEMA *m = &mk->metas[i];
        if (strcmp(m->family, family) == 0 && m->property == prop)
            return 1;
    }
    return 0;
}

/* ---- composition discovery (Phase 3) ---- */

static int MetaFindRuleIdx(const META_KB *mk, const char *r1,
                           const char *r2)
{
    for (uint32_t i = 0; i < mk->num_rules; i++)
        if (strcmp(mk->rules[i].r1, r1) == 0 &&
            strcmp(mk->rules[i].r2, r2) == 0)
            return (int)i;
    return -1;
}

int MetaFindRule(const META_KB *mk, const char *r1, const char *r2,
                 META_RULE *rule)
{
    if (mk == NULL || r1 == NULL || r2 == NULL || rule == NULL)
        return 0;
    int i = MetaFindRuleIdx(mk, r1, r2);
    if (i < 0)
        return 0;
    *rule = mk->rules[i];
    return 1;
}

/* premise instance enumeration: (R1(A,B), R2(B,C)) observed with
   A != C. Observations are deduped per (family,S,O), so each
   (i,j) index pair with matching middle IS one distinct premise
   instance — no consumption/mutation needed, links may serve
   several instances (census-faithful: premise count counts
   distinct (A,B,C) triples). */
static uint32_t ComposeTally(const META_KB *mk, const char *r1,
                             const char *r2, const char *r3,
                             uint32_t *confirm)
{
    *confirm = 0;
    uint32_t prem = 0;
    for (uint32_t i = 0; i < mk->num_obs; i++)
    {
        const META_OBS *o1 = &mk->obs[i];
        if (strcmp(o1->family, r1) != 0)
            continue;
        if (strcmp(o1->subject, o1->object) == 0)
            continue; /* (A,A) degenerate */
        for (uint32_t j = 0; j < mk->num_obs; j++)
        {
            const META_OBS *o2 = &mk->obs[j];
            if (strcmp(o2->family, r2) != 0)
                continue;
            if (strcmp(o2->subject, o1->object) != 0)
                continue; /* middle B must match */
            if (strcmp(o2->subject, o2->object) == 0)
                continue; /* (B,B) degenerate */
            if (strcmp(o2->object, o1->subject) == 0)
                continue; /* A == C: cycle, says nothing new */
            prem++;
            int confirmed = 0;
            for (uint32_t k = 0; k < mk->num_obs; k++)
            {
                const META_OBS *o3 = &mk->obs[k];
                if (strcmp(o3->family, r3) != 0)
                    continue;
                if (strcmp(o3->subject, o1->subject) == 0 &&
                    strcmp(o3->object, o2->object) == 0)
                {
                    confirmed = 1;
                    break;
                }
            }
            if (!confirmed)
                return prem; /* gate already failed: rate < 1.0 */
            (*confirm)++;
        }
    }
    return prem;
}

uint32_t MetaRuleDiscover(META_KB *mk)
{
    if (mk == NULL)
        return 0;
    uint32_t found = 0;
    /* candidate premise families: distinct observed families */
    char fams[META_OBS_MAX][SCHEMA_TOKEN_MAX];
    uint32_t nfam = 0;
    for (uint32_t i = 0; i < mk->num_obs; i++)
    {
        int dup = 0;
        for (uint32_t f = 0; f < nfam; f++)
            if (strcmp(fams[f], mk->obs[i].family) == 0)
                dup = 1;
        if (!dup && nfam < META_OBS_MAX)
            strcpy(fams[nfam++], mk->obs[i].family);
    }
    for (uint32_t a = 0; a < nfam; a++)
    {
        for (uint32_t b = 0; b < nfam; b++)
        {
            const char *r1 = fams[a];
            const char *r2 = fams[b];
            if (strcmp(r1, r2) == 0)
                continue; /* homogeneous: TRANSITIVE's territory */
            if (MetaFindRuleIdx(mk, r1, r2) >= 0)
                continue; /* idempotent */
            /* candidate r3 = every other observed family (copied:
               ComposeTally mutates obs while scanning) */
            for (uint32_t c = 0; c < nfam; c++)
            {
                char r3[SCHEMA_TOKEN_MAX];
                strcpy(r3, fams[c]);
                if (strcmp(r3, r1) == 0 || strcmp(r3, r2) == 0)
                    continue;
                if (MetaFindRuleIdx(mk, r1, r2) >= 0)
                    break; /* a sibling r3 already licensed */
                uint32_t confirm;
                uint32_t prem = ComposeTally(mk, r1, r2, r3, &confirm);
                if (prem < 2 || confirm != prem)
                    continue; /* gate: rate 1.0 AND support >= 2 */
                if (mk->num_rules >= META_RULE_MAX)
                    return found;
                META_RULE *r = &mk->rules[mk->num_rules++];
                memset(r, 0, sizeof(*r));
                strncpy(r->r1, r1, SCHEMA_TOKEN_MAX - 1);
                r->r1[SCHEMA_TOKEN_MAX - 1] = '\0';
                strncpy(r->r2, r2, SCHEMA_TOKEN_MAX - 1);
                r->r2[SCHEMA_TOKEN_MAX - 1] = '\0';
                strncpy(r->r3, r3, SCHEMA_TOKEN_MAX - 1);
                r->r3[SCHEMA_TOKEN_MAX - 1] = '\0';
                r->support = confirm;
                found++;
            }
        }
    }
    return found;
}

static const char *PropName(META_PROPERTY p)
{
    if (p == META_PROP_SYMMETRIC)
        return "symmetric";
    if (p == META_PROP_TRANSITIVE)
        return "transitive";
    return "none";
}

static META_PROPERTY PropParse(const char *tok)
{
    if (strcmp(tok, "symmetric") == 0)
        return META_PROP_SYMMETRIC;
    if (strcmp(tok, "transitive") == 0)
        return META_PROP_TRANSITIVE;
    return META_PROP_NONE;
}

uint32_t MetaKBSave(const META_KB *mk, const char *filepath)
{
    if (mk == NULL || filepath == NULL)
        return 0;
    FILE *f = fopen(filepath, "w");
    if (f == NULL)
        return 0;
    uint32_t written = 0;
    for (uint32_t i = 0; i < mk->num_metas; i++)
    {
        const META_SCHEMA *m = &mk->metas[i];
        fprintf(f, "meta(%s,%s).\n", m->family, PropName(m->property));
        fprintf(f, "metaprov(%s,[", m->family);
        for (uint32_t p = 0; p < m->num_prov; p++)
            fprintf(f, "%s%s", p ? "," : "", m->prov[p]);
        fprintf(f, "]).\n");
        written++;
    }
    for (uint32_t i = 0; i < mk->num_rules; i++)
    {
        const META_RULE *r = &mk->rules[i];
        fprintf(f, "compose(%s,%s,%s,%u).\n", r->r1, r->r2, r->r3,
                r->support);
        written++;
    }
    fclose(f);
    return written;
}

uint32_t MetaKBLoad(META_KB *mk, const char *filepath)
{
    if (mk == NULL || filepath == NULL)
        return 0;
    FILE *f = fopen(filepath, "r");
    if (f == NULL)
        return 0;
    MetaKBInit(mk);
    char line[1024];
    uint32_t loaded = 0;
    int have_pending = 0;
    char pending_family[SCHEMA_TOKEN_MAX] = "";
    META_PROPERTY pending_prop = META_PROP_NONE;
    char ids[SCHEMA_PROV_MAX][SCHEMA_TOKEN_MAX];

    while (fgets(line, sizeof(line), f))
    {
        if (strncmp(line, "meta(", 5) == 0)
        {
            char *open = strchr(line, '(');
            char *close = strrchr(line, ')');
            if (open == NULL || close == NULL || close < open)
                continue;
            *close = '\0';
            char *comma = strchr(open + 1, ',');
            if (comma == NULL)
                continue;
            *comma = '\0';
            strncpy(pending_family, open + 1, SCHEMA_TOKEN_MAX - 1);
            pending_family[SCHEMA_TOKEN_MAX - 1] = '\0';
            pending_prop = PropParse(comma + 1);
            have_pending = 1;
        }
        else if (strncmp(line, "metaprov(", 9) == 0 && have_pending)
        {
            char *open = strchr(line, '(');
            char *close = strrchr(line, ')');
            if (open == NULL || close == NULL || close < open)
                continue;
            *close = '\0';
            char *comma = strchr(open + 1, ',');
            if (comma == NULL)
                continue;
            *comma = '\0';
            if (strcmp(open + 1, pending_family) != 0)
                continue;
            char *list = comma + 1;
            char *lb = strchr(list, '[');
            char *rb = strchr(list, ']');
            if (lb == NULL || rb == NULL || rb < lb)
                continue;
            *rb = '\0';
            uint32_t n = 0;
            const char *tok = lb + 1;
            while (*tok && n < SCHEMA_PROV_MAX)
            {
                const char *c2 = strchr(tok, ',');
                size_t len = c2 ? (size_t)(c2 - tok) : strlen(tok);
                if (len >= SCHEMA_TOKEN_MAX)
                    len = SCHEMA_TOKEN_MAX - 1;
                ids[n][0] = '\0';
                strncat(ids[n], tok, len);
                n++;
                tok = c2 ? c2 + 1 : tok + strlen(tok);
            }
            if (mk->num_metas < META_MAX)
            {
                META_SCHEMA *m = &mk->metas[mk->num_metas++];
                memset(m, 0, sizeof(*m));
                strncpy(m->family, pending_family, SCHEMA_TOKEN_MAX - 1);
                m->family[SCHEMA_TOKEN_MAX - 1] = '\0';
                m->property = pending_prop;
                m->num_prov = n;
                for (uint32_t p = 0; p < n; p++)
                    strcpy(m->prov[p], ids[p]);
                loaded++;
            }
            have_pending = 0;
            pending_family[0] = '\0';
        }
        else if (strncmp(line, "compose(", 8) == 0)
        {
            char *open = strchr(line, '(');
            char *close = strrchr(line, ')');
            if (open == NULL || close == NULL || close < open)
                continue;
            *close = '\0';
            char *c1 = strchr(open + 1, ',');
            char *c2 = c1 ? strchr(c1 + 1, ',') : NULL;
            char *c3 = c2 ? strchr(c2 + 1, ',') : NULL;
            if (c1 == NULL || c2 == NULL || c3 == NULL)
                continue;
            *c1 = '\0';
            *c2 = '\0';
            *c3 = '\0';
            if (mk->num_rules >= META_RULE_MAX)
                continue;
            META_RULE *r = &mk->rules[mk->num_rules++];
            memset(r, 0, sizeof(*r));
            strncpy(r->r1, open + 1, SCHEMA_TOKEN_MAX - 1);
            r->r1[SCHEMA_TOKEN_MAX - 1] = '\0';
            strncpy(r->r2, c1 + 1, SCHEMA_TOKEN_MAX - 1);
            r->r2[SCHEMA_TOKEN_MAX - 1] = '\0';
            strncpy(r->r3, c2 + 1, SCHEMA_TOKEN_MAX - 1);
            r->r3[SCHEMA_TOKEN_MAX - 1] = '\0';
            r->support = (uint32_t)strtoul(c3 + 1, NULL, 10);
            loaded++;
        }
    }
    fclose(f);
    return loaded;
}