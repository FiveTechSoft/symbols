#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "dialog.h"
#include "generator.h"
#include "nlg.h"
#include "inference.h"

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

    char upper[512];
    StrToUpper(input, upper, sizeof(upper));

    if (strstr(upper, "HOLA") || strstr(upper, "BUENOS DIAS") || strstr(upper, "BUENAS"))
    {
        intent.act = SPEECH_ACT_GREETING;
        intent.is_social_only = (strlen(input) <= 12);
        return intent;
    }

    if (strstr(upper, "GRACIAS") || strstr(upper, "EXCELENTE") || strstr(upper, "GENIAL"))
    {
        intent.act = SPEECH_ACT_GRATITUDE;
        intent.is_social_only = 1;
        return intent;
    }

    if (strstr(upper, "ADIOS") || strstr(upper, "HASTA LUEGO") || strstr(upper, "CHAO"))
    {
        intent.act = SPEECH_ACT_FAREWELL;
        intent.is_social_only = 1;
        return intent;
    }

    if (strstr(upper, "POR QUE") || strstr(upper, "POR QUÉ"))
    {
        intent.act = SPEECH_ACT_QUERY_WHY;
        return intent;
    }

    /* Identity questions: "quien eres", "como te llamas", ... */
    if (strstr(upper, "QUIEN ERES") || strstr(upper, "QUÉ ERES") ||
        strstr(upper, "QUE ERES") || strstr(upper, "COMO TE LLAMAS") ||
        strstr(upper, "CÓMO TE LLAMAS") || strstr(upper, "TU NOMBRE"))
    {
        intent.act = SPEECH_ACT_IDENTITY;
        return intent;
    }

    /* Capability questions: "que puedes hacer", "para que sirves", ... */
    if (strstr(upper, "QUE PUEDES") || strstr(upper, "QUÉ PUEDES") ||
        strstr(upper, "PARA QUE SIRVES") || strstr(upper, "PARA QUÉ SIRVES") ||
        strstr(upper, "QUE SABES HACER") || strstr(upper, "QUÉ SABES HACER") ||
        strstr(upper, "QUE HACES") || strstr(upper, "QUÉ HACES"))
    {
        intent.act = SPEECH_ACT_CAPABILITY;
        return intent;
    }

    if (strchr(input, '?') || strstr(input, "\xC2\xBF") || strstr(upper, "QUE ") || strstr(upper, "QUIEN "))
    {
        if (strstr(upper, "QUE ES") || strstr(upper, "QUÉ ES"))
            intent.act = SPEECH_ACT_QUERY_WHAT_IS;
        else
            intent.act = SPEECH_ACT_QUERY_FACT;
        return intent;
    }

    /* Pattern: "X ES" or "X TIENE" at end → question */
    {
        size_t slen = strlen(upper);
        if (slen >= 3 && strcmp(upper + slen - 3, " ES") == 0)
        {
            intent.act = SPEECH_ACT_QUERY_FACT;
            return intent;
        }
        if (slen >= 7 && strcmp(upper + slen - 7, " TIENE") == 0)
        {
            intent.act = SPEECH_ACT_QUERY_FACT;
            return intent;
        }
    }

    intent.act = SPEECH_ACT_STATEMENT;
    return intent;
}

static void GetUnknownResponse(char *out, size_t size, const char *subj, const char *pred)
{
    static const char *templates[] = {
        "Aun no tengo informacion sobre si %s %s. Te gustaria ensenarmelo?",
        "Ese dato aun no lo conozco. Nada sobre que %s %s.",
        "No dispongo de registros para %s y la accion %s. Si me lo explicas, lo recordare."
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

    /* Pure social acts */
    if (intent.act == SPEECH_ACT_GREETING && intent.is_social_only)
    {
        snprintf(out_response, max_len,
                 "Hola! En que te puedo ayudar? Puedes ensenarme datos o hacerme preguntas.");
        return 1;
    }

    if (intent.act == SPEECH_ACT_GRATITUDE)
    {
        static const char *resp[] = {
            "De nada! Aqui estoy para lo que necesites razonar o consultar.",
            "Un placer ayudarte. Hay algo mas que quieras verificar?",
            "Para eso estoy! Preguntame lo que quieras."
        };
        snprintf(out_response, max_len, "%s", resp[rand() % 3]);
        return 1;
    }

    if (intent.act == SPEECH_ACT_FAREWELL)
    {
        snprintf(out_response, max_len, "Hasta luego! Todo lo que me has ensenado queda listo.");
        return 1;
    }

    if (intent.act == SPEECH_ACT_IDENTITY)
    {
        snprintf(out_response, max_len,
                 "Soy un sistema de razonamiento simbolico basado en grafos de conocimiento. "
                 "No uso redes neuronales: mi conocimiento vive en relaciones explicitas "
                 "<Sujeto, Predicado, Objeto> y razono por encadenamiento con confianza atenuada.");
        return 1;
    }

    if (intent.act == SPEECH_ACT_CAPABILITY)
    {
        snprintf(out_response, max_len,
                 "Puedo aprender hechos nuevos, responder preguntas sobre el grafo, "
                 "razonar por encadenamiento profundo y detectar sinonimos con embeddings 32D. "
                 "Comandos: /query, /find S P O, /why S P, /analogy A B, /stats, /export <file.dot|.ttl>.");
        return 1;
    }

    /* ---- Pattern: "X ES" at end → query graph directly ---- */
    if (intent.act == SPEECH_ACT_QUERY_FACT)
    {
        char upper_input[512];
        size_t ulen = strlen(user_input);
        for (size_t i = 0; i < ulen; i++)
            upper_input[i] = (char)toupper((unsigned char)user_input[i]);
        upper_input[ulen] = '\0';

        /* Check if ends with " ES" */
        size_t slen = strlen(upper_input);
        if (slen >= 3 && strcmp(upper_input + slen - 3, " ES") == 0)
        {
            /* Extract subject: everything before " ES" */
            char subj[64] = {0};
            size_t subj_len = slen - 3;
            /* Remove leading article */
            const char *start = upper_input;
            if (strncmp(start, "EL ", 3) == 0) start += 3;
            else if (strncmp(start, "LA ", 3) == 0) start += 3;
            else if (strncmp(start, "LOS ", 4) == 0) start += 4;
            else if (strncmp(start, "LAS ", 4) == 0) start += 4;

            subj_len = strlen(start);
            if (subj_len >= 63) subj_len = 63;
            memcpy(subj, start, subj_len);
            subj[subj_len] = '\0';
            /* Remove trailing spaces */
            while (subj_len > 0 && subj[subj_len - 1] == ' ') { subj[--subj_len] = '\0'; }
            /* Normalize diacritics (ñ→n, á→a, etc.) */
            NormalizeDiacritics(subj);

            if (subj_len > 0)
            {
                /* Try exact match first */
                SYMBOL_ID sid = SymbolFind(graph->symbols, subj);
                SYMBOL_ID pid = SymbolFind(graph->symbols, "ES");

                /* If not found, try to find any symbol from the subject words */
                if (sid == SYMBOL_INVALID)
                {
                    char work[64];
                    strncpy(work, subj, sizeof(work) - 1);
                    work[sizeof(work) - 1] = '\0';
                    char *word = strtok(work, " ");
                    while (word != NULL)
                    {
                        sid = SymbolFind(graph->symbols, word);
                        if (sid != SYMBOL_INVALID) break;
                        word = strtok(NULL, " ");
                    }
                }

                if (sid != SYMBOL_INVALID && pid != SYMBOL_INVALID)
                {
                    RELATION *results[8];
                    uint32_t found = GraphQuerySubjectPredicate(graph, sid, pid, results, 8);
                    if (found > 0)
                    {
                        uint32_t idx = rand() % 4;
                        static const char *openers[] = {
                            "Segun lo que se, ",
                            "Tengo registrado que ",
                            "La respuesta es: ",
                            "Segun mi conocimiento, "
                        };
                        snprintf(out_response, max_len, "%s", openers[idx]);
                        char *w = out_response + strlen(out_response);

                        const SYMBOL *subj_sym = SymbolGet(graph->symbols, sid);
                        snprintf(w, max_len - strlen(out_response), "%s es ", subj_sym ? subj_sym->name : subj);
                        w += strlen(w);

                        for (uint32_t i = 0; i < found && i < 5; i++)
                        {
                            const SYMBOL *obj_sym = SymbolGet(graph->symbols, results[i]->object);
                            if (i > 0) { snprintf(w, max_len - strlen(out_response), ", "); w += 2; }
                            snprintf(w, max_len - strlen(out_response), "%s", obj_sym ? obj_sym->name : "?");
                            w += strlen(w);
                        }
                        snprintf(w, max_len - strlen(out_response), ".");
                        return 1;
                    }
                    else
                    {
                        snprintf(out_response, max_len,
                                 "No tengo informacion sobre eso. Ensename y lo recordare.");
                        return 1;
                    }
                }
            }
        }

        /* Check if ends with " TIENE" */
        if (slen >= 7 && strcmp(upper_input + slen - 7, " TIENE") == 0)
        {
            char subj[64] = {0};
            const char *start = upper_input;
            if (strncmp(start, "EL ", 3) == 0) start += 3;
            else if (strncmp(start, "LA ", 3) == 0) start += 3;
            else if (strncmp(start, "LOS ", 4) == 0) start += 4;
            else if (strncmp(start, "LAS ", 4) == 0) start += 4;

            size_t subj_len = strlen(start) - 7;
            if (subj_len >= 63) subj_len = 63;
            memcpy(subj, start, subj_len);
            subj[subj_len] = '\0';
            while (subj_len > 0 && subj[subj_len - 1] == ' ') { subj[--subj_len] = '\0'; }
            NormalizeDiacritics(subj);

            if (subj_len > 0)
            {
                SYMBOL_ID sid = SymbolFind(graph->symbols, subj);
                SYMBOL_ID pid = SymbolFind(graph->symbols, "TIENE");

                if (sid == SYMBOL_INVALID)
                {
                    char work[64];
                    strncpy(work, subj, sizeof(work) - 1);
                    work[sizeof(work) - 1] = '\0';
                    char *word = strtok(work, " ");
                    while (word != NULL)
                    {
                        sid = SymbolFind(graph->symbols, word);
                        if (sid != SYMBOL_INVALID) break;
                        word = strtok(NULL, " ");
                    }
                }

                if (sid != SYMBOL_INVALID && pid != SYMBOL_INVALID)
                {
                    RELATION *results[8];
                    uint32_t found = GraphQuerySubjectPredicate(graph, sid, pid, results, 8);
                    if (found > 0)
                    {
                        snprintf(out_response, max_len, "%s tiene: ", subj);
                        char *w = out_response + strlen(out_response);
                        for (uint32_t i = 0; i < found && i < 5; i++)
                        {
                            const SYMBOL *obj_sym = SymbolGet(graph->symbols, results[i]->object);
                            if (i > 0) { snprintf(w, max_len - strlen(out_response), ", "); w += 2; }
                            snprintf(w, max_len - strlen(out_response), "%s", obj_sym ? obj_sym->name : "?");
                            w += strlen(w);
                        }
                        snprintf(w, max_len - strlen(out_response), ".");
                        return 1;
                    }
                    else
                    {
                        snprintf(out_response, max_len,
                                 "No tengo informacion sobre eso. Ensename y lo recordare.");
                        return 1;
                    }
                }
            }
        }
    }

    /* Knowledge queries */
    if (intent.act == SPEECH_ACT_QUERY_FACT || intent.act == SPEECH_ACT_QUERY_WHAT_IS)
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

            /* Fallback: just pick the one with most relations as subject */
            if (!found_pair)
            {
                uint32_t best_count = 0;
                for (uint32_t i = 0; i < match_count; i++)
                {
                    SYMBOL_ID try_subj = SymbolFind(graph->symbols, matches[i]);
                    if (try_subj == SYMBOL_INVALID) continue;

                    RELATION *dummy[4];
                    uint32_t n = GraphQuerySubject(graph, try_subj, dummy, 4);
                    if (n > best_count)
                    {
                        best_count = n;
                        strncpy(subj, matches[i], sizeof(subj) - 1);
                    }
                }
                /* Predicate = any other match */
                for (uint32_t i = 0; i < match_count; i++)
                {
                    if (strcmp(matches[i], subj) != 0)
                    {
                        strncpy(pred, matches[i], sizeof(pred) - 1);
                        break;
                    }
                }
            }
        }

        if (subj[0] == '\0' || pred[0] == '\0')
        {
            /* If looks like a question, respond with polite ignorance */
            if (strstr(upper, "QUE ") || strstr(upper, "CUAL ") ||
                strchr(clean, '?'))
            {
                snprintf(out_response, max_len,
                         "No dispongo de informacion sobre eso. "
                         "Preguntame algo que haya ensenado antes.");
            }
            else
            {
                snprintf(out_response, max_len,
                         "No logre identificar el sujeto y predicado. "
                         "Reformula la pregunta.");
            }
            return 1;
        }

        SYMBOL_ID sid = SymbolFind(graph->symbols, subj);
        SYMBOL_ID pid = SymbolFind(graph->symbols, pred);

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

        /* No direct results, try inference */
        INFERENCE_CONFIG cfg = InferenceConfigDefault();
        INFERENCE_PATH path;
        int proven = InferenceProve(graph, sid, pid, SYMBOL_INVALID, &cfg, &path);

        if (proven && path.depth >= 2)
        {
            uint32_t idx = rand() % 4;
            static const char *inf_openers[] = {
                "Razonando por encadenamiento, ",
                "Por deduccion transitiva, ",
                "Siguiendo la cadena logica, ",
                "Mediante razonamiento profundo, "
            };
            snprintf(out_response, max_len, "%s", inf_openers[idx]);

            char *w = out_response + strlen(out_response);
            const SYMBOL *s = SymbolGet(graph->symbols, path.step_nodes[0]);
            snprintf(w, max_len - strlen(out_response), "%s ", s ? s->name : subj);

            for (uint32_t i = 0; i < path.depth; i++)
            {
                const SYMBOL *p = SymbolGet(graph->symbols, path.step_predicates[i]);
                const SYMBOL *o = SymbolGet(graph->symbols, path.step_nodes[i + 1]);

                w = out_response + strlen(out_response);
                if (p && p->name[0])
                {
                    char lc[128];
                    lc[0] = (char)tolower((unsigned char)p->name[0]);
                    strncpy(lc + 1, p->name + 1, sizeof(lc) - 2);
                    lc[sizeof(lc) - 1] = '\0';
                    snprintf(w, max_len - strlen(out_response), "%s %s ", lc, o ? o->name : "?");
                }
            }

            w = out_response + strlen(out_response);
            snprintf(w, max_len - strlen(out_response),
                     " (confianza: %.0f%%, %u saltos).",
                     path.accumulated_confidence * 100.0f, path.depth - 1);
            return 1;
        }

        {
            char unk[256];
            GetUnknownResponse(unk, sizeof(unk), subj, pred);
            snprintf(out_response, max_len, "%s", unk);
        }
        return 1;
    }

    /* WHY queries */
    if (intent.act == SPEECH_ACT_QUERY_WHY)
    {
        snprintf(out_response, max_len,
                 "Para responder necesito un hecho concreto. Reformula como: 'Por que X tiene Y?'");
        return 1;
    }

    /* Learning statements */
    char resolved_buf[256];
    ContextPreprocessSentence(ctx, user_input, resolved_buf, sizeof(resolved_buf));

    if (LearningSentence(graph, resolved_buf))
    {
        static const char *learn_confirms[] = {
            "Entendido, he incorporado esa relacion a mi conocimiento.",
            "Tomo nota. He actualizado mi memoria con ese dato.",
            "Dato guardado. Ahora puedo razonar sobre ello si me preguntas.",
            "Anotado! Ya forma parte de lo que se."
        };
        snprintf(out_response, max_len, "%s", learn_confirms[rand() % 4]);
        return 1;
    }

    snprintf(out_response, max_len,
             "Disculpa, no logre extraer una relacion clara. "
             "Podrias expresarlo como 'Sujeto Predicado Objeto'?");
    return 1;
}
