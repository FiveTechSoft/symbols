#!/usr/bin/env bash
# ============================================================
# test_git_inquiry_e2e.sh: native Git inquiry route over the wire
#
# Drives ONE symbols-server process with OpenCode-shaped envelopes
# whose env blocks point at real, disposable Git fixtures, and asserts
# the conversational answers: branch + HEAD, dirty vs ignored paths,
# and plain abstention on detached/conflicted/dirty/stale states.
# Literal "git status" must stay on the shell tool route. Fixture repos
# configure a repo-local identity (run #21 lesson: never rely on
# ambient Git config). Exit 77 skips when the server, curl or git is
# missing.
# ============================================================
set -u
PORT="${1:-8211}"
ROOT=$(cd "$(dirname "$0")/.." && pwd)
EXE="$ROOT/build-gcc/symbols-server"
[ -x "$EXE" ] || EXE="$ROOT/build/symbols-server"
if [ ! -x "$EXE" ]; then
    echo "SKIP: symbols-server not built"
    exit 77
fi
command -v curl >/dev/null || { echo "SKIP: curl missing"; exit 77; }
command -v git  >/dev/null || { echo "SKIP: git missing"; exit 77; }

TMPD=$(mktemp -d)
SRV=""
cleanup() { [ -n "$SRV" ] && kill "$SRV" 2>/dev/null; rm -rf "$TMPD"; }
trap cleanup EXIT

new_repo() { # path -- repo-local identity, never ambient config
    git init -q -b master "$1"
    git -C "$1" config user.name "symbols test"
    git -C "$1" config user.email "symbols-test@localhost"
}

# Clean repo with two ignored paths (build/ and *.log)
CLEAN="$TMPD/clean"
new_repo "$CLEAN"
printf 'build/\n*.log\n' >"$CLEAN/.gitignore"
printf 'int main(void){return 0;}\n' >"$CLEAN/main.c"
git -C "$CLEAN" add .gitignore main.c
git -C "$CLEAN" commit -qm initial
mkdir -p "$CLEAN/build"
printf 'obj\n' >"$CLEAN/build/main.o"
printf 'log\n' >"$CLEAN/debug.log"

# Dirty repo: 1 staged, 1 unstaged, 1 untracked
DIRTY="$TMPD/dirty"
new_repo "$DIRTY"
printf 'one\n' >"$DIRTY/tracked.txt"
git -C "$DIRTY" add tracked.txt
git -C "$DIRTY" commit -qm initial
printf 'staged\n' >"$DIRTY/staged.txt"
git -C "$DIRTY" add staged.txt
printf 'two\n' >>"$DIRTY/tracked.txt"
printf 'untracked\n' >"$DIRTY/loose.txt"

# Detached HEAD
DETACHED="$TMPD/detached"
new_repo "$DETACHED"
printf 'one\n' >"$DETACHED/f.txt"
git -C "$DETACHED" add f.txt
git -C "$DETACHED" commit -qm initial
git -C "$DETACHED" checkout -q --detach HEAD

# Merge conflict
CONFLICT="$TMPD/conflict"
new_repo "$CONFLICT"
printf 'base\n' >"$CONFLICT/f.txt"
git -C "$CONFLICT" add f.txt
git -C "$CONFLICT" commit -qm base
git -C "$CONFLICT" checkout -qb side
printf 'side\n' >"$CONFLICT/f.txt"
git -C "$CONFLICT" commit -qam side
git -C "$CONFLICT" checkout -q master
printf 'master\n' >"$CONFLICT/f.txt"
git -C "$CONFLICT" commit -qam master
git -C "$CONFLICT" merge --no-edit side >/dev/null 2>&1

# Not a repository
PLAIN="$TMPD/plain"
mkdir -p "$PLAIN"

(cd "$ROOT" && exec "$EXE" "$PORT" "$ROOT" "data/c_lang/c_corpus.txt" >"$TMPD/server.log" 2>&1) &
SRV=$!
ready=0
for _ in $(seq 1 60); do
    if curl -sf -o /dev/null "http://127.0.0.1:$PORT/v1/models"; then ready=1; break; fi
    sleep 0.25
done
[ "$ready" = 1 ] || { echo "FAIL: server did not start"; cat "$TMPD/server.log"; exit 1; }

fail=0
mkbody() { # dir query [prior_assistant] -- OpenCode-shaped envelope
    local sys="<env>\n  Working directory: $1\n  Workspace root folder: /\\n  Is directory a git repo: yes\n  Platform: linux\n  Today's date: Tue Sep 22 2026\n</env>"
    if [ $# -ge 3 ]; then
        printf '{"messages":[{"role":"system","content":"%s"},{"role":"user","content":"%s"},{"role":"assistant","content":"%s"},{"role":"user","content":"%s"}],"tools":[{"type":"function","function":{"name":"bash"}}]}' \
            "$sys" "$2" "$3" "$4" >"$TMPD/body.json"
    else
        printf '{"messages":[{"role":"system","content":"%s"},{"role":"user","content":"%s"}],"tools":[{"type":"function","function":{"name":"bash"}}]}' \
            "$sys" "$2" >"$TMPD/body.json"
    fi
}
post() { # label dir query [prior_assistant followup]
    if [ $# -ge 4 ]; then mkbody "$2" "$3" "$4" "$5"; else mkbody "$2" "$3"; fi
    curl -sf -X POST "http://127.0.0.1:$PORT/v1/chat/completions" \
        -H 'Content-Type: application/json' --data-binary "@$TMPD/body.json" \
        >"$TMPD/$1.out" || { echo "FAIL: $1 (request failed)"; fail=1; }
}
assert_contains() {
    if ! grep -qF "$2" "$TMPD/$1.out"; then
        echo "FAIL: $3 (missing '$2')"; cat "$TMPD/$1.out"; fail=1
    else
        echo "ok: $3"
    fi
}
assert_not_contains() {
    if grep -qF "$2" "$TMPD/$1.out"; then
        echo "FAIL: $3 (unexpected '$2')"; cat "$TMPD/$1.out"; fail=1
    else
        echo "ok: $3"
    fi
}

# Clean repo: branch + HEAD + clean tree + ignored paths, then ready
post clean_status "$CLEAN" "estado del repo, por favor"
assert_contains clean_status 'Rama `master`' 'clean repo answers branch'
assert_contains clean_status 'HEAD `' 'clean repo answers HEAD'
assert_contains clean_status 'Árbol limpio' 'clean repo reports clean tree'
assert_contains clean_status '2 ruta(s) ignorada(s) (no cuentan como cambio)' \
    'clean repo separates ignored paths from changes'
post clean_ready "$CLEAN" "estado del repo, por favor" "Entendido." "¿puedo aplicar cambios en el repo?"
assert_contains clean_ready 'Listo para trabajar' 'clean repo passes preflight'

# Stale HEAD: status first, external commit, then readiness must abstain
post stale_status "$CLEAN" "¿en qué rama estoy?" >/dev/null
printf 'more\n' >>"$CLEAN/main.c"
git -C "$CLEAN" commit -qam second
post stale_ready "$CLEAN" "¿en qué rama estoy?" "Entendido." "¿puedo aplicar cambios en el repo?"
assert_contains stale_ready 'Me abstengo: HEAD cambió' 'moved HEAD is a stale abstention'

# Dirty repo: counts and abstention
post dirty_status "$DIRTY" "hay cambios en el repo?"
assert_contains dirty_status 'Cambios: 1 staged, 1 unstaged, 1 untracked.' \
    'dirty repo reports exact counts'
post dirty_ready "$DIRTY" "¿está listo el repo para trabajar?"
assert_contains dirty_ready 'Me abstengo' 'dirty repo fails preflight'
assert_contains dirty_ready 'cambios sin confirmar' 'dirty abstention says why'

# Detached HEAD
post detached_status "$DETACHED" "estado del repo"
assert_contains detached_status 'HEAD desacoplado' 'detached HEAD is stated'
post detached_ready "$DETACHED" "¿puedo trabajar en el repo?"
assert_contains detached_ready 'Me abstengo: HEAD desacoplado' 'detached fails preflight'

# Merge conflict
post conflict_status "$CONFLICT" "estado del repo"
assert_contains conflict_status 'conflictos de merge sin resolver' 'conflicts are stated'
post conflict_ready "$CONFLICT" "¿puedo seguir con el repo?"
assert_contains conflict_ready 'Me abstengo' 'conflicted repo fails preflight'
assert_contains conflict_ready 'conflictos de merge' 'conflict abstention says why'

# Not a repository
post plain_status "$PLAIN" "estado del repo"
assert_contains plain_status 'no está dentro de un repositorio Git' \
    'non-repo is an honest answer, not a guess'

# Literal command boundary: "git status" stays on the shell tool route
post literal_status "$CLEAN" "git status"
assert_contains literal_status '"name":"bash"' 'literal git status emits a bash tool call'
assert_contains literal_status 'git status' 'literal git status relays the command'
assert_not_contains literal_status 'Rama `master`' 'literal git status is not rerouted'

if [ "$fail" != 0 ]; then
    echo "test_git_inquiry_e2e: FAILED"
    exit 1
fi
echo "test_git_inquiry_e2e: all checks passed"
