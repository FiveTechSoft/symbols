#!/usr/bin/env bash
# ============================================================
# test_episodic_memory_e2e.sh: real process-restart memory proof
#
# Drives a real symbols-server over HTTP through the full memory
# lifecycle: learn -> query -> SERVER PROCESS RESTART -> query ->
# selective forget -> confirmed absence -> persistence-write failure
# -> recovery. The repo store data/memory/episodic.tsv is snapshotted
# and restored (bytes and permissions) no matter how the test ends.
# Exit 77 skips when the server binary or curl is missing.
# ============================================================
set -u
PORT="${1:-8217}"
ROOT=$(cd "$(dirname "$0")/.." && pwd)
EXE="$ROOT/build-gcc/symbols-server"
[ -x "$EXE" ] || EXE="$ROOT/build/symbols-server"
if [ ! -x "$EXE" ]; then
    echo "SKIP: symbols-server not built"
    exit 77
fi
command -v curl >/dev/null || { echo "SKIP: curl missing"; exit 77; }

STORE="$ROOT/data/memory/episodic.tsv"
MEMDIR="$ROOT/data/memory"
TMPD=$(mktemp -d)
SRV=""
fail=0

cp "$STORE" "$TMPD/store.orig" 2>/dev/null || touch "$TMPD/store.orig"
stat -c '%a' "$MEMDIR" > "$TMPD/memdir.mode" 2>/dev/null || echo 0755 > "$TMPD/memdir.mode"

cleanup() {
    [ -n "$SRV" ] && kill "$SRV" 2>/dev/null
    chmod "$(cat "$TMPD/memdir.mode")" "$MEMDIR" 2>/dev/null
    cp "$TMPD/store.orig" "$STORE" 2>/dev/null
    rm -f "$STORE.tmp" 2>/dev/null
    rm -rf "$TMPD"
}
trap cleanup EXIT

start_server() {
    (cd "$ROOT" && exec "$EXE" "$PORT" "$ROOT" "data/c_lang/c_corpus.txt" >"$TMPD/server$1.log" 2>&1) &
    SRV=$!
    local ready=0
    for _ in $(seq 1 60); do
        if curl -sf -o /dev/null "http://127.0.0.1:$PORT/v1/models"; then ready=1; break; fi
        sleep 0.25
    done
    [ "$ready" = 1 ] || { echo "FAIL: server did not start"; cat "$TMPD/server$1.log"; exit 1; }
}

stop_server() {
    [ -n "$SRV" ] && kill "$SRV" 2>/dev/null
    wait "$SRV" 2>/dev/null
    SRV=""
}

post() { # label, query
    printf '{"model":"symbols","messages":[{"role":"user","content":"%s"}]}' "$2" > "$TMPD/$1.req"
    curl -sf -X POST "http://127.0.0.1:$PORT/v1/chat/completions" \
        -H 'Content-Type: application/json' --data-binary "@$TMPD/$1.req" > "$TMPD/$1.out" || fail=1
}

assert_contains() {
    if ! grep -qF "$2" "$TMPD/$1.out"; then
        echo "FAIL: $3 (missing '$2')"; sed 's/data: //g' "$TMPD/$1.out" | head -5; fail=1
    else
        echo "ok: $3"
    fi
}
assert_not_contains() {
    if grep -qF "$2" "$TMPD/$1.out"; then
        echo "FAIL: $3 (unexpected '$2')"; fail=1
    else
        echo "ok: $3"
    fi
}

start_server 1

# --- learn + query in the first process ---
post learn "/learn Rosalind hermano_de Franklin"
assert_contains learn "Hecho aprendido y guardado persistentemente" "learn reports persisted success"
grep -qi $'rosalind\thermano_de\tfranklin' "$STORE" || { echo "FAIL: fact not in on-disk store"; fail=1; }
post q1 "es rosalind hermano de franklin?"
assert_contains q1 "Si," "learned fact answers Si"

# --- real process restart ---
stop_server
start_server 2
post q2 "es rosalind hermano de franklin?"
assert_contains q2 "Si," "fact survives the server process restart"

# --- selective forget + confirmed absence ---
post forget "/forget Rosalind hermano_de Franklin"
assert_contains forget "Hecho olvidado" "selective forget reports success"
post q3 "es rosalind hermano de franklin?"
assert_not_contains q3 "Si," "forgotten fact no longer answers Si"
if grep -qE "No tengo constancia|I don't know" "$TMPD/q3.out"; then
    echo "ok: forgotten fact abstains honestly"
else
    echo "FAIL: forgotten fact abstains honestly (no honest abstain in response)"
    fail=1
fi
if grep -qi $'rosalind\thermano_de\tfranklin' "$STORE"; then
    echo "FAIL: forgotten fact still in on-disk store"; fail=1
else
    echo "ok: forgotten fact removed from disk"
fi
post reforget "/forget Rosalind hermano_de Franklin"
assert_contains reforget "no estaba en la memoria" "re-forget reports not-present"

# --- forget survives another restart ---
stop_server
start_server 3
post q4 "es rosalind hermano de franklin?"
assert_not_contains q4 "Si," "forget persists across the restart"

# --- persistence-write failure must never report success ---
if [ "$(id -u)" -eq 0 ]; then
    echo "skip: running as root; chmod-based write failure not enforceable"
else
STORE_BEFORE=$(md5sum "$STORE" | cut -d' ' -f1)
chmod 0555 "$MEMDIR"
post faillearn "/learn Barbara hermano_de McClintock"
assert_contains faillearn "No se pudo" "failed write is reported, never claimed"
assert_not_contains faillearn "Hecho aprendido" "failed write never claims success"
post q5 "es barbara hermano de mcclintock?"
assert_not_contains q5 "Si," "failed learn never answers Si"
post failforget "/forget Rosalind hermano_de Franklin"
assert_contains failforget "no estaba en la memoria" "forget of unknown fact still honest under read-only dir"
chmod 0755 "$MEMDIR"
STORE_AFTER=$(md5sum "$STORE" | cut -d' ' -f1)
[ "$STORE_BEFORE" = "$STORE_AFTER" ] || { echo "FAIL: store changed under read-only dir"; fail=1; }
[ ! -e "$STORE.tmp" ] || { echo "FAIL: temporary file left behind"; fail=1; }
echo "ok: failed writes left the store byte-identical (atomic)"
fi

# --- recovery + failed forget keeps the fact intact ---
post learn2 "/learn Barbara hermano_de McClintock"
assert_contains learn2 "Hecho aprendido y guardado persistentemente" "store recovers once writable"
if [ "$(id -u)" -eq 0 ]; then
    echo "skip: running as root; chmod-based failed-forget not enforceable"
else
STORE_BEFORE=$(md5sum "$STORE" | cut -d' ' -f1)
chmod 0555 "$MEMDIR"
post roforget "/forget Barbara hermano_de McClintock"
assert_contains roforget "sigue intacto" "failed forget reported, fact kept"
chmod 0755 "$MEMDIR"
STORE_AFTER=$(md5sum "$STORE" | cut -d' ' -f1)
[ "$STORE_BEFORE" = "$STORE_AFTER" ] || { echo "FAIL: store changed by failed forget"; fail=1; }
post q6 "es barbara hermano de mcclintock?"
assert_contains q6 "Si," "fact kept after failed forget still answers Si"
fi

# --- full clear ---
post clear "/forget"
assert_contains clear "borrada" "bare forget clears memory"
post q7 "es barbara hermano de mcclintock?"
assert_not_contains q7 "Si," "cleared fact stops answering"

stop_server

if [ "$fail" != 0 ]; then
    echo "test_episodic_memory_e2e: FAILED"
    exit 1
fi
echo "test_episodic_memory_e2e: all checks passed"
