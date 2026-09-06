#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "dialog.h"
#include "stem.h"
#include "parser.h"
#include "generator.h"
#include "nlg.h"
#include "i18n.h"
#include "i18n.h"

static void StrToUpper(const char *src, char *dst, size_t max_len)
{
    size_t i = 0;
    while (src[i] && i < max_len - 1)
    {
        dst[i] = (char)toupper((unsigned char)src[i]);
        i++;
    }
    dst[i] = '\0';
}

DIALOG_INTENT DialogClassify(const char *input)
{
    DIALOG_INTENT intent;
    memset(&intent, 0, sizeof(intent));

    if (input == NULL)
        return intent;

    PARSED_SENTENCE tokens;
    ParserTokenize(input, &tokens);

    if (tokens.count == 0)
        return intent;

    if (strchr(input, '?') != NULL)
    {
        intent.act = SPEECH_ACT_QUERY;
        return intent;
    }

    /* Question-only closed forms without punctuation still read as a
       question: "quien eres" (2 tokens) is a self-question, not small
       talk. Same rule as the ingester, through one shared function. */
    if (ParserIsQuestion(input))
    {
        intent.act = SPEECH_ACT_QUERY;
        return intent;
    }

    /* Short input with nothing to store is social. Longer input is a
       statement: the tree pass will identify its symbols and relations. */
    if (tokens.count <= 2)
    {
        intent.act = SPEECH_ACT_SOCIAL;
        intent.is_social_only = 1;
        return intent;
    }

    intent.act = SPEECH_ACT_STATEMENT;
    return intent;
}

static void GetUnknownResponse(char *out, size_t size, const char *subj, const char *rel)
{
    int n = LangVariantCount(I18N_UNKNOWN);
    const char *fmt = LangString(I18N_UNKNOWN, rand() % (n > 0 ? n : 1));
    snprintf(out, size, fmt, subj, rel);
}

int DialogGenerateResponse(
    GRAPH *graph,
    CONTEXT *ctx,
    const char *user_input,
    char *out_response,
    size_t max_len)
{
    if (!graph || !user_input || !out_response || max_len == 0)
        return 0;

    DIALOG_INTENT intent = DialogClassify(user_input);

    /* Social input: short, nothing to store. Minimal ack. */
    if (intent.act == SPEECH_ACT_SOCIAL)
    {
        int n = LangVariantCount(I18N_SOCIAL);
        snprintf(out_response, max_len, "%s",
                 LangString(I18N_SOCIAL, rand() % (n > 0 ? n : 1)));
        return 1;
    }


    /* ---- Structured QA: delegate to the parser (single source) ---- */
    {
        QUESTION pq = ParserDetectQuestion(graph, user_input);
        if (pq.valid)
        {
            char answer[256] = {0};
            if (ParserAnswerQuestion(graph, &pq, answer, sizeof(answer)))
            {
                /* Verbalize through a learned mold when one exists;
                   otherwise echo the bare answer. */
                char first[128] = {0};
                uint32_t k = 0;
                while (answer[k] != '\0' && answer[k] != ',' &&
                       k < sizeof(first) - 1)
                {
                    first[k] = answer[k];
                    k++;
                }
                first[k] = '\0';
                char sent[512] = {0};
                if (strcmp(pq.subject, "YO") == 0 &&
                    (strcmp(pq.relation, "ES") == 0 ||
                     strcmp(pq.relation, "ESTAR") == 0))
                {
                    /* Identity always takes the localized copula
                       ("Soy X"), never a learned mold ("YO ES X"):
                       any ingested "es"-sentence records a mold for
                       the base copula that would otherwise shadow it. */
                    snprintf(out_response, max_len, "%s%s",
                             LangString(I18N_AM, 0), answer);
                }
                else if (first[0] != '\0' &&
                         SurfaceRender(pq.relation, pq.subject, first,
                                       sent, sizeof(sent)))
                {
                    snprintf(out_response, max_len, "%s", sent);
                }
                else
                {
                    snprintf(out_response, max_len, "%s", answer);
                }
                return 1;
            }
        }
    }

    /* Suffix query blocks removed: the parser FACT shape resolves
       descriptor+entity in both orders with no word lists. */

    /* Knowledge queries */
    if (intent.act == SPEECH_ACT_QUERY)
    {
        char clean[256];
        strncpy(clean, user_input, sizeof(clean) - 1);
        clean[sizeof(clean) - 1] = '\0';

        char upper[256];
        StrToUpper(clean, upper, sizeof(upper));

        char subj[64] = "";
        char rel[64] = "";

        /* Find all matching symbols */
        char matches[16][64];
        uint32_t match_count = 0;

        uint32_t sym_count = SymbolCount(graph->symbols);
        for (uint32_t i = 0; i < sym_count && match_count < 16; i++)
        {
            const SYMBOL *sym = SymbolGet(graph->symbols, (SYMBOL_ID)i);
            if (sym == NULL || sym->name == NULL) continue;
            if (sym->name[0] == '\0') continue;
            /* 1-letter symbols are too easy to hit by substring match
               ("E" inside "ERES") and fabricate nonsense answers. */
            if (strlen(sym->name) < 2) continue;
            if (strstr(upper, sym->name))
            {
                strncpy(matches[match_count], sym->name, 63);
                matches[match_count][63] = '\0';
                match_count++;
            }
        }

        if (match_count >= 2)
        {
            /* Try each pair: find which one has relations as (subject, other) */
            int found_pair = 0;
            for (uint32_t i = 0; i < match_count && !found_pair; i++)
            {
                SYMBOL_ID try_subj = SymbolFind(graph->symbols, matches[i]);
                if (try_subj == SYMBOL_INVALID) continue;

                    RELATION *dummy[1];
                    uint32_t has_rels = GraphQuerySubject(graph, try_subj, dummy, 1);
                if (has_rels == 0) continue;

                for (uint32_t j = 0; j < match_count && !found_pair; j++)
                {
                    if (i == j) continue;
                    SYMBOL_ID try_rel = SymbolFind(graph->symbols, matches[j]);
                    if (try_rel == SYMBOL_INVALID) continue;

                    RELATION *r[1];
                    uint32_t n = GraphQuerySubjectRelation(graph, try_subj, try_rel, r, 1);
                    if (n > 0)
                    {
                        strncpy(subj, matches[i], sizeof(subj) - 1);
                        strncpy(rel, matches[j], sizeof(rel) - 1);
                        found_pair = 1;
                    }
                }
            }

            /* No exact pair stored: admit unknown. Guessing from
               relation counts fabricates answers. */
            (void)found_pair;
        }

        if (subj[0] == '\0' || rel[0] == '\0')
        {
            if (strchr(clean, '?'))
            {
                snprintf(out_response, max_len, "%s",
                         LangString(I18N_NO_RECORD, 0));
            }
            else
            {
                snprintf(out_response, max_len, "%s",
                         LangString(I18N_REPHRASE, 0));
            }
            return 1;
        }

        SYMBOL_ID sid = StemFindSymbol(graph->symbols, subj);
        SYMBOL_ID rid = StemFindSymbol(graph->symbols, rel);

        if (sid == SYMBOL_INVALID || rid == SYMBOL_INVALID)
        {
            char unk[256];
            GetUnknownResponse(unk, sizeof(unk), subj, rel);
            snprintf(out_response, max_len, "%s", unk);
            return 1;
        }

        RELATION *results[8];
        SYMBOL_ID resolved = sid;

        uint32_t found = GraphQuerySubjectRelationFuzzy(
            graph, sid, rid, results, 8, 0.70f, &resolved);

        if (found > 0)
        {
            /* Direct answer with confidence (i18n openers) */
            int n_open = LangVariantCount(I18N_OPENER);
            int idx_open = (int)(rand() % (n_open > 0 ? n_open : 1));
            snprintf(out_response, max_len, "%s",
                     LangString(I18N_OPENER, idx_open));

            /* Subject name */
            const SYMBOL *subj_sym = SymbolGet(graph->symbols, resolved);
            const char *subj_name = subj_sym ? subj_sym->name : subj;

            char *w = out_response + strlen(out_response);
            snprintf(w, max_len - strlen(out_response), "%s ", subj_name);

            /* Lowercase relation for grammar */
            const SYMBOL *rel_sym = SymbolGet(graph->symbols, rid);
            if (rel_sym && rel_sym->name[0])
            {
                w = out_response + strlen(out_response);
                char lc_rel[128];
                lc_rel[0] = (char)tolower((unsigned char)rel_sym->name[0]);
                strncpy(lc_rel + 1, rel_sym->name + 1, sizeof(lc_rel) - 2);
                lc_rel[sizeof(lc_rel) - 1] = '\0';
                snprintf(w, max_len - strlen(out_response), "%s ", lc_rel);
            }

            /* Objects with percentages */
            float total_w = 0;
            for (uint32_t i = 0; i < found; i++)
                total_w += results[i]->weight;

            for (uint32_t i = 0; i < found && i < 5; i++)
            {
                const SYMBOL *obj_sym = SymbolGet(graph->symbols, results[i]->object);
                float pct = total_w > 0 ? (results[i]->weight / total_w * 100.0f) : 0.0f;

                w = out_response + strlen(out_response);
                if (i > 0 && i == found - 1)
                    snprintf(w, max_len - strlen(out_response), "%s",
                             LangString(I18N_AND, 0));
                else if (i > 0)
                    snprintf(w, max_len - strlen(out_response), ", ");

                w = out_response + strlen(out_response);
                snprintf(w, max_len - strlen(out_response), "%s", obj_sym ? obj_sym->name : "?");

                if (found > 1)
                {
                    w = out_response + strlen(out_response);
                    snprintf(w, max_len - strlen(out_response), " (%.0f%%)", pct);
                }
            }

            w = out_response + strlen(out_response);
            snprintf(w, max_len - strlen(out_response), ".");

            /* Try inference for extra context */
            RELATION *taxo[2];
            SYMBOL_ID es_sym = SymbolFind(graph->symbols, "ES");
            if (es_sym != SYMBOL_INVALID && rid != es_sym)
            {
                if (GraphQuerySubjectRelation(graph, resolved, es_sym, taxo, 1) > 0)
                {
                    const SYMBOL *parent = SymbolGet(graph->symbols, taxo[0]->object);
                    if (parent)
                    {
                        w = out_response + strlen(out_response);
                        snprintf(w, max_len - strlen(out_response),
                                 LangString(I18N_TAXO, 0), parent->name);
                    }
                }
            }

            return 1;
        }

        /* No direct results: report unknown, keep it honest. */
        {
            char unk[256];
            GetUnknownResponse(unk, sizeof(unk), subj, rel);
            snprintf(out_response, max_len, "%s", unk);
        }
        return 1;
    }

    /* Learning statements: input → syntax tree → symbols → relations,
       with discourse tracking (the context variant preprocesses and
       pushes entities itself). */
    if (ParserIngestSentenceCtx(graph, ctx, user_input))
    {
        int n = LangVariantCount(I18N_LEARNED);
        snprintf(out_response, max_len, "%s",
                 LangString(I18N_LEARNED, rand() % (n > 0 ? n : 1)));
        return 1;
    }

    snprintf(out_response, max_len, "%s", LangString(I18N_EXTRACT, 0));
    return 1;
}
