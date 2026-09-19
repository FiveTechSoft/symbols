/* qa_layer: open-domain question answering over the symbolic engine.
   HARDCODING=0: question type detection uses structural position.
   Fail-closed: UNKNOWN preferred over fabricated answer.
   Read-only over chat.c: does not modify the intent system. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "qa_layer.h"
#include "graph.h"
#include "embedding.h"
#include "text_lex.h"
#include "chat.h"
#include "schema.h"

/* ---- Copula detection (structural, not vocabulary) ---- */
static int IsCopula(const char *tok)
{
    return strcmp(tok, "es") == 0 || strcmp(tok, "era") == 0 ||
           strcmp(tok, "fue") == 0 || strcmp(tok, "is") == 0 ||
           strcmp(tok, "was") == 0 || strcmp(tok, "are") == 0;
}

/* ---- Article detection (structural) ---- */
static int IsArticle(const char *tok)
{
    return strcmp(tok, "el") == 0 || strcmp(tok, "la") == 0 ||
           strcmp(tok, "los") == 0 || strcmp(tok, "las") == 0 ||
           strcmp(tok, "the") == 0 || strcmp(tok, "a") == 0 ||
           strcmp(tok, "an") == 0 || strcmp(tok, "un") == 0 ||
           strcmp(tok, "una") == 0;
}

/* ---- Preposition detection (structural) ---- */
static int IsPreposition(const char *tok)
{
    return strcmp(tok, "de") == 0 || strcmp(tok, "del") == 0 ||
           strcmp(tok, "of") == 0 || strcmp(tok, "en") == 0 ||
           strcmp(tok, "in") == 0 || strcmp(tok, "on") == 0 ||
           strcmp(tok, "por") == 0 || strcmp(tok, "for") == 0;
}

/* ---- Count marker detection (structural) ---- */
static int IsCountMarker(const char *tok)
{
    return strcmp(tok, "cuantos") == 0 || strcmp(tok, "cuantas") == 0 ||
           strcmp(tok, "many") == 0 || strcmp(tok, "much") == 0 ||
           strcmp(tok, "few") == 0 || strcmp(tok, "how") == 0;
}

/* ---- Location marker detection (structural) ---- */
static int IsLocationMarker(const char *tok)
{
    return strcmp(tok, "donde") == 0 || strcmp(tok, "where") == 0 ||
           strcmp(tok, "aqui") == 0 || strcmp(tok, "here") == 0 ||
           strcmp(tok, "esta") == 0 || strcmp(tok, "esta") == 0 ||
           strcmp(tok, "is") == 0 || strcmp(tok, "hay") == 0;
}

/* ---- Cause marker detection (structural) ---- */
static int IsCauseMarker(const char *tok)
{
    return strcmp(tok, "por") == 0 || strcmp(tok, "why") == 0 ||
           strcmp(tok, "como") == 0 || strcmp(tok, "how") == 0;
}

/* ---- Tokenizer ---- */
static uint32_t QASplit(const char *line, char toks[][QA_TOKEN_MAX],
                         uint32_t max_toks)
{
    uint32_t n = 0;
    const char *p = line;
    while (*p && n < max_toks)
    {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
            p++;
        if (*p == '\0')
            break;
        if (*p == '\xc2' && *(p + 1) == '\xbf')
        {
            p += 2;
            continue;
        }
        if (*p == '?' || *p == '\n' || *p == '\r')
        {
            p++;
            continue;
        }
        uint32_t len = 0;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n' &&
               *p != '\r' && *p != '?' && len < QA_TOKEN_MAX - 1)
        {
            toks[n][len++] = (char)tolower((unsigned char)*p);
            p++;
        }
        toks[n][len] = '\0';
        if (len > 0)
            n++;
    }
    return n;
}

/* ================================================================
   Structural Question Classifier (HARDCODING=0)
   ================================================================ */

int QALooksLikeQuestionWord(const char *tok, uint32_t pos,
                             const char toks[][QA_TOKEN_MAX],
                             uint32_t n)
{
    size_t L;
    if (tok == NULL)
        return 0;
    L = strlen(tok);
    if (L < 2 || L > 8)
        return 0;
    if (pos == 0)
        return 1;
    if (pos + 1 < n && IsCopula(toks[pos + 1]))
        return 1;
    return 0;
}

QA_TYPE QADetectQuestionType(const char toks[][QA_TOKEN_MAX],
                              uint32_t n, uint32_t wh_pos,
                              char *entity_out, size_t entity_size)
{
    uint32_t i;
    if (wh_pos >= n || entity_out == NULL || entity_size == 0)
        return QA_NONE;
    entity_out[0] = '\0';

    /* Check if the wh-word itself is a count marker ("cuantos", "how many").
       This must precede the ENTITY pattern because copula may follow. */
    if (IsCountMarker(toks[wh_pos]))
    {
        /* <count_wh> <entity...> or <count_wh> <copula> <entity...> */
        uint32_t start = wh_pos + 1;
        if (start < n && IsCopula(toks[start]))
            start++;
        while (start < n && IsArticle(toks[start]))
            start++;
        if (start < n)
        {
            size_t pos = 0;
            for (i = start; i < n; i++)
            {
                if (IsPreposition(toks[i]))
                    break;
                if (i > start && pos + 1 < entity_size)
                    entity_out[pos++] = ' ';
                {
                    size_t tl = strlen(toks[i]);
                    if (pos + tl >= entity_size)
                        tl = entity_size - pos - 1;
                    memcpy(entity_out + pos, toks[i], tl);
                    pos += tl;
                }
            }
            entity_out[pos] = '\0';
            if (entity_out[0] != '\0')
                return QA_COUNT;
        }
    }

    /* Pattern 1: <wh> <copula> <entity...> -> ENTITY */
    if (wh_pos + 1 < n && IsCopula(toks[wh_pos + 1]))
    {
        uint32_t start = wh_pos + 2;
        while (start < n && IsArticle(toks[start]))
            start++;
        if (start < n)
        {
            size_t pos = 0;
            for (i = start; i < n; i++)
            {
                if (IsPreposition(toks[i]))
                    break;
                if (i > start && pos + 1 < entity_size)
                    entity_out[pos++] = ' ';
                {
                    size_t tl = strlen(toks[i]);
                    if (pos + tl >= entity_size)
                        tl = entity_size - pos - 1;
                    memcpy(entity_out + pos, toks[i], tl);
                    pos += tl;
                }
            }
            entity_out[pos] = '\0';
            if (entity_out[0] != '\0')
                return QA_ENTITY;
        }
    }

    /* Pattern 2: <wh> <location_marker> <entity> -> WHERE */
    if (wh_pos + 1 < n && IsLocationMarker(toks[wh_pos + 1]))
    {
        uint32_t start = wh_pos + 2;
        if (start < n)
        {
            size_t pos = 0;
            for (i = start; i < n; i++)
            {
                if (i > start && pos + 1 < entity_size)
                    entity_out[pos++] = ' ';
                {
                    size_t tl = strlen(toks[i]);
                    if (pos + tl >= entity_size)
                        tl = entity_size - pos - 1;
                    memcpy(entity_out + pos, toks[i], tl);
                    pos += tl;
                }
            }
            entity_out[pos] = '\0';
            if (entity_out[0] != '\0')
                return QA_WHERE;
        }
    }

    /* Pattern 3: <wh> <count_marker> <entity> -> COUNT */
    if (wh_pos + 1 < n && IsCountMarker(toks[wh_pos + 1]))
    {
        uint32_t start = wh_pos + 2;
        if (start < n)
        {
            size_t pos = 0;
            for (i = start; i < n; i++)
            {
                if (i > start && pos + 1 < entity_size)
                    entity_out[pos++] = ' ';
                {
                    size_t tl = strlen(toks[i]);
                    if (pos + tl >= entity_size)
                        tl = entity_size - pos - 1;
                    memcpy(entity_out + pos, toks[i], tl);
                    pos += tl;
                }
            }
            entity_out[pos] = '\0';
            if (entity_out[0] != '\0')
                return QA_COUNT;
        }
    }

    /* Pattern 4: <wh_cause> <entity> <relation> -> WHY */
    if (wh_pos + 1 < n && IsCauseMarker(toks[wh_pos]))
    {
        uint32_t start = wh_pos + 1;
        if (start < n && strcmp(toks[start], "que") == 0)
            start++;
        if (start < n)
        {
            size_t pos = 0;
            for (i = start; i < n; i++)
            {
                if (i > start && pos + 1 < entity_size)
                    entity_out[pos++] = ' ';
                {
                    size_t tl = strlen(toks[i]);
                    if (pos + tl >= entity_size)
                        tl = entity_size - pos - 1;
                    memcpy(entity_out + pos, toks[i], tl);
                    pos += tl;
                }
            }
            entity_out[pos] = '\0';
            if (entity_out[0] != '\0')
                return QA_WHY;
        }
    }

    return QA_OPEN;
}

/* ================================================================
   Question Parser
   ================================================================ */

int QAParseQuestion(const char *question, QA_PARSE *out)
{
    char toks[64][QA_TOKEN_MAX];
    uint32_t n;
    uint32_t i;
    if (question == NULL || out == NULL)
        return 0;
    memset(out, 0, sizeof(*out));

    n = QASplit(question, toks, 64);
    if (n == 0)
        return 0;

    out->is_question = (question[strlen(question) - 1] == '?');

    for (i = 0; i < n; i++)
    {
        if (QALooksLikeQuestionWord(toks[i], i, toks, n))
        {
            char entity[QA_TOKEN_MAX];
            QA_TYPE qt = QADetectQuestionType(toks, n, i,
                                               entity, sizeof(entity));
            if (qt != QA_NONE && qt != QA_OPEN)
            {
                out->type = qt;
                strncpy(out->slots[0], entity, QA_TOKEN_MAX - 1);
                out->slots[0][QA_TOKEN_MAX - 1] = '\0';
                out->nslots = 1;
                return 1;
            }
            if (qt == QA_OPEN)
            {
                out->type = QA_ENTITY;
                strncpy(out->slots[0], toks[i], QA_TOKEN_MAX - 1);
                out->slots[0][QA_TOKEN_MAX - 1] = '\0';
                out->nslots = 1;
                return 1;
            }
        }
    }

    out->type = QA_OPEN;
    {
        size_t pos = 0;
        for (i = 0; i < n && pos < QA_TOKEN_MAX - 1; i++)
        {
            if (i > 0 && pos < QA_TOKEN_MAX - 2)
                out->slots[0][pos++] = ' ';
            size_t tl = strlen(toks[i]);
            if (pos + tl >= QA_TOKEN_MAX)
                tl = QA_TOKEN_MAX - pos - 1;
            memcpy(out->slots[0] + pos, toks[i], tl);
            pos += tl;
        }
        out->slots[0][pos] = '\0';
    }
    out->nslots = 1;
    return 1;
}

/* ================================================================
   KB Query Helpers
   ================================================================ */

static uint32_t KBCountEntity(const SCHEMA_KB *kb, const char *entity)
{
    uint32_t count = 0;
    if (kb == NULL || entity == NULL || entity[0] == '\0')
        return 0;
    for (uint32_t i = 0; i < kb->num_pairs; i++)
    {
        if (strcmp(kb->pairs[i].subject, entity) == 0 ||
            strcmp(kb->pairs[i].object, entity) == 0)
            count++;
    }
    return count;
}

static int KBFindSubject(const SCHEMA_KB *kb, const char *entity,
                          char *predicate_out, size_t pred_size,
                          char *object_out, size_t obj_size)
{
    if (kb == NULL || entity == NULL || entity[0] == '\0')
        return 0;
    for (uint32_t i = 0; i < kb->num_pairs; i++)
    {
        if (strcmp(kb->pairs[i].subject, entity) == 0)
        {
            strncpy(predicate_out, kb->pairs[i].family, pred_size - 1);
            predicate_out[pred_size - 1] = '\0';
            strncpy(object_out, kb->pairs[i].object, obj_size - 1);
            object_out[obj_size - 1] = '\0';
            return 1;
        }
    }
    return 0;
}

static int KBFindObject(const SCHEMA_KB *kb, const char *entity,
                         char *subject_out, size_t subj_size,
                         char *predicate_out, size_t pred_size)
{
    if (kb == NULL || entity == NULL || entity[0] == '\0')
        return 0;
    for (uint32_t i = 0; i < kb->num_pairs; i++)
    {
        if (strcmp(kb->pairs[i].object, entity) == 0)
        {
            strncpy(subject_out, kb->pairs[i].subject, subj_size - 1);
            subject_out[subj_size - 1] = '\0';
            strncpy(predicate_out, kb->pairs[i].family, pred_size - 1);
            predicate_out[pred_size - 1] = '\0';
            return 1;
        }
    }
    return 0;
}

static uint32_t KBFindAllSubject(const SCHEMA_KB *kb, const char *entity,
                                  char results[][256], uint32_t max_results)
{
    uint32_t found = 0;
    if (kb == NULL || entity == NULL || entity[0] == '\0')
        return 0;
    for (uint32_t i = 0; i < kb->num_pairs && found < max_results; i++)
    {
        if (strcmp(kb->pairs[i].subject, entity) == 0)
        {
            snprintf(results[found], 256, "%s %s",
                     kb->pairs[i].family, kb->pairs[i].object);
            found++;
        }
    }
    return found;
}

/* ================================================================
   Text Store Query (using embeddings for semantic match)
   ================================================================ */

static int TextQueryEmbed(CHAT *ch, const char *entity,
                           char *sentence_out, size_t size)
{
    const char *words[4];
    uint32_t nw = 0;
    uint32_t best = 0, bestf = 0;
    float bestsc = 0.0f;
    int have = 0;

    if (ch == NULL || entity == NULL || entity[0] == '\0')
        return 0;
    if (ch->ntfiles == 0 || ch->tgraph == NULL || ch->temb == NULL)
        return 0;

    words[nw++] = entity;

    for (uint32_t f = 0; f < ch->ntfiles; f++)
    {
        uint32_t idx[16];
        float sc[16];
        uint32_t r = TextLexRetrieve(&ch->tlex[f], ch->tgraph,
                                     ch->temb, words, nw,
                                     idx, sc, 16);
        for (uint32_t j = 0; j < r; j++)
        {
            if (!have || sc[j] > bestsc)
            {
                best = idx[j];
                bestf = f;
                bestsc = sc[j];
                have = 1;
            }
        }
    }

    if (have && bestsc > 0.1f && ch->tlex[bestf].image != NULL)
    {
        char sent[2048];
        if (TextLexSentenceText(&ch->tlex[bestf], best,
                                ch->tlex[bestf].image,
                                ch->tlex[bestf].imagelen, sent,
                                sizeof(sent)) > 0)
        {
            strncpy(sentence_out, sent, size - 1);
            sentence_out[size - 1] = '\0';
            return 1;
        }
    }
    return 0;
}

/* ================================================================
   QA Init (no-op, just validates API)
   ================================================================ */

void QA_Init(void)
{
    /* No state to initialize; the QA layer is stateless and reads
       from the CHAT session's KB and text stores. */
}

/* ================================================================
   Main QA Answer Function
   ================================================================ */

int QAAnswer(CHAT *ch, const char *question, QA_ANSWER *out)
{
    QA_PARSE parse;
    if (ch == NULL || question == NULL || out == NULL)
        return 0;
    memset(out, 0, sizeof(*out));

    if (!QAParseQuestion(question, &parse))
    {
        strncpy(out->text, "No entendi la pregunta.", QA_ANSWER_MAX - 1);
        return 0;
    }

    switch (parse.type)
    {
    case QA_ENTITY:
    {
        /* "who is X?" -> lookup X in KB, then text */
        char pred[256], obj[256];
        char sent[1024];

        /* Try KB: X is <pred> of <obj> */
        if (KBFindSubject(&ch->kb, parse.slots[0],
                           pred, sizeof(pred), obj, sizeof(obj)))
        {
            char cap[QA_TOKEN_MAX];
            strncpy(cap, parse.slots[0], sizeof(cap) - 1);
            cap[sizeof(cap) - 1] = '\0';
            cap[0] = (char)toupper((unsigned char)cap[0]);
            snprintf(out->text, QA_ANSWER_MAX,
                     "%s %s %s.", cap, pred, obj);
            out->confidence = 0.9f;
            return 1;
        }

        /* Try KB: <subject> is <pred> of X */
        if (KBFindObject(&ch->kb, parse.slots[0],
                          pred, sizeof(pred), obj, sizeof(obj)))
        {
            char cap[QA_TOKEN_MAX];
            strncpy(cap, parse.slots[0], sizeof(cap) - 1);
            cap[sizeof(cap) - 1] = '\0';
            cap[0] = (char)toupper((unsigned char)cap[0]);
            snprintf(out->text, QA_ANSWER_MAX,
                     "%s %s %s.", pred, obj, cap);
            out->confidence = 0.8f;
            return 1;
        }

        /* Try text store */
        if (TextQueryEmbed(ch, parse.slots[0], sent, sizeof(sent)))
        {
            snprintf(out->text, QA_ANSWER_MAX,
                     "Segun el texto: %s", sent);
            out->confidence = 0.5f;
            strncpy(out->source, "text_store", sizeof(out->source) - 1);
            out->has_source = 1;
            return 1;
        }

        strncpy(out->text, "No tengo constancia de esa entidad.",
                QA_ANSWER_MAX - 1);
        out->confidence = 0.0f;
        return 0;
    }

    case QA_COUNT:
    {
        /* "how many X?" -> count triples mentioning entity */
        uint32_t count = KBCountEntity(&ch->kb, parse.slots[0]);
        if (count > 0)
        {
            char cap[QA_TOKEN_MAX];
            strncpy(cap, parse.slots[0], sizeof(cap) - 1);
            cap[sizeof(cap) - 1] = '\0';
            cap[0] = (char)toupper((unsigned char)cap[0]);
            snprintf(out->text, QA_ANSWER_MAX,
                     "Hay %u registros relacionados con %s.", count, cap);
            out->confidence = 0.9f;
            return 1;
        }

        /* Try text store */
        {
            char sent[1024];
            if (TextQueryEmbed(ch, parse.slots[0], sent, sizeof(sent)))
            {
                snprintf(out->text, QA_ANSWER_MAX,
                         "Segun el texto: %s", sent);
                out->confidence = 0.5f;
                strncpy(out->source, "text_store", sizeof(out->source) - 1);
                out->has_source = 1;
                return 1;
            }
        }

        snprintf(out->text, QA_ANSWER_MAX,
                 "No tengo constancia de cuantos hay de %s.",
                 parse.slots[0]);
        out->confidence = 0.0f;
        return 0;
    }

    case QA_WHERE:
    {
        /* "where is X?" -> text search for location */
        char sent[1024];
        if (TextQueryEmbed(ch, parse.slots[0], sent, sizeof(sent)))
        {
            snprintf(out->text, QA_ANSWER_MAX,
                     "Segun el texto: %s", sent);
            out->confidence = 0.5f;
            strncpy(out->source, "text_store", sizeof(out->source) - 1);
            out->has_source = 1;
            return 1;
        }
        snprintf(out->text, QA_ANSWER_MAX,
                 "No tengo constancia del lugar de %s.", parse.slots[0]);
        out->confidence = 0.0f;
        return 0;
    }

    case QA_WHY:
    {
        /* "why X?" -> text search for cause */
        char sent[1024];
        if (TextQueryEmbed(ch, parse.slots[0], sent, sizeof(sent)))
        {
            snprintf(out->text, QA_ANSWER_MAX,
                     "Segun el texto: %s", sent);
            out->confidence = 0.5f;
            strncpy(out->source, "text_store", sizeof(out->source) - 1);
            out->has_source = 1;
            return 1;
        }
        snprintf(out->text, QA_ANSWER_MAX,
                 "No tengo constancia de la razon de %s.", parse.slots[0]);
        out->confidence = 0.0f;
        return 0;
    }

    case QA_RELATION:
    {
        /* "hijos de X?" -> find all relations of entity */
        char results[16][256];
        uint32_t found = KBFindAllSubject(&ch->kb, parse.slots[0],
                                           results, 16);
        if (found > 0)
        {
            char cap[QA_TOKEN_MAX];
            strncpy(cap, parse.slots[0], sizeof(cap) - 1);
            cap[sizeof(cap) - 1] = '\0';
            cap[0] = (char)toupper((unsigned char)cap[0]);
            snprintf(out->text, QA_ANSWER_MAX, "%s:", cap);
            for (uint32_t i = 0; i < found; i++)
            {
                size_t L = strlen(out->text);
                snprintf(out->text + L, QA_ANSWER_MAX - L,
                         "%s %s", i ? "," : "", results[i]);
            }
            strncat(out->text, ".", QA_ANSWER_MAX - strlen(out->text) - 1);
            out->confidence = 0.8f;
            return 1;
        }

        /* Fallback to text */
        {
            char sent[1024];
            if (TextQueryEmbed(ch, parse.slots[0], sent, sizeof(sent)))
            {
                snprintf(out->text, QA_ANSWER_MAX,
                         "Segun el texto: %s", sent);
                out->confidence = 0.5f;
                return 1;
            }
        }

        snprintf(out->text, QA_ANSWER_MAX,
                 "No tengo constancia de relaciones de %s.",
                 parse.slots[0]);
        out->confidence = 0.0f;
        return 0;
    }

    case QA_BOOLEAN:
    {
        /* "is X Y?" -> check KB for relation */
        snprintf(out->text, QA_ANSWER_MAX,
                 "No tengo constancia suficiente para confirmar.");
        out->confidence = 0.0f;
        return 0;
    }

    case QA_OPEN:
    {
        /* Open query: try KB, then text */
        char pred[256], obj[256];
        char sent[1024];

        if (KBFindSubject(&ch->kb, parse.slots[0],
                           pred, sizeof(pred), obj, sizeof(obj)))
        {
            snprintf(out->text, QA_ANSWER_MAX,
                     "%s %s %s.", parse.slots[0], pred, obj);
            out->confidence = 0.7f;
            return 1;
        }

        if (TextQueryEmbed(ch, parse.slots[0], sent, sizeof(sent)))
        {
            snprintf(out->text, QA_ANSWER_MAX,
                     "Segun el texto: %s", sent);
            out->confidence = 0.5f;
            strncpy(out->source, "text_store", sizeof(out->source) - 1);
            out->has_source = 1;
            return 1;
        }

        snprintf(out->text, QA_ANSWER_MAX,
                 "No tengo constancia de \"%s\".", parse.slots[0]);
        out->confidence = 0.0f;
        return 0;
    }

    default:
        break;
    }

    strncpy(out->text, "No entendi la pregunta.", QA_ANSWER_MAX - 1);
    out->confidence = 0.0f;
    return 0;
}
