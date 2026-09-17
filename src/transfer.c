#include <stdio.h>
#include <string.h>
#include "transfer.h"

/* ============================================================
   Structural licensing.

   A schema with order dependent_first carries a DIRECTIONALITY
   constraint between two role axes: subject must be dependent,
   object independent. The SYMMETRIC meta-property licenses
   swapping those roles. The swap is admitted ONLY when both
   tokens hold exactly opposite roles on the same axis: the
   property then preserves the constraint's structure (one of
   each) under exchange. No word knowledge anywhere: roles are
   consulted in the KB's role lexicon, tokens are not matched
   by name.
   ============================================================ */

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

/* direction admission WITHOUT meta: mirrors schema.c OrderAdmits
   minus the meta layer (kept local to avoid widening schema.h) */
static int OrderAdmitsPlain(const SCHEMA_KB *kb, const RELATIONAL_SCHEMA *s,
                            const char *subj, const char *obj)
{
    if (PairEvidence(kb, s->family, subj, obj))
        return 1;
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
        return 0;
    }
}

/* swapped-direction admission under SYMMETRIC: both tokens must
   hold exactly opposite roles on one axis, so exchanging them
   preserves the structural constraint (one dependent + one
   independent / one operator + one patient stays true). */
static int SwapLicensed(const SCHEMA_KB *kb, const RELATIONAL_SCHEMA *s,
                        const char *subj, const char *obj)
{
    switch (s->order)
    {
    case SCHEMA_ORDER_DEP_FIRST:
        /* requested: subj=independent role, obj=dependent role */
        return RoleIs(kb, subj, 1) && RoleIs(kb, obj, 0);
    case SCHEMA_ORDER_OPER_FIRST:
        return RoleIs(kb, subj, 3) && RoleIs(kb, obj, 2);
    case SCHEMA_ORDER_SYM:
    case SCHEMA_ORDER_CHAIN:
    default:
        return 0; /* sym already admits both; chain stays fail-closed */
    }
}

static int VocabBoth(const SCHEMA_KB *kb, const char *a, const char *b)
{
    return SchemaVocabKnown(kb, a) && SchemaVocabKnown(kb, b);
}

/* TRANSITIVE derivation: family F with property transitive and
   observed links (A,B),(B,C) admits (A,C) — but only from PAIR
   EVIDENCE in this KB (the links are working state: after a wipe
   the middle must be re-presented as input). Returns 1 and fills
   out with the derived chain sentence. */
int TransferDeriveChain(const SCHEMA_KB *kb, const META_KB *mk,
                        const char *family, const char *subject,
                        const char *object, char *out, size_t out_size)
{
    if (kb == NULL || mk == NULL || out == NULL || out_size < 4)
        return 0;
    if (!MetaHasProperty(mk, family, META_PROP_TRANSITIVE))
        return 0;
    const RELATIONAL_SCHEMA *s = SchemaFindFamily(kb, family);
    if (s == NULL || s->num_connectives == 0)
        return 0;
    /* the transitive conclusion (S,O) needs the links (S,M)+(M,O)
       re-presented as input (pair evidence); it must NOT be
       derivable directly (that is the plain path) */
    if (PairEvidence(kb, family, subject, object))
        return 0;
    for (uint32_t i = 0; i < kb->num_pairs; i++)
    {
        const PAIR_EVID *p1 = &kb->pairs[i];
        if (strcmp(p1->family, family) != 0 ||
            strcmp(p1->subject, subject) != 0)
            continue;
        for (uint32_t j = 0; j < kb->num_pairs; j++)
        {
            const PAIR_EVID *p2 = &kb->pairs[j];
            if (strcmp(p2->family, family) != 0 ||
                strcmp(p2->object, object) != 0)
                continue;
            if (strcmp(p1->object, p2->subject) != 0)
                continue;
            if (!VocabBoth(kb, subject, object))
                return 0;
            return SchemaBuildSentence(kb, family, subject, object,
                                       out, out_size);
        }
    }
    return 0;
}

int TransferDerive(const SCHEMA_KB *kb, const META_KB *mk,
                   const char *family, const char *subject,
                   const char *object, char *out, size_t out_size)
{
    if (kb == NULL || out == NULL || out_size < 4)
        return 0;
    const RELATIONAL_SCHEMA *s = SchemaFindFamily(kb, family);
    if (s == NULL)
        return 0;
    if (!OrderAdmitsPlain(kb, s, subject, object))
        return 0;
    if (!VocabBoth(kb, subject, object))
        return 0;
    return SchemaBuildSentence(kb, family, subject, object, out, out_size);
}

int TransferDeriveSwapped(const SCHEMA_KB *kb, const META_KB *mk,
                          const char *family, const char *subject,
                          const char *object, char *out, size_t out_size)
{
    if (kb == NULL || mk == NULL || out == NULL || out_size < 4)
        return 0;
    const RELATIONAL_SCHEMA *s = SchemaFindFamily(kb, family);
    if (s == NULL)
        return 0;
    /* the swap must NOT already be admitted by the plain order check
       (otherwise TransferDerive is the right path, and calling this
       would double-license without meta evidence) */
    if (OrderAdmitsPlain(kb, s, subject, object))
        return 0;
    /* structural property must exist and license the exchange */
    if (!MetaHasProperty(mk, family, META_PROP_SYMMETRIC))
        return 0;
    if (!SwapLicensed(kb, s, subject, object))
        return 0;
    if (!VocabBoth(kb, subject, object))
        return 0;
    return SchemaBuildSentence(kb, family, subject, object, out, out_size);
}