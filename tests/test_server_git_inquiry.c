/* test_server_git_inquiry: native read-only Git inquiry routing.
   Natural-language repository state questions route to agent_git and
   are answered from one fresh inspection snapshot; literal commands
   stay on the shell route and mutation phrasing never matches. */
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

static void check_contains(const char *name, const char *hay,
                           const char *needle)
{
    if (strstr(hay, needle) == NULL)
    {
        printf("FAIL: %s (missing '%s' in '%s')\n", name, needle, hay);
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

static const char *body_with_env =
    "{\"messages\":["
    "{\"role\":\"system\",\"content\":\"prompt <env>\\n"
    "  Working directory: /tmp/oc-git\\n"
    "  Workspace root folder: /\\n"
    "  Is directory a git repo: yes\\n"
    "</env>\"},"
    "{\"role\":\"user\",\"content\":\"estado del repo\"}"
    "]}";

static const char *body_windows_env =
    "{\"messages\":["
    "{\"role\":\"system\",\"content\":\"<env>\\n"
    "  Working directory: C:\\\\work\\\\proj\\n"
    "</env>\"},"
    "{\"role\":\"user\",\"content\":\"estado del repo\"}"
    "]}";

static const char *body_user_spoof =
    "{\"messages\":["
    "{\"role\":\"system\",\"content\":\"no env here\"},"
    "{\"role\":\"user\",\"content\":\"Working directory: /etc\"}"
    "]}";

int main(void)
{
    char out[4096];

    /* --- inquiry intent: positives --- */
    check("inquiry: en que rama estoy",
          ServerIsGitInquiryTask("¿en qué rama estoy?"));
    check("inquiry: rama actual del repo",
          ServerIsGitInquiryTask("cual es la rama actual del repo"));
    check("inquiry: estado del repositorio",
          ServerIsGitInquiryTask("dime el estado del repositorio"));
    check("inquiry: cambios sin confirmar",
          ServerIsGitInquiryTask("hay cambios sin confirmar en el repo?"));
    check("inquiry: archivos ignorados",
          ServerIsGitInquiryTask("que archivos ignorados hay en el repo"));
    check("inquiry: current branch",
          ServerIsGitInquiryTask("current branch?"));
    check("inquiry: working tree",
          ServerIsGitInquiryTask("how is the working tree looking"));
    check("inquiry: dirty",
          ServerIsGitInquiryTask("is the repo dirty?"));
    check("inquiry: ultimo commit",
          ServerIsGitInquiryTask("cuál es el último commit del repo"));

    /* --- inquiry intent: negatives --- */
    check("inquiry: literal git status stays on shell route",
          !ServerIsGitInquiryTask("git status"));
    check("inquiry: uname is not git",
          !ServerIsGitInquiryTask("uname -a"));
    check("inquiry: line swap is not git",
          !ServerIsGitInquiryTask("intercambia las líneas"));
    check("inquiry: create file is not git",
          !ServerIsGitInquiryTask("crea test.txt"));
    check("inquiry: corpus question is not git",
          !ServerIsGitInquiryTask("qué es un puntero en C"));
    check("inquiry: commit mutation rejected",
          !ServerIsGitInquiryTask("haz commit de los cambios en el repo"));
    check("inquiry: new branch mutation rejected",
          !ServerIsGitInquiryTask("crea una rama nueva en el repo"));
    check("inquiry: checkout mutation rejected",
          !ServerIsGitInquiryTask("git checkout master"));
    check("inquiry: cambia de rama rejected",
          !ServerIsGitInquiryTask("cambia de rama a develop"));

    /* --- preflight intent --- */
    check("preflight: puedo aplicar cambios",
          ServerIsGitPreflightTask("¿puedo aplicar cambios en el repo?"));
    check("preflight: repo listo",
          ServerIsGitPreflightTask("¿está listo el repo para trabajar?"));
    check("preflight: is the repo ready",
          ServerIsGitPreflightTask("is the repo ready?"));
    check("preflight: safe to edit",
          ServerIsGitPreflightTask("safe to edit this repo?"));
    check("preflight: no repo context",
          !ServerIsGitPreflightTask("¿puedo editar test.txt?"));
    check("preflight: mutation rejected",
          !ServerIsGitPreflightTask("haz push cuando el repo esté listo"));
    check("preflight: plain statement is not a readiness question",
          !ServerIsGitPreflightTask("el repo tiene dos archivos"));

    /* --- working dir extraction --- */
    check("workdir: env block extracted",
          ServerExtractWorkingDir(body_with_env, out, sizeof(out)));
    check_str("workdir: exact path", out, "/tmp/oc-git");
    check("workdir: windows env extracted",
          ServerExtractWorkingDir(body_windows_env, out, sizeof(out)));
    check_str("workdir: windows backslashes collapse", out, "C:\\work\\proj");
    check("workdir: user spoof rejected",
          !ServerExtractWorkingDir(body_user_spoof, out, sizeof(out)));
    check("workdir: no env rejected",
          !ServerExtractWorkingDir("{\"messages\":[]}", out, sizeof(out)));

    /* --- status composition --- */
    {
        GIT_REPOSITORY_STATE st;
        memset(&st, 0, sizeof(st));
        snprintf(st.head, sizeof(st.head), "%s",
                 "abcdef1234567890abcdef1234567890abcdef12");
        snprintf(st.branch, sizeof(st.branch), "%s", "main");
        st.ignored_paths = 2;
        ServerComposeGitStatusAnswer(&st, out, sizeof(out));
        check_contains("compose: clean branch", out, "Rama `main`, HEAD `abcdef12`.");
        check_contains("compose: clean tree", out, "Árbol limpio");
        check_contains("compose: ignored not dirty", out,
                       "2 ruta(s) ignorada(s) (no cuentan como cambio).");

        st.staged_paths = 1;
        st.unstaged_paths = 2;
        st.untracked_paths = 3;
        ServerComposeGitStatusAnswer(&st, out, sizeof(out));
        check_contains("compose: dirty counts", out,
                       "Cambios: 1 staged, 2 unstaged, 3 untracked.");

        memset(&st, 0, sizeof(st));
        snprintf(st.head, sizeof(st.head), "%s",
                 "abcdef1234567890abcdef1234567890abcdef12");
        st.detached_head = 1;
        ServerComposeGitStatusAnswer(&st, out, sizeof(out));
        check_contains("compose: detached", out, "HEAD desacoplado en `abcdef12`");

        st.conflicted_paths = 2;
        ServerComposeGitStatusAnswer(&st, out, sizeof(out));
        check_contains("compose: conflicts", out,
                       "2 ruta(s) con conflictos de merge sin resolver");
    }

    /* --- inspection failure composition --- */
    ServerComposeGitInspectFailure(GIT_INSPECT_NOT_REPOSITORY, "", "/tmp/x",
                                   out, sizeof(out));
    check_contains("compose: not a repo", out,
                   "`/tmp/x` no está dentro de un repositorio Git");
    ServerComposeGitInspectFailure(GIT_INSPECT_MALFORMED_OUTPUT, "", "/tmp/x",
                                   out, sizeof(out));
    check_contains("compose: unreliable output abstains", out,
                   "prefiero no adivinar el estado");

    /* --- preflight composition --- */
    {
        GIT_REPOSITORY_STATE obs;
        memset(&obs, 0, sizeof(obs));
        snprintf(obs.head, sizeof(obs.head), "%s",
                 "bbbbbbbbaaaaaaaabbbbbbbbaaaaaaaabbbbbbbb");
        snprintf(obs.branch, sizeof(obs.branch), "%s", "master");
        ServerComposeGitPreflightAnswer(GIT_PREFLIGHT_READY, &obs, NULL,
                                        out, sizeof(out));
        check_contains("preflight: ready", out,
                       "Listo para trabajar: rama `master`, HEAD `bbbbbbbb`");

        snprintf(obs.head, sizeof(obs.head), "%s",
                 "ccccccccaaaaaaaabbbbbbbbaaaaaaaabbbbbbbb");
        ServerComposeGitPreflightAnswer(
            GIT_PREFLIGHT_STALE_HEAD, &obs,
            "bbbbbbbbaaaaaaaabbbbbbbbaaaaaaaabbbbbbbb", out, sizeof(out));
        check_contains("preflight: stale names both heads", out,
                       "era `bbbbbbbb`, ahora es `cccccccc`");

        obs.staged_paths = 1;
        obs.unstaged_paths = 2;
        obs.untracked_paths = 3;
        ServerComposeGitPreflightAnswer(GIT_PREFLIGHT_DIRTY_TREE, &obs, NULL,
                                        out, sizeof(out));
        check_contains("preflight: dirty abstains", out,
                       "Me abstengo: el árbol tiene cambios sin confirmar"
                       " (1 staged, 2 unstaged, 3 untracked).");

        memset(&obs, 0, sizeof(obs));
        snprintf(obs.head, sizeof(obs.head), "%s",
                 "ddddddddaaaaaaaabbbbbbbbaaaaaaaabbbbbbbb");
        obs.detached_head = 1;
        ServerComposeGitPreflightAnswer(GIT_PREFLIGHT_DETACHED_HEAD, &obs,
                                        NULL, out, sizeof(out));
        check_contains("preflight: detached abstains", out,
                       "Me abstengo: HEAD desacoplado en `dddddddd`");

        obs.conflicted_paths = 4;
        ServerComposeGitPreflightAnswer(GIT_PREFLIGHT_CONFLICTS, &obs, NULL,
                                        out, sizeof(out));
        check_contains("preflight: conflicts abstain", out,
                       "Me abstengo: hay 4 ruta(s) con conflictos de merge");
    }

    if (failures != 0)
    {
        printf("test_server_git_inquiry: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_server_git_inquiry: all checks passed\n");
    return 0;
}
