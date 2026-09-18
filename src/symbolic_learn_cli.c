#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "learn.h"
#include "metaschema.h"
#include "transfer.h"

/* ============================================================
   symbolic-learn CLI: the first end-to-end C symbolic learning
   loop. ONE process, line-oriented: each stdin line is
   "<verb> <args...>". State lives in this process; two files
   hold the ONLY persistent structure:

     kb_schema.txt : schema facts (family, order, connectives, prov)
     kb_meta.txt   : meta facts (family, property, prov)

   Working state (exemplars, observations, vocab, pairs, roles)
   is NEVER saved: it dies at every wipe/exit. Verbs:

      learn <sentence>        observe a sentence as evidence
      file <path>             ingest "S<TAB>REL<TAB>O" lines from a file
      query <s> <conn> <o>    derive or UNKNOWN (plain, swapped, chain,
                              then every licensed composition rule)
      discover                meta + composition discovery over observations
      rules                   list licensed compose rules
      present <fam> <s> <o>   present a pair into the target world
      roles <tok> <role>      declare a role (world re-presentation)
      save / load / wipe      persistence + wipe-proof cycle
      counts                  working counters
      exit                    quit

   The WIPE-PROOF sequence (each line = one stdin line):
     learn speed proportional distance
     learn distance proportional speed
     discover
     save
     wipe            (exemplars+observations die; structure cold-loads)
     roles speed dependent / roles distance independent
     present proportionality speed distance
     present proportionality photon wave
     query photon proportional wave     -> OK
     query wave proportional photon     -> OK (SYMMETRIC swap)
     query electron proportional mass   -> UNKNOWN
   ============================================================ */

static const char *ConnToFamily(const char *conn)
{
    return LearnerConnFamily(conn);
}

static void PrintCounts(const SCHEMA_KB *kb, const META_KB *mk)
{
    printf("schemas=%u meta=%u obs=%u vocab=%u\n", SchemaCount(kb),
           MetaCount(mk), mk->num_obs, kb->num_vocab);
}

static void DoSave(const SCHEMA_KB *kb, const META_KB *mk)
{
    uint32_t a = SchemaKBSave(kb, "kb_schema.txt");
    uint32_t b = MetaKBSave(mk, "kb_meta.txt");
    printf("saved: schemas=%u metas=%u\n", a, b);
}

static void DoLoad(SCHEMA_KB *kb, META_KB *mk)
{
    uint32_t a = SchemaKBLoad(kb, "kb_schema.txt");
    uint32_t b = MetaKBLoad(mk, "kb_meta.txt");
    printf("loaded: schemas=%u metas=%u\n", a, b);
}

static void DoWipe(SCHEMA_KB *kb, META_KB *mk, LEARNER *lr)
{
    /* exemplars + observations die; structure reloads from disk */
    SchemaKBInit(kb);
    MetaKBInit(mk);
    LearnerInit(lr, kb, mk);
    SchemaKBLoad(kb, "kb_schema.txt");
    MetaKBLoad(mk, "kb_meta.txt");
    printf("wiped exemplars+observations; structure cold-loaded\n");
    PrintCounts(kb, mk);
}

static void CmdLearn(LEARNER *lr, const char *line)
{
    if (LearnerLearnLine(lr, line))
        printf("learned: %s (%s)\n", line,
               lr->last_was_exemplar ? "exemplar" : "observation-only");
    else
        printf("REJECTED: %s\n", line);
}

/* ingest: "S<TAB>REL<TAB>O" (bible TSV line) -> one learn call.
   REL tokens are Spanish bible relation names; the connective
   layer is the ONLY lexical knowledge, so the REL token is
   translated by the same consultable-table principle: this
   table is ingestion vocabulary, never asserted. */
static const char *GenericRelToConn(const char *rel, char *buf, size_t bsize)
{
    if (rel == NULL || rel[0] == '\0')
        return NULL;
    if (strcmp(rel, "HIJO_DE") == 0)
        return "isa";
    if (strcmp(rel, "REY_DE") == 0)
        return "reigns";
    if (strcmp(rel, "HERMANO_DE") == 0)
        return "sibling_of";
    if (strcmp(rel, "PADRE_DE") == 0)
        return "father_of";
    if (strcmp(rel, "ESPOSA_DE") == 0)
        return "wife_of";

    size_t len = strlen(rel);
    if (len > 3 && (strcmp(rel + len - 3, "_DE") == 0 || strcmp(rel + len - 3, "_de") == 0))
    {
        size_t stem_len = len - 3;
        if (stem_len + 4 < bsize)
        {
            for (size_t i = 0; i < stem_len; i++)
                buf[i] = (char)tolower((unsigned char)rel[i]);
            strcpy(buf + stem_len, "_of");
            return buf;
        }
    }
    if (len > 3 && (strcmp(rel + len - 3, "_OF") == 0 || strcmp(rel + len - 3, "_of") == 0))
    {
        size_t stem_len = len - 3;
        if (stem_len + 4 < bsize)
        {
            for (size_t i = 0; i < stem_len; i++)
                buf[i] = (char)tolower((unsigned char)rel[i]);
            strcpy(buf + stem_len, "_of");
            return buf;
        }
    }
    return NULL;
}

static void CmdIngest(LEARNER *lr, const char *line)
{
    char s[64], rel[64], o[64];
    if (sscanf(line, "%63s %63s %63s", s, rel, o) != 3)
    {
        printf("INGEST-REJECTED: %s\n", line);
        return;
    }
    char conn_buf[64];
    const char *conn = GenericRelToConn(rel, conn_buf, sizeof(conn_buf));
    if (conn == NULL && LearnerConnFamily(rel) != NULL)
        conn = rel;
    if (conn == NULL)
    {
        printf("INGEST-SKIP: %s (relation not mappable)\n", rel);
        return;
    }
    if (strcmp(s, o) == 0)
    {
        printf("INGEST-SKIP: %s %s %s (self-loop)\n", s, rel, o);
        return;
    }
    char sent[LEARN_MAX_LINE];
    snprintf(sent, sizeof(sent), "%s %s %s", s, conn, o);
    if (LearnerLearnLine(lr, sent))
    {
        if (lr->last_was_exemplar)
            printf("ingested: %s %s %s\n", s, conn, o);
    }
    else
        printf("INGEST-REJECTED: %s %s %s\n", s, conn, o);
}

static void CmdDiscover(LEARNER *lr)
{
    uint32_t metas = LearnerDiscoverMeta(lr);
    uint32_t rules = MetaRuleDiscover(lr->mk);
    printf("discovered %u new meta properties, %u new compose rules\n",
           metas, rules);
}

static void CmdRules(META_KB *mk)
{
    if (mk->num_rules == 0)
    {
        printf("rules: 0 licensed\n");
        return;
    }
    printf("rules:\n");
    for (uint32_t i = 0; i < mk->num_rules; i++)
        printf("  %s o %s => %s (support %u)\n", mk->rules[i].r1,
               mk->rules[i].r2, mk->rules[i].r3, mk->rules[i].support);
}

/* ingest "S<TAB>REL<TAB>O" lines from a file (learn file <path>) */
static void CmdFile(LEARNER *lr, const char *path)
{
    FILE *f = fopen(path, "r");
    if (f == NULL)
    {
        printf("cannot open '%s'\n", path);
        return;
    }
    char line[LEARN_MAX_LINE];
    uint32_t learned = 0, skipped = 0;
    while (fgets(line, sizeof(line), f) != NULL)
    {
        size_t len = strlen(line);
        while (len && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';
        if (len == 0)
            continue;
        char buf[LEARN_MAX_LINE];
        strncpy(buf, line, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        char *t1 = strchr(buf, '\t');
        char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
        if (t1 == NULL || t2 == NULL)
        {
            skipped++;
            continue;
        }
        *t1 = '\0';
        *t2 = '\0';
        const char *rel = t1 + 1;
        const char *obj = t2 + 1;
        char conn_buf[64];
        const char *conn = GenericRelToConn(rel, conn_buf, sizeof(conn_buf));
        if (conn == NULL && LearnerConnFamily(rel) != NULL)
            conn = rel; /* already a connective; validated by the
                           consultable table, never hardcoded here */
        if (conn == NULL || strcmp(buf, obj) == 0)
        {
            skipped++;
            continue;
        }
        char sent[LEARN_MAX_LINE];
        snprintf(sent, sizeof(sent), "%s %s %s", buf, conn, obj);
        if (LearnerLearnLine(lr, sent))
            learned++;
        else
            skipped++;
    }
    fclose(f);
    printf("file: %u learned, %u skipped from '%s'\n", learned, skipped,
           path);
}

static void CmdPresent(LEARNER *lr, char *args)
{
    char fam[64], s[64], o[64];
    if (sscanf(args, "%63s %63s %63s", fam, s, o) == 3)
        printf("present: %s\n",
               LearnerPresentPair(lr, fam, s, o) ? "OK" : "REJECTED");
    else
        printf("usage: present <family> <subj> <obj>\n");
}

static void CmdRoles(SCHEMA_KB *kb, char *args)
{
    char tok[64], role[64];
    if (sscanf(args, "%63s %63s", tok, role) != 2)
    {
        printf("usage: roles <token> <dependent|independent|operator|patient>\n");
        return;
    }
    if (strcmp(role, "dependent") == 0)
        SchemaDeclareRole(kb, tok, 1, 0, 0, 0);
    else if (strcmp(role, "independent") == 0)
        SchemaDeclareRole(kb, tok, 0, 1, 0, 0);
    else if (strcmp(role, "operator") == 0)
        SchemaDeclareRole(kb, tok, 0, 0, 1, 0);
    else if (strcmp(role, "patient") == 0)
        SchemaDeclareRole(kb, tok, 0, 0, 0, 1);
    else
    {
        printf("unknown role: %s\n", role);
        return;
    }
    printf("role declared: %s = %s\n", tok, role);
}

static void CmdQuery(const SCHEMA_KB *kb, const META_KB *mk, char *args)
{
    char subj[64], conn[32], obj[64];
    if (sscanf(args, "%63s %31s %63s", subj, conn, obj) != 3)
    {
        printf("usage: query <subj> <connective> <obj>\n");
        return;
    }
    const char *family = ConnToFamily(conn);
    if (family == NULL)
    {
        printf("UNKNOWN\n");
        return;
    }
    char out[128];
    if (TransferDerive(kb, mk, family, subj, obj, out, sizeof(out)) ||
        TransferDeriveSwapped(kb, mk, family, subj, obj, out, sizeof(out)) ||
        TransferDeriveChain(kb, mk, family, subj, obj, out, sizeof(out)))
    {
        printf("OK: %s\n", out);
        return;
    }
    /* heterogeneous composition: every licensed rule with r3 == this
       family is a derivation path whose premises must be observed
       pair evidence (ComposeGate fails closed) */
    for (uint32_t i = 0; i < mk->num_rules; i++)
    {
        const META_RULE *r = &mk->rules[i];
        if (strcmp(r->r3, family) != 0)
            continue;
        if (TransferCompose(kb, mk, r->r1, r->r2, subj, obj, obj, out,
                            sizeof(out)))
        {
            printf("OK: %s\n", out);
            return;
        }
    }
    printf("UNKNOWN\n");
}

int main(int argc, char **argv)
{
    if (argc > 1)
    {
        printf("usage: symbolic-learn  (verbs on stdin: learn|file|ingest|"
               "query|discover|rules|present|roles|save|load|wipe|counts|"
               "exit)\n");
        return 2;
    }

    SCHEMA_KB kb;
    META_KB mk;
    LEARNER lr;
    SchemaKBInit(&kb);
    MetaKBInit(&mk);
    LearnerInit(&lr, &kb, &mk);

    char line[LEARN_MAX_LINE];
    while (fgets(line, sizeof(line), stdin))
    {
        size_t len = strlen(line);
        while (len && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';
        if (len == 0)
            continue;

        /* verb = first token; args = rest */
        char *sp = line;
        while (*sp && *sp != ' ')
            sp++;
        char *args = sp;
        while (*args == ' ')
            args++;
        *sp = '\0';
        char *verb = line;

        if (strcmp(verb, "exit") == 0 || strcmp(verb, "quit") == 0)
            break;
        else if (strcmp(verb, "learn") == 0)
            CmdLearn(&lr, args);
        else if (strcmp(verb, "ingest") == 0)
            CmdIngest(&lr, args);
        else if (strcmp(verb, "file") == 0)
            CmdFile(&lr, args);
        else if (strcmp(verb, "discover") == 0)
            CmdDiscover(&lr);
        else if (strcmp(verb, "rules") == 0)
            CmdRules(&mk);
        else if (strcmp(verb, "present") == 0)
            CmdPresent(&lr, args);
        else if (strcmp(verb, "roles") == 0)
            CmdRoles(&kb, args);
        else if (strcmp(verb, "query") == 0)
            CmdQuery(&kb, &mk, args);
        else if (strcmp(verb, "save") == 0)
            DoSave(&kb, &mk);
        else if (strcmp(verb, "load") == 0)
            DoLoad(&kb, &mk);
        else if (strcmp(verb, "wipe") == 0)
            DoWipe(&kb, &mk, &lr);
        else if (strcmp(verb, "counts") == 0)
            PrintCounts(&kb, &mk);
        else
            printf("unknown verb: %s\n", verb);
    }
    return 0;
}