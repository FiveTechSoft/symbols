/* Actual child environment and parser driver. Private fault seam only here. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "agent_shell.h"
#include "agent_diagnose.h"
#include "agent_runner.h"
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#else
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#endif

void AgentShellCompilerEnvTestFailAfter(long allocations);
static int failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "failed line %d: %s\n", __LINE__, #c); ++failures; } } while (0)

static const char *value(const char *key) { const char *v = getenv(key); return v ? v : "<missing>"; }
static int child(void)
{
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    printf("LC_ALL=%s\nLC_MESSAGES=%s\nLANG=%s\nVSLANG=%s\nSYMBOLS_E_SENTINEL=%s\n",
           value("LC_ALL"), value("LC_MESSAGES"), value("LANG"), value("VSLANG"), value("SYMBOLS_E_SENTINEL"));
#ifdef _WIN32
    {
        char *block = GetEnvironmentStringsA(), *p;
        unsigned lc = 0, lm = 0, lang = 0, vs = 0, drive = 0;
        if (!block) return 7;
        for (p = block; *p; p += strlen(p) + 1) {
            if (!_strnicmp(p, "LC_ALL=", 7)) ++lc;
            if (!_strnicmp(p, "LC_MESSAGES=", 12)) ++lm;
            if (!_strnicmp(p, "LANG=", 5)) ++lang;
            if (!_strnicmp(p, "VSLANG=", 7)) ++vs;
            if (!strcmp(p, "=C:=C:\\")) ++drive;
        }
        FreeEnvironmentStringsA(block);
        printf("KEY_COUNTS=%u,%u,%u,%u\nDRIVE_ENTRY=%u\n", lc, lm, lang, vs, drive);
    }
#endif
    printf("PATH=%s\n", value("PATH"));
    return 0;
}

static char *environment_snapshot(size_t *bytes)
{
    size_t n = 1;
    char *copy;
#ifndef _WIN32
    char *out;
#endif
#ifdef _WIN32
    char *block = GetEnvironmentStringsA(), *p;
    if (!block) return NULL;
    for (p = block; *p; p += strlen(p) + 1) n += strlen(p) + 1;
    copy = malloc(n);
    if (copy) memcpy(copy, block, n);
    FreeEnvironmentStringsA(block);
#else
    extern char **environ;
    char **p;
    for (p = environ; p && *p; ++p) n += strlen(*p) + 1;
    copy = malloc(n);
    if (copy) {
        out = copy;
        for (p = environ; p && *p; ++p) { size_t len = strlen(*p) + 1; memcpy(out, *p, len); out += len; }
        *out = '\0';
    }
#endif
    *bytes = n;
    return copy;
}

static const char *cwd_arg, *command_arg;
static int run_mode(int compiler)
{
    SHELL_EXEC_RESULT *r = malloc(sizeof(*r));
    int rc;
    if (!r) return 0;
    rc = compiler ? AgentShellExecCompiler(command_arg, cwd_arg, 5000, r)
                  : AgentShellExec(command_arg, cwd_arg, 5000, r);
    if (!rc || r->exit_code || r->execution_failed || r->timed_out ||
        r->stdout_truncated || r->stderr_truncated) { free(r); return 0; }
    if (compiler) {
        rc = strstr(r->stdout_buf, "LC_ALL=C\nLC_MESSAGES=C\nLANG=C\n") != NULL;
#ifdef _WIN32
        rc = rc && strstr(r->stdout_buf, "VSLANG=1033\n") != NULL &&
             strstr(r->stdout_buf, "KEY_COUNTS=1,1,1,1\n") != NULL &&
             strstr(r->stdout_buf, "DRIVE_ENTRY=1\n") != NULL;
#endif
    } else {
        rc = strstr(r->stdout_buf, "LC_ALL=C.utf8\nLC_MESSAGES=parent-message\nLANG=parent-language\n") != NULL;
#ifdef _WIN32
        rc = rc && strstr(r->stdout_buf, "VSLANG=1049\n") != NULL;
#endif
    }
    if (!strstr(r->stdout_buf, "SYMBOLS_E_SENTINEL=kept value\n") ||
        !strstr(r->stdout_buf, value("PATH"))) rc = 0;
    free(r);
    return rc;
}

typedef struct { int compiler; int okay; } WORK;
#ifdef _WIN32
static DWORD WINAPI worker(LPVOID arg)
#else
static void *worker(void *arg)
#endif
{
    WORK *w = arg;
    int i;
    w->okay = 1;
    for (i = 0; i < 3; ++i) if (!run_mode(w->compiler)) w->okay = 0;
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

static int environment_test(const char *cwd, const char *cmd)
{
    SHELL_EXEC_RESULT *r = malloc(sizeof(*r));
    WORK work[2] = {{0, 0}, {1, 0}};
    size_t before_size = 0, after_size = 0;
    char *before = environment_snapshot(&before_size), *after;
    int i;
#ifdef _WIN32
    HANDLE threads[2];
#else
    pthread_t threads[2];
    int created[2] = {0,0};
#endif
    if (!r || !before) { free(r); free(before); return 8; }
    cwd_arg = cwd; command_arg = cmd;
    CHECK(run_mode(0)); CHECK(run_mode(1));
    for (i = 0; i < 2; ++i) {
#ifdef _WIN32
        threads[i] = CreateThread(NULL, 0, worker, &work[i], 0, NULL);
        CHECK(threads[i] != NULL);
#else
        created[i] = pthread_create(&threads[i], NULL, worker, &work[i]) == 0;
        CHECK(created[i]);
#endif
    }
    for (i = 0; i < 2; ++i) {
#ifdef _WIN32
        if (threads[i]) { CHECK(WaitForSingleObject(threads[i], INFINITE) == WAIT_OBJECT_0); CloseHandle(threads[i]); }
#else
        if (created[i]) CHECK(pthread_join(threads[i], NULL) == 0);
#endif
        CHECK(work[i].okay);
    }
    CHECK(AgentShellExecCompiler("exit 17", cwd, 2000, r) == 1);
    CHECK(r->exit_code == 17 && !r->execution_failed);
    CHECK(AgentShellExecCompiler(cmd, "__symbols_E_missing_directory__", 2000, r) == 0);
    CHECK(r->execution_failed && r->exit_code == -1);
    CHECK(AgentShellExecCompiler(
#ifdef _WIN32
        "test_compiler_environment.exe --sleep-child",
#else
        "./test_compiler_environment --sleep-child",
#endif
        cwd, 20, r) == 1);
    CHECK(r->timed_out);
    CHECK(AgentShellExecCompilerGuarded("rm -rf __symbols_E_never_delete__", cwd, 2000, r) == 1);
    CHECK(r->execution_failed && r->exit_code == 126);
    CHECK(AgentShellExecCompilerGuarded("echo policy_kept", cwd, 2000, r) == 1);
    CHECK(!r->execution_failed && r->exit_code == 0 && strstr(r->stdout_buf, "policy_kept"));
    AgentShellCompilerEnvTestFailAfter(0);
    CHECK(AgentShellExecCompiler("echo E_NO_LAUNCH", cwd, 2000, r) == 0);
    CHECK(r->execution_failed && r->exit_code == -1 && r->stdout_len == 0);
#ifdef _WIN32
    for (i = 1; i < 4; ++i) {
        AgentShellCompilerEnvTestFailAfter(i);
        CHECK(AgentShellExecCompiler("echo E_NO_LAUNCH", cwd, 2000, r) == 0);
        CHECK(r->execution_failed && r->exit_code == -1 && r->stdout_len == 0);
    }
#endif
    AgentShellCompilerEnvTestFailAfter(-1);
    CHECK(run_mode(0)); CHECK(run_mode(1));
    CHECK(!strcmp(value("LC_ALL"), "C.utf8"));
    CHECK(!strcmp(value("LC_MESSAGES"), "parent-message"));
    CHECK(!strcmp(value("LANG"), "parent-language"));
#ifdef _WIN32
    CHECK(!strcmp(value("VSLANG"), "1049"));
#endif
    CHECK(!strcmp(value("SYMBOLS_E_SENTINEL"), "kept value"));
    after = environment_snapshot(&after_size);
    CHECK(after && before_size == after_size && !memcmp(before, after, before_size));
    free(before); free(after); free(r);
    printf("environment_failures=%d\n", failures);
    return failures ? 1 : 0;
}

/* Exercise the actual Runner routing, not just the shell API. */
static int runner_route(const char *cwd, const char *name)
{
    const char *before = "int ERoute(void) { return 0; }\n";
    const char *after = "int ERoute(void) { return 1; }\n";
    SWE_BENCH_TASK task;
    SWE_BENCH_RESULT *result;
    AGENT_RUNNER *runner;
    FILE *file;
#ifdef _WIN32
    if (!SetCurrentDirectoryA(cwd)) return 11;
#else
    if (chdir(cwd)) return 11;
#endif
    file = fopen("E-route.c", "wb");
    if (!file) return 12;
    fputs(before, file); fclose(file);
    memset(&task, 0, sizeof(task));
    strcpy(task.task_id, "E-ROUTE"); strcpy(task.target_file, "E-route.c");
    task.target_line = 1;
    strcpy(task.buggy_snippet, before); strcpy(task.fixed_snippet, after);
    snprintf(task.build_command, sizeof(task.build_command), "%s --assert-compiler-child", name);
    snprintf(task.test_command, sizeof(task.test_command), "%s --assert-normal-child", name);
    runner = AgentRunnerCreate(cwd, 0);
    result = malloc(sizeof(*result));
    if (!runner || !result) { remove("E-route.c"); free(result); AgentRunnerDestroy(runner); return 13; }
    CHECK(AgentRunnerSolveTask(runner, &task, result) == 1);
    CHECK(result->is_solved);
    AgentRunnerDestroy(runner); free(result); remove("E-route.c");
    return failures ? 1 : 0;
}

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "--env-child")) return child();
    if (argc > 1 && !strcmp(argv[1], "--assert-compiler-child"))
        return strcmp(value("LC_ALL"), "C") || strcmp(value("LC_MESSAGES"), "C") || strcmp(value("LANG"), "C")
#ifdef _WIN32
            || strcmp(value("VSLANG"), "1033")
#endif
            ? 41 : 0;
    if (argc > 1 && !strcmp(argv[1], "--assert-normal-child"))
        return strcmp(value("LC_ALL"), "C.utf8") || strcmp(value("LC_MESSAGES"), "parent-message") || strcmp(value("LANG"), "parent-language") ? 42 : 0;
    if (argc == 4 && !strcmp(argv[1], "--runner-route")) return runner_route(argv[2], argv[3]);
    if (argc > 1 && !strcmp(argv[1], "--sleep-child")) {
#ifdef _WIN32
        Sleep(4000);
#else
        struct timespec t = {4,0}; nanosleep(&t, NULL);
#endif
        return 0;
    }
    if (argc == 4 && !strcmp(argv[1], "--vslang-proof")) {
        SHELL_EXEC_RESULT *r = malloc(sizeof(*r));
        int rc;
        if (!r) return 8;
        rc = AgentShellExecCompiler(argv[3], argv[2], 5000, r);
        if (rc != 1 || r->execution_failed || r->timed_out || r->exit_code != 0 ||
            r->stdout_truncated || r->stderr_truncated) { free(r); return 8; }
        if (!strstr(r->stdout_buf, "VSLANG=1033\n")) {
            if (strstr(r->stdout_buf, "VSLANG=1049\n") && !strcmp(value("VSLANG"), "1049")) {
                puts("E_VSLANG_MUTANT actual_child_inherited1049_parent1049=1");
                free(r); return 42;
            }
            free(r); return 9;
        }
        puts("E_VSLANG_CONTROL actual_child1033_parent1049=1");
        free(r); return 0;
    }
    if (argc == 4 && !strcmp(argv[1], "--environment")) return environment_test(argv[2], argv[3]);
    if (argc == 2 && !strcmp(argv[1], "--parse")) {
        char text[4096], remedy[1024];
        size_t n = fread(text, 1, sizeof(text)-1, stdin);
        DIAGNOSTIC_REPORT report;
        int rc;
        text[n] = '\0';
        rc = DiagnosticParseOutput(text, &report);
        DiagnosticAbduceRemedy(&report, NULL, remedy, sizeof(remedy));
        printf("%d %u %u %u %d\n", rc, report.item_count, report.error_count, report.warning_count, (int)report.root_type);
        {
            uint32_t i;
            for (i = 0; i < report.item_count; ++i)
                printf("%d %u %u %u %u %u\n%s\n%s\n%s\n", (int)report.items[i].type,
                       report.items[i].expected_arity, report.items[i].actual_arity,
                       report.items[i].line, report.items[i].col, report.items[i].is_warning,
                       report.items[i].file, report.items[i].offending_symbol, report.items[i].raw_message);
        }
        puts(remedy);
        return 0;
    }
    if (argc == 2 && !strcmp(argv[1], "--inconsistent")) {
        DIAGNOSTIC_REPORT r;
        char remedy[1024];
        memset(&r, 0, sizeof(r)); r.root_type = DIAG_ERR_ARITY_MISMATCH; r.error_count = 1;
        CHECK(DiagnosticAbduceRemedy(&r, NULL, remedy, sizeof(remedy)) == 1);
        CHECK(!strstr(remedy, "provided") && !strstr(remedy, "expected 0"));
        return failures ? 1 : 0;
    }
    if (argc == 4 && !strcmp(argv[1], "--compiler")) {
        SHELL_EXEC_RESULT *r = malloc(sizeof(*r));
        int rc;
        if (!r) return 8;
        rc = AgentShellExecCompiler(argv[3], argv[2], 30000, r);
        printf("EXEC %d %d %d %d\n", rc, r->exit_code, r->execution_failed, r->timed_out);
        fwrite(r->stdout_buf, 1, r->stdout_len, stdout);
        fwrite(r->stderr_buf, 1, r->stderr_len, stdout);
        free(r); return 0;
    }
    return 9;
}
