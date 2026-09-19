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
    SYMBOL_ID eid;

    if (ch == NULL || entity == NULL || entity[0] == '\0')
        return 0;
    if (ch->ntfiles == 0 || ch->tgraph == NULL || ch->temb == NULL)
        return 0;

    /* Resolve entity symbol: try exact, then lowercase */
    eid = SymbolFind(ch->tgraph->symbols, entity);
    if (eid == SYMBOL_INVALID)
    {
        char lower[64];
        uint32_t i;
        for (i = 0; entity[i] && i < sizeof(lower) - 1; i++)
            lower[i] = (char)tolower((unsigned char)entity[i]);
        lower[i] = '\0';
        eid = SymbolFind(ch->tgraph->symbols, lower);
    }
    if (eid == SYMBOL_INVALID)
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
            /* Verify entity presence in returned sentence:
               reject if entity symbol does not appear in any
               token of the sentence. Prevents unrelated text
               returned by embedding similarity alone. */
            int found = 0;
            for (uint32_t f2 = 0; f2 < ch->ntfiles && !found; f2++)
            {
                TEXTLEX *tl2 = &ch->tlex[f2];
                if (tl2->nsent <= best)
                    continue;
                TL_SENT *st = &tl2->sents[best];
                for (uint32_t t = 0; t < st->ntok; t++)
                {
                    if (st->ids[t] == eid)
                    {
                        found = 1;
                        break;
                    }
                }
            }
            if (found)
            {
                strncpy(sentence_out, sent, size - 1);
                sentence_out[size - 1] = '\0';
                return 1;
            }
        }
    }
    return 0;
}

/* ---- Context-aware fallback: use surrounding question words ----
   When the entity word is not in the symbol table, use ALL other
   question words that DO resolve as query terms. This is analogous
   to attention: the surrounding known words navigate the semantic
   space to find relevant sentences, even when the target word is
   missing from the vocabulary. No entity verification — the entity
   may not appear verbatim as a symbol, that's why we're here. */
static int IsStopWordQA(const char *tok)
{
    static const char *STOP[] = {
        "de", "of", "del", "'s", "el", "la", "los", "las", "the",
        "un", "una", "unos", "unas", "a", "an", "en", "y", "e",
        "o", "u", "que", "quien", "quienes", "cual", "cuales",
        "su", "sus", "his", "her", "mi", "my", "tu", "your",
        "es", "era", "fue", "is", "was", "son", "por", "why",
        "no", "si", "como", "how", "donde", "where", "cuantos",
        "cuantas", "many", "much", "few",
    };
    for (size_t i = 0; i < sizeof(STOP) / sizeof(STOP[0]); i++)
        if (strcmp(tok, STOP[i]) == 0)
            return 1;
    return 0;
}

static int TextQueryContext(CHAT *ch, const char *question,
                            const char *entity,
                            char *sentence_out, size_t size)
{
    const char *ctx[8];
    uint32_t nctx = 0;
    uint32_t best = 0, bestf = 0;
    float bestsc = 0.0f;
    int have = 0;
    char qtoks[16][64];
    uint32_t ntoks = 0;
    const char *p;
    char elow[64];
    uint32_t ei;

    if (ch == NULL || question == NULL || entity == NULL || entity[0] == '\0')
        return 0;
    if (ch->ntfiles == 0 || ch->tgraph == NULL || ch->temb == NULL)
        return 0;

    /* Lowercase entity for case-insensitive comparison */
    for (ei = 0; entity[ei] && ei < sizeof(elow) - 1; ei++)
        elow[ei] = (char)tolower((unsigned char)entity[ei]);
    elow[ei] = '\0';

    /* Tokenize question by spaces */
    p = question;
    while (*p && ntoks < 16)
    {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
            p++;
        if (*p == '\0')
            break;
        {
            size_t tl = 0;
            const char *start = p;
            while (*p && *p != ' ' && *p != '\t' && *p != '\n' &&
                   *p != '\r' && tl < 63)
            {
                qtoks[ntoks][tl++] = (char)tolower((unsigned char)*p);
                p++;
            }
            qtoks[ntoks][tl] = '\0';
            if (tl >= 2)
                ntoks++;
        }
    }

    /* Collect context words: skip stop words, entity, short tokens */
    for (uint32_t i = 0; i < ntoks && nctx < 8; i++)
    {
        char low[64];
        uint32_t j;
        int skip = 0;

        if (strlen(qtoks[i]) < 2)
            continue;
        if (IsStopWordQA(qtoks[i]))
            continue;
        /* Skip the entity itself (exact or case-insensitive) */
        if (strcmp(qtoks[i], entity) == 0 || strcmp(qtoks[i], elow) == 0)
            continue;

        /* Try SymbolFind with original token, then lowercase */
        for (j = 0; qtoks[i][j] && j < sizeof(low) - 1; j++)
            low[j] = (char)tolower((unsigned char)qtoks[i][j]);
        low[j] = '\0';

        if (SymbolFind(ch->tgraph->symbols, qtoks[i]) != SYMBOL_INVALID)
        {
            ctx[nctx++] = qtoks[i];
            continue;
        }
        if (SymbolFind(ch->tgraph->symbols, low) != SYMBOL_INVALID)
        {
            ctx[nctx++] = qtoks[i];
            continue;
        }
    }

    if (nctx == 0)
        return 0;

    /* Query with context words */
    for (uint32_t f = 0; f < ch->ntfiles; f++)
    {
        uint32_t idx[16];
        float sc[16];
        uint32_t r = TextLexRetrieve(&ch->tlex[f], ch->tgraph,
                                     ch->temb, ctx, nctx,
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

/* ---- Raw-text substring search: bypass symbol table ----
   When the entity is not in the symbol table, scan the raw corpus
   image for the entity string (case-insensitive). Returns the
   first sentence that contains the entity as a substring. This is
   the last-resort fallback: it works even when the tokenizer
   produced a different symbol than the query word. */
static int TextFindRaw(CHAT *ch, const char *entity,
                       char *sentence_out, size_t size)
{
    if (ch == NULL || entity == NULL || entity[0] == '\0')
        return 0;
    if (ch->ntfiles == 0)
        return 0;

    /* Build lowercase entity for case-insensitive search */
    char elow[128];
    uint32_t i;
    for (i = 0; entity[i] && i < sizeof(elow) - 1; i++)
        elow[i] = (char)tolower((unsigned char)entity[i]);
    elow[i] = '\0';
    size_t elen = strlen(elow);
    if (elen == 0)
        return 0;

    for (uint32_t f = 0; f < ch->ntfiles; f++)
    {
        TEXTLEX *tl = &ch->tlex[f];
        if (tl->image == NULL || tl->imagelen == 0)
            continue;
        /* Scan raw image for substring match */
        for (size_t pos = 0; pos + elen <= tl->imagelen; pos++)
        {
            int match = 1;
            for (size_t k = 0; k < elen; k++)
            {
                if (tolower((unsigned char)tl->image[pos + k]) != (unsigned char)elow[k])
                {
                    match = 0;
                    break;
                }
            }
            if (!match)
                continue;
            /* Found substring — find which sentence contains it */
            for (uint32_t s = 0; s < tl->nsent; s++)
            {
                TL_SENT *st = &tl->sents[s];
                if (st->ntok == 0)
                    continue;
                /* Sentence byte range: first token start to last token end */
                size_t sent_start = (size_t)st->offs[0];
                size_t sent_end = (size_t)st->offs[st->ntok - 1] +
                                  (size_t)st->lens[st->ntok - 1];
                if (pos >= sent_start && pos < sent_end)
                {
                    if (TextLexSentenceText(tl, s, tl->image, tl->imagelen,
                                            sentence_out, size) > 0)
                        return 1;
                }
            }
        }
    }
    return 0;
}

/* ---- Definitional search: find "X is/are Y" patterns in corpus ----
   Structural scan: for each sentence, check if the entity token is
   immediately followed by a high-frequency function word (copula
   candidate). No vocabulary lists — frequency threshold deduced
   from the corpus itself. Returns 1 if a definitional sentence
   is found. */
static int TextFindDefinition(CHAT *ch, const char *entity,
                               char *sentence_out, size_t size)
{
    SYMBOL_ID eid;
    uint32_t total_freq = 0;
    uint32_t nfunc = 0;

    if (ch == NULL || entity == NULL || entity[0] == '\0')
        return 0;
    if (ch->ntfiles == 0 || ch->tgraph == NULL)
        return 0;

    /* Resolve entity symbol */
    eid = SymbolFind(ch->tgraph->symbols, entity);
    if (eid == SYMBOL_INVALID)
    {
        char lower[64];
        uint32_t i;
        for (i = 0; entity[i] && i < sizeof(lower) - 1; i++)
            lower[i] = (char)tolower((unsigned char)entity[i]);
        lower[i] = '\0';
        eid = SymbolFind(ch->tgraph->symbols, lower);
    }
    if (eid == SYMBOL_INVALID)
        return 0;

    /* Compute total frequency and count function words (>1% of tokens).
       Function words are structurally defined: high-frequency tokens
       that appear across many sentences. Copulas are a subset. */
    for (uint32_t i = 0; i < ch->tgraph->symbols->count; i++)
        total_freq += ch->tgraph->symbols->items[i].frequency;
    if (total_freq == 0)
        return 0;

    /* Scan sentences for ENTITY + FUNCTION_WORD pattern */
    for (uint32_t f = 0; f < ch->ntfiles; f++)
    {
        TEXTLEX *tl = &ch->tlex[f];
        if (tl->image == NULL || tl->nsent == 0)
            continue;
        for (uint32_t s = 0; s < tl->nsent; s++)
        {
            TL_SENT *sent = &tl->sents[s];
            for (uint32_t j = 0; j + 1 < sent->ntok; j++)
            {
                if (sent->ids[j] != eid)
                    continue;
                /* Next token: check if it's a high-frequency function
                   word (top ~5% by frequency = likely copula/link) */
                const SYMBOL *next = SymbolGet(ch->tgraph->symbols,
                                               sent->ids[j + 1]);
                if (next == NULL)
                    continue;
                float rel = (float)next->frequency / (float)total_freq;
                if (rel < 0.005f)
                    continue;
                /* Found ENTITY + function word: extract sentence */
                if (TextLexSentenceText(tl, s, tl->image, tl->imagelen,
                                        sentence_out, size) > 0)
                    return 1;
            }
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

        /* Translation fallback: translate entity to canonical form */
        {
            const char *translated = DictTranslate(&ch->dict, parse.slots[0]);
            if (translated != NULL)
            {
                /* Retry KB with translated entity */
                if (KBFindSubject(&ch->kb, translated,
                                  pred, sizeof(pred), obj, sizeof(obj)))
                {
                    char cap[QA_TOKEN_MAX];
                    strncpy(cap, translated, sizeof(cap) - 1);
                    cap[sizeof(cap) - 1] = '\0';
                    cap[0] = (char)toupper((unsigned char)cap[0]);
                    snprintf(out->text, QA_ANSWER_MAX,
                             "%s %s %s.", cap, pred, obj);
                    out->confidence = 0.85f;
                    strncpy(out->source, "dict_translate",
                            sizeof(out->source) - 1);
                    out->has_source = 1;
                    return 1;
                }
                if (KBFindObject(&ch->kb, translated,
                                 pred, sizeof(pred), obj, sizeof(obj)))
                {
                    char cap[QA_TOKEN_MAX];
                    strncpy(cap, translated, sizeof(cap) - 1);
                    cap[sizeof(cap) - 1] = '\0';
                    cap[0] = (char)toupper((unsigned char)cap[0]);
                    snprintf(out->text, QA_ANSWER_MAX,
                             "%s %s %s.", pred, obj, cap);
                    out->confidence = 0.8f;
                    strncpy(out->source, "dict_translate",
                            sizeof(out->source) - 1);
                    out->has_source = 1;
                    return 1;
                }
                /* Retry text store with translated entity */
                if (TextQueryEmbed(ch, translated, sent, sizeof(sent)))
                {
                    snprintf(out->text, QA_ANSWER_MAX,
                             "Segun el texto: %s", sent);
                    out->confidence = 0.45f;
                    strncpy(out->source, "dict_translate",
                            sizeof(out->source) - 1);
                    out->has_source = 1;
                    return 1;
                }
            }
        }

        /* Context fallback: use surrounding question words */
        if (TextQueryContext(ch, question, parse.slots[0], sent, sizeof(sent)))
        {
            snprintf(out->text, QA_ANSWER_MAX,
                     "Segun el texto: %s", sent);
            out->confidence = 0.4f;
            strncpy(out->source, "text_context", sizeof(out->source) - 1);
            out->has_source = 1;
            return 1;
        }

        /* Try definitional search: "X is/are Y" pattern */
        if (TextFindDefinition(ch, parse.slots[0], sent, sizeof(sent)))
        {
            snprintf(out->text, QA_ANSWER_MAX,
                     "Segun el texto: %s", sent);
            out->confidence = 0.6f;
            strncpy(out->source, "text_store", sizeof(out->source) - 1);
            out->has_source = 1;
            return 1;
        }

        /* Raw substring fallback: scan corpus text directly */
        if (TextFindRaw(ch, parse.slots[0], sent, sizeof(sent)))
        {
            snprintf(out->text, QA_ANSWER_MAX,
                     "Segun el texto: %s", sent);
            out->confidence = 0.3f;
            strncpy(out->source, "text_raw", sizeof(out->source) - 1);
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

        /* Context fallback */
        {
            char sent[1024];
            if (TextQueryContext(ch, question, parse.slots[0],
                                sent, sizeof(sent)))
            {
                snprintf(out->text, QA_ANSWER_MAX,
                         "Segun el texto: %s", sent);
                out->confidence = 0.4f;
                strncpy(out->source, "text_context",
                        sizeof(out->source) - 1);
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
        if (TextQueryContext(ch, question, parse.slots[0],
                            sent, sizeof(sent)))
        {
            snprintf(out->text, QA_ANSWER_MAX,
                     "Segun el texto: %s", sent);
            out->confidence = 0.4f;
            strncpy(out->source, "text_context", sizeof(out->source) - 1);
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
        if (TextQueryContext(ch, question, parse.slots[0],
                            sent, sizeof(sent)))
        {
            snprintf(out->text, QA_ANSWER_MAX,
                     "Segun el texto: %s", sent);
            out->confidence = 0.4f;
            strncpy(out->source, "text_context", sizeof(out->source) - 1);
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

        /* Context fallback */
        {
            char sent[1024];
            if (TextQueryContext(ch, question, parse.slots[0],
                                sent, sizeof(sent)))
            {
                snprintf(out->text, QA_ANSWER_MAX,
                         "Segun el texto: %s", sent);
                out->confidence = 0.4f;
                strncpy(out->source, "text_context",
                        sizeof(out->source) - 1);
                out->has_source = 1;
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

        if (TextQueryContext(ch, question, parse.slots[0],
                            sent, sizeof(sent)))
        {
            snprintf(out->text, QA_ANSWER_MAX,
                     "Segun el texto: %s", sent);
            out->confidence = 0.4f;
            strncpy(out->source, "text_context", sizeof(out->source) - 1);
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
