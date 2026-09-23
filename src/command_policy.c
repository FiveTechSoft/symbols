/* command_policy.c: see command_policy.h. */
#include "command_policy.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CP_MAX_ARGS 48
#define CP_ARG_LEN  256

typedef struct
{
    int  argc;
    char argv[CP_MAX_ARGS][CP_ARG_LEN];
    int  pipe_in;      /* this command reads from a pipe */
    int  redirect_abs; /* output redirected to an absolute path outside /tmp or /dev/null */
    int  redirect_any; /* output redirected to a file (not /dev/null) */
} SIMPLE;

static const char *const READ_PROGS[] = {
    "ls", "cat", "head", "tail", "grep", "rg", "find", "wc", "echo", "printf", "pwd", "stat", "file",
    "diff", "cmp", "sort", "uniq", "cut", "tr", "which", "true", "false", "test", "[", "date", "env",
    "uname", "whoami", "basename", "dirname", "realpath", "sha256sum", "md5sum", "less", "more", "tree", NULL};

static const char *const SHELLS[] = {"sh", "bash", "zsh", "dash", "ksh", "python", "python3", "perl", "ruby", "node", NULL};

static int in_list(const char *s, const char *const *l)
{
    for (; *l; l++)
        if (!strcmp(s, *l))
            return 1;
    return 0;
}

static void set_reason(char *r, size_t n, const char *fmt, const char *a)
{
    if (r && n && !r[0])
        snprintf(r, n, fmt, a);
}

static int has_arg(const SIMPLE *s, int from, const char *a)
{
    for (int i = from; i < s->argc; i++)
        if (!strcmp(s->argv[i], a))
            return 1;
    return 0;
}

/* short-option cluster contains letter c ("-fd" has f and d) */
static int has_short(const SIMPLE *s, int from, char c)
{
    for (int i = from; i < s->argc; i++) {
        const char *a = s->argv[i];
        if (a[0] == '-' && a[1] != '-' && a[1] && strchr(a + 1, c))
            return 1;
    }
    return 0;
}

static int has_prefix_arg(const SIMPLE *s, int from, const char *p)
{
    for (int i = from; i < s->argc; i++)
        if (!strncmp(s->argv[i], p, strlen(p)))
            return 1;
    return 0;
}

static POLICY_CLASS classify_git(const SIMPLE *s, int g, char *r, size_t rn)
{
    int i = g + 1;
    while (i < s->argc && s->argv[i][0] == '-') {       /* git -C dir, -c k=v */
        if ((!strcmp(s->argv[i], "-C") || !strcmp(s->argv[i], "-c")) && i + 1 < s->argc)
            i++;
        i++;
    }
    if (i >= s->argc)
        return POLICY_READ;
    const char *sub = s->argv[i];
    int a = i + 1;
    static const char *const RD[] = {"status", "diff", "log", "show", "blame", "rev-parse", "ls-files",
                                     "describe", "shortlog", "grep", "cat-file", "ls-tree", "merge-base",
                                     "rev-list", "fetch", "remote", "config", "version", "help", NULL};
    if (!strcmp(sub, "push")) {
        if (has_arg(s, a, "--force") || has_short(s, a, 'f') || has_prefix_arg(s, a, "--force-with-lease") ||
            has_arg(s, a, "--mirror") || has_arg(s, a, "--delete") || has_short(s, a, 'd') ||
            has_arg(s, a, "--prune")) {
            set_reason(r, rn, "git %s rewrites or deletes remote refs", sub);
            return POLICY_DESTRUCTIVE;
        }
        for (int k = a; k < s->argc; k++)
            if (s->argv[k][0] == '+' || s->argv[k][0] == ':') {
                set_reason(r, rn, "git %s with a force/delete refspec", sub);
                return POLICY_DESTRUCTIVE;
            }
        return POLICY_WRITE;
    }
    if (!strcmp(sub, "reset") && (has_arg(s, a, "--hard") || has_arg(s, a, "--merge") || has_arg(s, a, "--keep"))) {
        set_reason(r, rn, "git %s --hard discards work", sub);
        return POLICY_DESTRUCTIVE;
    }
    if (!strcmp(sub, "clean") && (has_short(s, a, 'f') || has_arg(s, a, "--force"))) {
        set_reason(r, rn, "git %s -f deletes untracked files", sub);
        return POLICY_DESTRUCTIVE;
    }
    if (!strcmp(sub, "checkout") && (has_arg(s, a, "--") || has_arg(s, a, ".") || has_short(s, a, 'f') ||
                                     has_arg(s, a, "--force"))) {
        set_reason(r, rn, "git %s overwrites working-tree changes", sub);
        return POLICY_DESTRUCTIVE;
    }
    if (!strcmp(sub, "restore") && !has_arg(s, a, "--staged") && !has_short(s, a, 'S')) {
        set_reason(r, rn, "git %s overwrites working-tree changes", sub);
        return POLICY_DESTRUCTIVE;
    }
    if (!strcmp(sub, "switch") && (has_arg(s, a, "--discard-changes") || has_short(s, a, 'f') || has_arg(s, a, "--force") ||
                                   has_short(s, a, 'C'))) {
        set_reason(r, rn, "git %s discards changes or resets a branch", sub);
        return POLICY_DESTRUCTIVE;
    }
    if (!strcmp(sub, "branch") && (has_short(s, a, 'D') || has_short(s, a, 'd') || has_arg(s, a, "--delete") ||
                                   has_short(s, a, 'f') || has_arg(s, a, "--force") || has_short(s, a, 'M'))) {
        set_reason(r, rn, "git %s deletes or force-moves a branch", sub);
        return POLICY_DESTRUCTIVE;
    }
    if (!strcmp(sub, "tag") && (has_short(s, a, 'd') || has_arg(s, a, "--delete") || has_short(s, a, 'f'))) {
        set_reason(r, rn, "git %s deletes or moves a tag", sub);
        return POLICY_DESTRUCTIVE;
    }
    if (!strcmp(sub, "stash") && a < s->argc && (!strcmp(s->argv[a], "drop") || !strcmp(s->argv[a], "clear"))) {
        set_reason(r, rn, "git %s drop/clear loses stashed work", sub);
        return POLICY_DESTRUCTIVE;
    }
    if (!strcmp(sub, "commit") && has_arg(s, a, "--amend")) {
        set_reason(r, rn, "git %s --amend rewrites history", sub);
        return POLICY_DESTRUCTIVE;
    }
    if (!strcmp(sub, "rebase") || !strcmp(sub, "filter-branch") || !strcmp(sub, "filter-repo") ||
        !strcmp(sub, "replace") || !strcmp(sub, "prune")) {
        set_reason(r, rn, "git %s rewrites history or drops objects", sub);
        return POLICY_DESTRUCTIVE;
    }
    if (!strcmp(sub, "reflog") && a < s->argc && (!strcmp(s->argv[a], "expire") || !strcmp(s->argv[a], "delete"))) {
        set_reason(r, rn, "git %s expire/delete drops recovery points", sub);
        return POLICY_DESTRUCTIVE;
    }
    if (!strcmp(sub, "gc") && has_prefix_arg(s, a, "--prune")) {
        set_reason(r, rn, "git %s --prune drops unreachable objects", sub);
        return POLICY_DESTRUCTIVE;
    }
    if (!strcmp(sub, "update-ref") && has_short(s, a, 'd')) {
        set_reason(r, rn, "git %s -d deletes a ref", sub);
        return POLICY_DESTRUCTIVE;
    }
    if (!strcmp(sub, "worktree") && a < s->argc && !strcmp(s->argv[a], "remove") &&
        (has_short(s, a, 'f') || has_arg(s, a, "--force"))) {
        set_reason(r, rn, "git %s remove --force discards a worktree", sub);
        return POLICY_DESTRUCTIVE;
    }
    if (!strcmp(sub, "remote") && a < s->argc && (!strcmp(s->argv[a], "remove") || !strcmp(s->argv[a], "rm") ||
                                                  !strcmp(s->argv[a], "set-url")))
        return POLICY_WRITE;
    if (!strcmp(sub, "config") && (has_arg(s, a, "--global") || has_arg(s, a, "--system"))) {
        set_reason(r, rn, "git %s --global/--system reaches outside the workspace", sub);
        return POLICY_DESTRUCTIVE;
    }
    if (!strcmp(sub, "config") && a + 1 < s->argc && s->argv[a][0] != '-')
        return POLICY_WRITE;   /* git config key value */
    if (!strcmp(sub, "branch") || !strcmp(sub, "tag") || !strcmp(sub, "stash"))
        return ((a >= s->argc && strcmp(sub, "stash") != 0) || (a < s->argc && !strcmp(s->argv[a], "list")) || has_arg(s, a, "--list") ||
                has_short(s, a, 'a') || has_short(s, a, 'v') || has_short(s, a, 'l') ||
                (!strcmp(sub, "stash") && a < s->argc && !strcmp(s->argv[a], "show")))
                   ? POLICY_READ
                   : POLICY_WRITE;
    if (in_list(sub, RD))
        return POLICY_READ;
    return POLICY_WRITE;   /* add, commit, merge, switch, cherry-pick, revert, init, clone, pull, ... */
}

static POLICY_CLASS classify_simple(const SIMPLE *s, char *r, size_t rn)
{
    int p = 0;
    /* skip leading assignments and harmless wrappers */
    while (p < s->argc) {
        const char *a = s->argv[p];
        const char *eq = strchr(a, '=');
        if (eq && eq != a && a[0] != '-') {
            int ok = 1;
            for (const char *c = a; c < eq; c++)
                if (!isalnum((unsigned char)*c) && *c != '_')
                    ok = 0;
            if (ok) { p++; continue; }
        }
        if (!strcmp(a, "command") || !strcmp(a, "nohup") || !strcmp(a, "time") || !strcmp(a, "exec") ||
            !strcmp(a, "nice")) { p++; continue; }
        if (!strcmp(a, "env")) {
            p++;
            while (p < s->argc && s->argv[p][0] == '-') p++;
            continue;
        }
        break;
    }
    if (p >= s->argc)
        return POLICY_READ;
    const char *prog = s->argv[p];
    const char *base = strrchr(prog, '/');
    base = base ? base + 1 : prog;
    if (!strcmp(base, "sudo") || !strcmp(base, "su") || !strcmp(base, "doas") || !strcmp(base, "runas")) {
        set_reason(r, rn, "%s escalates privileges", base);
        return POLICY_DESTRUCTIVE;
    }
    if (s->pipe_in && in_list(base, SHELLS)) {
        set_reason(r, rn, "piping into %s runs unreviewed code", base);
        return POLICY_DESTRUCTIVE;
    }
    if (!strcmp(base, "eval")) {
        set_reason(r, rn, "%s runs a constructed command", base);
        return POLICY_UNPARSEABLE;
    }
    if (!strcmp(base, "git"))
        return classify_git(s, p, r, rn);
    if (!strcmp(base, "rm") || !strcmp(base, "rmdir")) {
        if (!strcmp(base, "rm") && (has_short(s, p + 1, 'r') || has_short(s, p + 1, 'R') || has_arg(s, p + 1, "--recursive"))) {
            set_reason(r, rn, "%s -r deletes a tree", base);
            return POLICY_DESTRUCTIVE;
        }
        for (int i = p + 1; i < s->argc; i++) {
            const char *a = s->argv[i];
            if (a[0] == '/' || a[0] == '~' || !strncmp(a, "..", 2) || strchr(a, '*')) {
                set_reason(r, rn, "%s outside the workspace or with a glob", base);
                return POLICY_DESTRUCTIVE;
            }
        }
        return POLICY_WRITE;
    }
    static const char *const DANGER[] = {"dd", "mkfs", "shred", "fdisk", "parted", "wipefs", "shutdown", "reboot",
                                         "halt", "poweroff", "kill", "killall", "pkill", "crontab", "systemctl",
                                         "launchctl", "format", "diskpart", "del", "rd", "chattr", NULL};
    if (in_list(base, DANGER) || !strncmp(base, "mkfs.", 5)) {
        set_reason(r, rn, "%s can destroy data or system state", base);
        return POLICY_DESTRUCTIVE;
    }
    if ((!strcmp(base, "chmod") || !strcmp(base, "chown") || !strcmp(base, "chgrp")) &&
        (has_short(s, p + 1, 'R') || has_arg(s, p + 1, "--recursive"))) {
        set_reason(r, rn, "%s -R changes a whole tree", base);
        return POLICY_DESTRUCTIVE;
    }
    if (!strcmp(base, "find") && (has_arg(s, p + 1, "-delete") || has_arg(s, p + 1, "-exec") || has_arg(s, p + 1, "-execdir"))) {
        set_reason(r, rn, "%s -delete/-exec changes files", base);
        return POLICY_DESTRUCTIVE;
    }
    if ((!strcmp(base, "mv") || !strcmp(base, "cp") || !strcmp(base, "ln") || !strcmp(base, "truncate")) ) {
        for (int i = p + 1; i < s->argc; i++) {
            const char *a = s->argv[i];
            if ((a[0] == '/' && strncmp(a, "/tmp/", 5) != 0) || a[0] == '~' || !strncmp(a, "..", 2)) {
                set_reason(r, rn, "%s writes outside the workspace", base);
                return POLICY_DESTRUCTIVE;
            }
        }
        if (!strcmp(base, "truncate"))
            return POLICY_DESTRUCTIVE;
        return POLICY_WRITE;
    }
    if (s->redirect_abs) {
        set_reason(r, rn, "%s redirects output outside the workspace", base);
        return POLICY_DESTRUCTIVE;
    }
    if (!strcmp(base, "sed") && (has_short(s, p + 1, 'i') || has_prefix_arg(s, p + 1, "--in-place")))
        return POLICY_WRITE;
    if (in_list(base, READ_PROGS))
        return POLICY_READ;
    return POLICY_WRITE;
}

/* Split and tokenize; returns worst class. */
POLICY_CLASS CommandPolicyClassify(const char *cmd, char *reason, size_t rn)
{
    if (reason && rn)
        reason[0] = '\0';
    if (!cmd)
        return POLICY_UNPARSEABLE;
    POLICY_CLASS worst = POLICY_READ;
    SIMPLE *s = (SIMPLE *)calloc(1, sizeof(*s));
    if (!s)
        return POLICY_UNPARSEABLE;
    char tok[CP_ARG_LEN];
    size_t tl = 0;
    int in_tok = 0, sq = 0, dq = 0, redirect_next = 0, pipe_next = 0, depth = 0;
    const char *p = cmd;
    for (;; p++) {
        char c = *p;
        int end_cmd = 0, end_tok = 0;
        if (c == '\0') {
            if (sq || dq) {
                set_reason(reason, rn, "unbalanced %s", sq ? "single quote" : "double quote");
                free(s);
                return POLICY_UNPARSEABLE;
            }
            end_tok = end_cmd = 1;
        } else if (sq) {
            if (c == '\'') sq = 0;
            else if (tl + 1 < sizeof(tok)) tok[tl++] = c;
            in_tok = 1;
            continue;
        } else if (c == '\\' && p[1]) {
            p++;
            if (tl + 1 < sizeof(tok)) tok[tl++] = *p;
            in_tok = 1;
            continue;
        } else if (c == '`' || (c == '$' && p[1] == '(')) {
            /* command substitution: classify its body as its own command line */
            char close = c == '`' ? '`' : ')';
            const char *b = c == '`' ? p + 1 : p + 2;
            int d = 1;
            const char *e = b;
            if (close == ')' && e[0] == '(') {   /* $(( arithmetic )) */
                const char *z = strstr(e, "))");
                if (!z) { free(s); set_reason(reason, rn, "%s", "unbalanced $(("); return POLICY_UNPARSEABLE; }
                p = z + 1;
                in_tok = 1;
                continue;
            }
            for (; *e; e++) {
                if (close == ')' && *e == '(') d++;
                else if (*e == close && --d == 0) break;
            }
            if (!*e) { free(s); set_reason(reason, rn, "%s", "unbalanced command substitution"); return POLICY_UNPARSEABLE; }
            size_t bl = (size_t)(e - b);
            char *inner = (char *)malloc(bl + 1);
            if (!inner) { free(s); return POLICY_UNPARSEABLE; }
            memcpy(inner, b, bl);
            inner[bl] = '\0';
            char r2[160] = "";
            POLICY_CLASS ic = depth > 8 ? POLICY_UNPARSEABLE : CommandPolicyClassify(inner, r2, sizeof(r2));
            free(inner);
            if (ic > worst) { worst = ic; if (reason && rn) snprintf(reason, rn, "%s", r2); }
            p = e;
            in_tok = 1;
            continue;
        } else if (dq) {
            if (c == '"') dq = 0;
            else if (tl + 1 < sizeof(tok)) tok[tl++] = c;
            in_tok = 1;
            continue;
        } else if (c == '\'') { sq = 1; in_tok = 1; continue; }
        else if (c == '"') { dq = 1; in_tok = 1; continue; }
        else if (c == ';' || c == '\n' || c == '&' || c == '|' || c == '(' || c == ')' || c == '{' || c == '}') {
            end_tok = end_cmd = 1;
            if (c == '|' && p[1] != '|')
                pipe_next = 1;
            if ((c == '|' || c == '&') && p[1] == c)
                p++;
        } else if (c == '>' || c == '<') {
            end_tok = 1;
            if (c == '>') redirect_next = 1;
            while (p[1] == '>' || p[1] == '&' || p[1] == '|') p++;
        } else if (isspace((unsigned char)c)) {
            end_tok = 1;
        } else {
            if (tl + 1 < sizeof(tok)) tok[tl++] = c;
            in_tok = 1;
            continue;
        }
        if (end_tok && in_tok) {
            tok[tl] = '\0';
            if (redirect_next == 2) {
                if ((tok[0] == '/' && strcmp(tok, "/dev/null") != 0 && strncmp(tok, "/tmp/", 5) != 0 &&
                     strncmp(tok, "/dev/std", 8) != 0) || tok[0] == '~' || !strncmp(tok, "..", 2))
                    s->redirect_abs = 1;
                if (strcmp(tok, "/dev/null") != 0 && strncmp(tok, "/dev/std", 8) != 0)
                    s->redirect_any = 1;
                redirect_next = 0;
            } else if (s->argc < CP_MAX_ARGS) {
                snprintf(s->argv[s->argc++], CP_ARG_LEN, "%s", tok);
            }
            tl = 0;
            in_tok = 0;
        }
        if (redirect_next == 1 && (c == '>' ))
            redirect_next = 2;
        if (end_cmd) {
            if (redirect_next == 2) { /* redirect with no target */ }
            if (s->argc > 0 || s->redirect_abs) {
                POLICY_CLASS k = classify_simple(s, reason && worst < POLICY_DESTRUCTIVE ? reason : NULL, rn);
                if (k == POLICY_READ && s->redirect_any)
                    k = POLICY_WRITE;
                if (k > worst) worst = k;
            }
            int pn = pipe_next;
            memset(s, 0, sizeof(*s));
            s->pipe_in = pn;
            pipe_next = 0;
            redirect_next = 0;
            if (c == '\0')
                break;
        }
    }
    (void)depth;
    free(s);
    return worst;
}

const char *CommandPolicyName(POLICY_CLASS c)
{
    switch (c) {
    case POLICY_READ: return "read";
    case POLICY_WRITE: return "write";
    case POLICY_DESTRUCTIVE: return "destructive";
    default: return "unparseable";
    }
}

int CommandPolicyAllowed(POLICY_CLASS c)
{
    return c == POLICY_READ || c == POLICY_WRITE;
}
