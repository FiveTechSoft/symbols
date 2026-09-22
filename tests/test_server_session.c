/* test_server_session: cross-session isolation for the OpenAI-compatible
   server. OpenCode 1.18.x sends no session identifier, so the server
   derives a stable key from the request prefix (first system message,
   first user message). Independent sessions sharing one server process
   must never see each other's state. */
#include <stdio.h>
#include <string.h>
#include "server_proto.h"

static int failures = 0;

static void check(const char *name, int ok)
{
    if (!ok)
    {
        printf("FAIL: %s\n", name);
        failures++;
    }
    else
    {
        printf("ok: %s\n", name);
    }
}

static void check_str(const char *name, const char *got, const char *want)
{
    if (strcmp(got, want) != 0)
    {
        printf("FAIL: %s (got '%s', want '%s')\n", name, got, want);
        failures++;
    }
    else
    {
        printf("ok: %s\n", name);
    }
}

static const char *body_session_a_turn1 =
    "{\"messages\":["
    "{\"role\":\"system\",\"content\":\"<env> Working directory: /repo/a </env>\"},"
    "{\"role\":\"user\",\"content\":\"crea test.txt\"}"
    "]}";

static const char *body_session_a_later_turn =
    "{\"messages\":["
    "{\"role\":\"system\",\"content\":\"<env> Working directory: /repo/a </env>\"},"
    "{\"role\":\"user\",\"content\":\"crea test.txt\"},"
    "{\"role\":\"assistant\",\"content\":\"Creado `test.txt`.\"},"
    "{\"role\":\"user\",\"content\":\"añade otra linea\"}"
    "]}";

static const char *body_session_b_other_dir =
    "{\"messages\":["
    "{\"role\":\"system\",\"content\":\"<env> Working directory: /repo/b </env>\"},"
    "{\"role\":\"user\",\"content\":\"crea test.txt\"}"
    "]}";

static const char *body_session_c_other_prompt =
    "{\"messages\":["
    "{\"role\":\"system\",\"content\":\"<env> Working directory: /repo/a </env>\"},"
    "{\"role\":\"user\",\"content\":\"borra test.txt\"}"
    "]}";

int main(void)
{
    char key_a[64], key_a2[64], key_b[64], key_c[64], buf[64];

    printf("test_server_session\n");

    /* explicit identifiers still win over derivation */
    check("explicit user field extracted",
          ServerExtractSession("{\"user\":\"session-abc\",\"messages\":[]}",
                               buf, sizeof(buf)) == 1);
    check_str("explicit user value", buf, "session-abc");

    /* derived key: stable prefix of one session */
    check("derive key from session A turn 1",
          ServerDeriveSessionKey(body_session_a_turn1, key_a, sizeof(key_a)) == 1);
    check("derive key from session A later turn",
          ServerDeriveSessionKey(body_session_a_later_turn, key_a2, sizeof(key_a2)) == 1);
    check("derived key has auto prefix", strncmp(key_a, "auto-", 5) == 0);
    check_str("same session keeps key across turns", key_a2, key_a);

    /* independent sessions never share a key */
    check("derive key from session B",
          ServerDeriveSessionKey(body_session_b_other_dir, key_b, sizeof(key_b)) == 1);
    check("different working directory isolates the session",
          strcmp(key_a, key_b) != 0);
    check("derive key from session C",
          ServerDeriveSessionKey(body_session_c_other_prompt, key_c, sizeof(key_c)) == 1);
    check("different opening prompt isolates the session",
          strcmp(key_a, key_c) != 0);

    /* no usable prefix: no key, caller keeps "default" */
    check("derive returns 0 without messages",
          ServerDeriveSessionKey("{\"messages\":[]}", buf, sizeof(buf)) == 0);
    check("derive returns 0 on garbage",
          ServerDeriveSessionKey("not json", buf, sizeof(buf)) == 0);

    if (failures != 0)
    {
        printf("test_server_session: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_server_session: all checks passed\n");
    return 0;
}
