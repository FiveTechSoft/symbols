/* test_server_proto: wire layer pins (pure, deterministic).
   JSON extraction (last-user-wins, \u escapes), response shape,
   unknown mapping, parse inspection, observation line. Engine
   assertions use the real corpus (fast, read-only). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "schema.h"
#include "metaschema.h"
#include "learn.h"
#include "chat.h"
#include "server_proto.h"

static int g_pass = 0, g_fail = 0;

static void check(const char *name, int cond)
{
    if (cond)
    {
        printf("  PASS %s\n", name);
        g_pass++;
    }
    else
    {
        printf("  FAIL %s\n", name);
        g_fail++;
    }
}

static void check_str(const char *name, const char *got,
                      const char *want)
{
    if (got != NULL && strcmp(got, want) == 0)
    {
        printf("  PASS %s\n", name);
        g_pass++;
    }
    else
    {
        printf("  FAIL %s\n    got  %.160s\n    want %.160s\n", name,
               got ? got : "(null)", want);
        g_fail++;
    }
}

int main(void)
{
    char buf[8192];
    CHAT ch;

    /* ---- extraction ---- */
    check("extract simple",
          ServerExtractQuery("{\"messages\":[{\"role\":\"user\","
                             "\"content\":\"Who is the father of "
                             "David?\"}]}",
                             buf, sizeof(buf)) == 1);
    check_str("extracted text", buf, "Who is the father of David?");
    check("last user wins",
          ServerExtractQuery("{\"messages\":[{\"role\":\"user\","
                             "\"content\":\"first\"},{\"role\":"
                             "\"assistant\",\"content\":\"x\"},{"
                             "\"role\":\"user\",\"content\":\"second\"}]}",
                             buf, sizeof(buf)) == 1);
    check_str("last wins text", buf, "second");
    check("system ignored, user taken",
          ServerExtractQuery("{\"messages\":[{\"role\":\"system\","
                             "\"content\":\"sys\"},{\"role\":\"user\","
                             "\"content\":\"q\"}]}",
                             buf, sizeof(buf)) == 1);
    check_str("user after system", buf, "q");
    check("unicode escape",
          ServerExtractQuery("{\"messages\":[{\"role\":\"user\","
                             "\"content\":\"Qui\\u00e9n es?\"}]}",
                             buf, sizeof(buf)) == 1);
    check_str("unescaped", buf, "Qui\xc3\xa9n es?");
    check("no user role",
          ServerExtractQuery("{\"messages\":[{\"role\":\"assistant\","
                             "\"content\":\"x\"}]}",
                             buf, sizeof(buf)) == 0);
    check("garbage",
          ServerExtractQuery("not json", buf, sizeof(buf)) == 0);

    /* ---- session extraction ---- */
    check("session user field",
          ServerExtractSession("{\"user\":\"session-abc\",\"messages\":[]}",
                               buf, sizeof(buf)) == 1);
    check_str("session user value", buf, "session-abc");
    check("session_id field",
          ServerExtractSession("{\"session_id\":\"sess-42\",\"messages\":[]}",
                               buf, sizeof(buf)) == 1);
    check_str("session_id value", buf, "sess-42");
    check("session absent returns 0",
          ServerExtractSession("{\"messages\":[]}",
                               buf, sizeof(buf)) == 0);

    /* ---- response shape ---- */
    check("build response",
          ServerBuildResponse("symbols", 1726000000L, 7,
                              "El padre de David es Jesse.",
                              "Who is the father of David?", buf,
                              sizeof(buf)) == 1);
    check("response has id",
          strstr(buf, "\"id\":\"chatcmpl-symbols-7\"") != NULL);
    check("response has model",
          strstr(buf, "\"model\":\"symbols\"") != NULL);
    check("response has content",
          strstr(buf, "El padre de David es Jesse.") != NULL);
    check("response stop",
          strstr(buf, "\"finish_reason\":\"stop\"") != NULL);
    check("build models",
          ServerBuildModels("symbols", buf, sizeof(buf)) == 1);
    check("models has id", strstr(buf, "\"id\":\"symbols\"") != NULL);

    /* ---- unknown mapping ---- */
    check("parse-fail unknown",
          ServerIsUnknown("No entendi la pregunta.") == 1);
    check("constancia unknown",
          ServerIsUnknown("No tengo constancia del padre de X.") == 1);
    check("echo unknown",
          ServerIsUnknown("No tengo constancia suficiente para "
                          "responder a \"x\".") == 1);
    check("ambiguous unknown",
          ServerIsUnknown("Hay 2 constancias del padre de James.") ==
              1);
    check("answer known",
          ServerIsUnknown("El padre de David es Jesse.") == 0);
    check_str("status abstain",
              ServerStatusOf("No entendi la pregunta."), "ABSTAIN");
    check_str("status ambiguous",
              ServerStatusOf("Hay 2 constancias."), "AMBIGUOUS");
    check_str("status unknown",
              ServerStatusOf("No tengo constancia de x."),
              "UNKNOWN");
    check_str("status answer",
              ServerStatusOf("El padre de David es Jesse."), "ANSWER");

    /* ---- engine: parse inspection + answer + map ---- */
    ChatInit(&ch, "data/bible/bible_relations.tsv");
    {
        ChatParse p;
        check("parse father",
              ChatParseLine(&ch, "Who is the father of David?", &p) ==
                  1);
        check_str("intent", p.intent, "PARENT_OF");
        check_str("slot_a", p.slot_a, "david");
    }
    {
        ChatParse p;
        check("parse fail",
              ChatParseLine(&ch, "blorb xyzzy nonsense", &p) == 0);
        check_str("fail intent", p.intent, "NONE");
    }
    {
        char raw[4096], mapped[4096];
        check("answer query",
              ServerAnswerQuery(&ch, "Who is the father of David?",
                                raw, sizeof(raw)) == 1);
        check("answer has Jesse", strstr(raw, "Jesse") != NULL);
        check("map keeps answer",
              ServerMapContent(raw, mapped, sizeof(mapped)) == 1);
        check("mapped has Jesse", strstr(mapped, "Jesse") != NULL);
    }
    {
        char raw[4096], mapped[4096];
        check("answer unknown",
              ServerAnswerQuery(&ch, "Who is the father of Babylonia?",
                                raw, sizeof(raw)) == 1);
        check("raw is constancia",
              strncmp(raw, "No tengo constancia",
                      sizeof("No tengo constancia") - 1) == 0);
        check("map to idk",
              ServerMapContent(raw, mapped, sizeof(mapped)) == 1);
        check_str("mapped idk", mapped, "I don't know.");
    }

    /* ---- observation line ---- */
    {
        ChatParse p;
        char obs[8192];
        memset(&p, 0, sizeof(p));
        ChatParseLine(&ch, "Who is the father of David?", &p);
        check("build observation",
              ServerBuildObservation("2026-09-18T00:00:00Z", "symbols",
                                     2, 0, "Who is the father of "
                                     "David?",
                                     &p, "", "[\"david\",\"jesse\"]",
                                     "[\"G1 KB ANSWER\"]",
                                     "El padre de David es Jesse.",
                                     "ANSWER", 3, obs,
                                     sizeof(obs)) == 1);
        check("obs has intent",
              strstr(obs, "\"intent\":\"PARENT_OF\"") != NULL);
        check("obs has status",
              strstr(obs, "\"status\":\"ANSWER\"") != NULL);
        check("obs has latency",
              strstr(obs, "\"latency_ms\":3") != NULL);
    }

    /* ---- streaming ---- */
    check("stream true detected",
          ServerWantsStream("{\"model\":\"symbols\",\"stream\":true,"
                            "\"messages\":[]}") == 1);
    check("stream false",
          ServerWantsStream("{\"model\":\"symbols\",\"stream\":false,"
                            "\"messages\":[]}") == 0);
    check("stream absent",
          ServerWantsStream("{\"model\":\"symbols\"}") == 0);
    check("stream in content string ignored",
          ServerWantsStream("{\"messages\":[{\"role\":\"user\","
                            "\"content\":\"say \\\"stream\\\": true "
                            "loud\"}]}") == 0);
    {
        char sse[8192];
        check("build sse",
              ServerBuildStreamResponse("symbols", 1726000000L, 9,
                                        "El padre de David es Jesse.",
                                        sse, sizeof(sse)) == 1);
        check("sse has delta",
              strstr(sse, "\"delta\":{\"role\":\"assistant\"") != NULL);
        check("sse has chunk",
              strstr(sse, "\"object\":\"chat.completion.chunk\"") !=
                  NULL);
        check("sse finish stop",
              strstr(sse, "\"finish_reason\":\"stop\"") != NULL);
        check("sse done marker",
              strstr(sse, "data: [DONE]") != NULL);
    }

    printf("test_server_proto: %d passed, %d failed\n", g_pass,
           g_fail);
    return g_fail == 0 ? 0 : 1;
}
