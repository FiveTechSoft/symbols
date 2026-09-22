#!/usr/bin/env bash
# ============================================================
# test_session_isolation.sh: cross-session isolation regression
#
# Replays two independent, real OpenCode 1.18.32 sessions against ONE
# symbols-server process. Fixtures under tests/fixtures/opencode_sessions
# carry the byte-exact message arrays, declared tool names, and env
# blocks captured from two real OpenCode 1.18.32 TUI sessions on
# 2026-09-22; client boilerplate (prompt text, tool schemas) is elided
# because the server never parses it. Session A runs the full create/append/append
# sequence first; session B must afterwards start cleanly: its "crea
# test.txt" must be answered as a creation, never with session A's
# append state. Exit 77 skips when the server binary is missing.
# ============================================================
set -u
PORT="${1:-8209}"
ROOT=$(cd "$(dirname "$0")/.." && pwd)
EXE="$ROOT/build-gcc/symbols-server"
[ -x "$EXE" ] || EXE="$ROOT/build/symbols-server"
if [ ! -x "$EXE" ]; then
    echo "SKIP: symbols-server not built"
    exit 77
fi
command -v curl >/dev/null || { echo "SKIP: curl missing"; exit 77; }

TMPD=$(mktemp -d)
SRV=""
cleanup() { [ -n "$SRV" ] && kill "$SRV" 2>/dev/null; rm -rf "$TMPD"; }
trap cleanup EXIT

(cd "$ROOT" && exec "$EXE" "$PORT" "$ROOT" "data/c_lang/c_corpus.txt" >"$TMPD/server.log" 2>&1) &
SRV=$!
ready=0
for _ in $(seq 1 60); do
    if curl -sf -o /dev/null "http://127.0.0.1:$PORT/v1/models"; then ready=1; break; fi
    sleep 0.25
done
[ "$ready" = 1 ] || { echo "FAIL: server did not start"; cat "$TMPD/server.log"; exit 1; }

F="$ROOT/tests/fixtures/opencode_sessions"
fail=0
post() { curl -sf -X POST "http://127.0.0.1:$PORT/v1/chat/completions" \
    -H 'Content-Type: application/json' --data-binary "@$F/$1" || fail=1; }

assert_contains() { # file, needle, label
    if ! grep -qF "$2" "$TMPD/$1.out"; then
        echo "FAIL: $3 (missing '$2' in $1 response)"
        fail=1
    else
        echo "ok: $3"
    fi
}
assert_not_contains() {
    if grep -qF "$2" "$TMPD/$1.out"; then
        echo "FAIL: $3 (unexpected '$2' in $1 response)"
        fail=1
    else
        echo "ok: $3"
    fi
}

# Session A: full three-turn sequence against the fresh server
post a1_create_user.json  >"$TMPD/a1.out"
post a2_create_tool.json  >"$TMPD/a2.out"
post a3_append1_user.json >"$TMPD/a3.out"
post a4_append1_tool.json >"$TMPD/a4.out"
post a5_append2_user.json >"$TMPD/a5.out"
post a6_append2_tool.json >"$TMPD/a6.out"
assert_contains a2 'Creado' 'session A turn 1 creates test.txt'
assert_contains a4 'primera linea' 'session A turn 2 appends primera linea'
assert_contains a6 'segunda linea' 'session A turn 3 appends segunda linea'
post a7_swap_user.json    >"$TMPD/a7.out"
post a8_swap_tool.json    >"$TMPD/a8.out"
assert_contains a7 '"name":"edit"' 'authentic swap prompt dispatches edit instead of corpus QA'
assert_contains a7 'primera linea\\nsegunda linea' 'swap requires the exact known two-line sequence'
assert_contains a7 'segunda linea\\nprimera linea' 'swap reverses only the two lines'
assert_contains a8 'Intercambiadas las líneas' 'confirmed swap gets a bounded conversational result'
assert_contains a8 '@@ -1,2 +1,2 @@' 'confirmed swap gets a bounded two-line diff'
assert_not_contains a8 '"finish_reason":"tool_calls"' 'swap continuation never redispatches edit'
[ "$(grep -o 'Intercambiadas las líneas' "$TMPD/a8.out" | wc -l)" -eq 1 ] || { echo "FAIL: swap continuation must have exactly one final confirmation"; fail=1; }

# Session B: independent client, same server process, afterwards
post b1_create_user.json  >"$TMPD/b1.out"
post b2_create_tool.json  >"$TMPD/b2.out"
post b3_append1_user.json >"$TMPD/b3.out"
post b4_append1_tool.json >"$TMPD/b4.out"
post b5_append2_user.json >"$TMPD/b5.out"
post b6_append2_tool.json >"$TMPD/b6.out"
assert_contains b2 'Creado' 'session B turn 1 creates test.txt'
assert_not_contains b2 'primera linea' 'session B turn 1 sees no session A append state'
assert_not_contains b2 'segunda linea' 'session B turn 1 sees no session A final content'
assert_contains b4 'primera linea' 'session B turn 2 appends primera linea'
assert_contains b6 'segunda linea' 'session B turn 3 appends segunda linea'

if [ "$fail" != 0 ]; then
    echo "test_session_isolation: FAILED"
    exit 1
fi
echo "test_session_isolation: all checks passed"
