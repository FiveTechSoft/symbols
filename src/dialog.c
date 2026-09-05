#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "dialog.h"
#include "stem.h"
#include "parser.h"
#include "generator.h"
#include "nlg.h"

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

static void GetUnknownResponse(char *out, size_t size, const char *subj, const char *pred)
{
    static const char *templates[] = {
        "No record of %s %s yet. Teach it to me.",
        "Nothing stored about %s %s.",
        "Unknown: %s %s. Use /learn S P O to store it."
    };
    snprintf(out, size, templates[rand() % 3], subj, pred);
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
        static const char *resp[] = {
            "Hello.",
            "Noted.",
            "I store symbols and relations. Teach me with /learn S P O."
        };
        snprintf(out_response, max_len, "%s", resp[rand() % 3]);
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
                snprintf(out_response, max_len, "%s", answer);
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
        char pred[64] = "";

        /* Find all matching symbols */
        char matches[16][64];
        uint32_t match_count = 0;

        uint32_t sym_count = SymbolCount(graph->symbols);
        for (uint32_t i = 0; i < sym_count && match_count < 16; i++)
        {
            const SYMBOL *sym = SymbolGet(graph->symbols, (SYMBOL_ID)i);
            if (sym == NULL || sym->name == NULL) continue;
            if (sym->name[0] == '\0') continue;
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
                    SYMBOL_ID try_pred = SymbolFind(graph->symbols, matches[j]);
                    if (try_pred == SYMBOL_INVALID) continue;

                    RELATION *r[1];
                    uint32_t n = GraphQuerySubjectPredicate(graph, try_subj, try_pred, r, 1);
                    if (n > 0)
                    {
                        strncpy(subj, matches[i], sizeof(subj) - 1);
                        strncpy(pred, matches[j], sizeof(pred) - 1);
                        found_pair = 1;
                    }
                }
            }

            /* No exact pair stored: admit unknown. Guessing from
               relation counts fabricates answers. */
            (void)found_pair;
        }

        if (subj[0] == '\0' || pred[0] == '\0')
        {
            if (strchr(clean, '?'))
            {
                snprintf(out_response, max_len,
                         "No record of that. Ask about something stored.");
            }
            else
            {
                snprintf(out_response, max_len,
                         "Could not identify symbols and relation. "
                         "Rephrase it.");
            }
            return 1;
        }

        SYMBOL_ID sid = StemFindSymbol(graph->symbols, subj);
        SYMBOL_ID pid = StemFindSymbol(graph->symbols, pred);

        if (sid == SYMBOL_INVALID || pid == SYMBOL_INVALID)
        {
            char unk[256];
            GetUnknownResponse(unk, sizeof(unk), subj, pred);
            snprintf(out_response, max_len, "%s", unk);
            return 1;
        }

        RELATION *results[8];
        SYMBOL_ID resolved = sid;

        uint32_t found = GraphQuerySubjectPredicateFuzzy(
            graph, sid, pid, results, 8, 0.70f, &resolved);

        if (found > 0)
        {
            /* Direct answer with confidence */
            uint32_t idx = rand() % 6;
            static const char *openers[] = {
                "Segun lo que he aprendido, ",
                "Tengo registrado que ",
                "Efectivamente, ",
                "Por la informacion que manejo, ",
                "La evidencia indica que ",
                "He encontrado que "
            };
            snprintf(out_response, max_len, "%s", openers[idx]);

            /* Subject name */
            const SYMBOL *subj_sym = SymbolGet(graph->symbols, resolved);
            const char *subj_name = subj_sym ? subj_sym->name : subj;

            char *w = out_response + strlen(out_response);
            snprintf(w, max_len - strlen(out_response), "%s ", subj_name);

            /* Lowercase predicate for grammar */
            const SYMBOL *pred_sym = SymbolGet(graph->symbols, pid);
            if (pred_sym && pred_sym->name[0])
            {
                w = out_response + strlen(out_response);
                char lc_pred[128];
                lc_pred[0] = (char)tolower((unsigned char)pred_sym->name[0]);
                strncpy(lc_pred + 1, pred_sym->name + 1, sizeof(lc_pred) - 2);
                lc_pred[sizeof(lc_pred) - 1] = '\0';
                snprintf(w, max_len - strlen(out_response), "%s ", lc_pred);
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
                    snprintf(w, max_len - strlen(out_response), " y ");
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
            if (es_sym != SYMBOL_INVALID && pid != es_sym)
            {
                if (GraphQuerySubjectPredicate(graph, resolved, es_sym, taxo, 1) > 0)
                {
                    const SYMBOL *parent = SymbolGet(graph->symbols, taxo[0]->object);
                    if (parent)
                    {
                        w = out_response + strlen(out_response);
                        snprintf(w, max_len - strlen(out_response),
                                 " (que es un %s)", parent->name);
                    }
                }
            }

            return 1;
        }

        /* No direct results: report unknown, keep it honest. */
        {
            char unk[256];
            GetUnknownResponse(unk, sizeof(unk), subj, pred);
            snprintf(out_response, max_len, "%s", unk);
        }
        return 1;
    }

    /* Learning statements: input → syntax tree → symbols → relations */
    char resolved_buf[256];
    ContextPreprocessSentence(ctx, user_input, resolved_buf, sizeof(resolved_buf));

    if (ParserIngestSentence(graph, resolved_buf))
    {
        static const char *learn_confirms[] = {
            "Stored.",
            "Noted. Symbols and relation saved.",
            "Saved. Ask me about it."
        };
        snprintf(out_response, max_len, "%s", learn_confirms[rand() % 3]);
        return 1;
    }

    snprintf(out_response, max_len,
             "Could not extract a clear relation. "
             "Try it as 'symbol relation symbol'.");
    return 1;
}
