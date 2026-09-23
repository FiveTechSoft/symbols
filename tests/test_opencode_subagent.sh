#!/usr/bin/env bash
# OpenCode 1.18.32 child session (task tool): a request whose declared tools
# have neither `task` nor `todowrite` gets the subagent result block in its
# final text; a primary-shaped request does not.
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
EXE="$ROOT/build/symbols-server"; [ -x "$EXE" ] || EXE="$ROOT/build-gcc/symbols-server"
[ -x "$EXE" ] || { echo "SKIP: symbols-server not built"; exit 77; }
command -v curl >/dev/null || { echo "SKIP: curl missing"; exit 77; }
command -v python3 >/dev/null || { echo "SKIP: python3 missing"; exit 77; }
PORT=$((19500 + ($$ % 200))); TMPD=$(mktemp -d); SRV=""
cleanup() { [ -n "$SRV" ] && kill "$SRV" 2>/dev/null; rm -rf "$TMPD"; }; trap cleanup EXIT
(cd "$ROOT" && exec "$EXE" "$PORT" "$TMPD" "data/c_lang/c_corpus.txt" >"$TMPD/server.log" 2>&1) & SRV=$!
for _ in $(seq 1 60); do curl -sf -o /dev/null "http://127.0.0.1:$PORT/v1/models" && break; sleep .1; done
ask() { # tools stream question
  python3 - "$1" "$2" "$3" >"$TMPD/req.json" <<'PY'
import json, sys
tools = [{"type": "function", "function": {"name": n, "parameters": {"type": "object", "properties": {}}}} for n in sys.argv[1].split(",")]
print(json.dumps({"model": "symbols", "stream": sys.argv[2] == "1", "tools": tools,
                  "messages": [{"role": "system", "content": "<env>\n  Working directory: /tmp/ocfx\n  Platform: linux\n</env>"},
                               {"role": "user", "content": sys.argv[3]}]}))
PY
  curl -s -X POST "http://127.0.0.1:$PORT/v1/chat/completions" -H 'Content-Type: application/json' --data-binary @"$TMPD/req.json"
}
out=$(ask "bash,glob,read,grep,list" 0 "Who was the father of David?")
echo "$out" | grep -q 'Subagent result (Symbols):' || { echo "FAIL: child reply lacks result block"; echo "$out"; exit 1; }
echo "$out" | grep -q 'read-only session' || { echo "FAIL: read-only child not marked"; echo "$out"; exit 1; }
out=$(ask "bash,glob,read,edit,write" 1 "Who was the father of David?")
echo "$out" | grep -q 'Subagent result (Symbols):' || { echo "FAIL: streamed child reply lacks result block"; echo "$out"; exit 1; }
! echo "$out" | grep -q 'read-only session' || { echo "FAIL: writable child marked read-only"; exit 1; }
out=$(ask "bash,glob,read,edit,todowrite,task" 0 "Who was the father of David?")
! echo "$out" | grep -q 'Subagent result' || { echo "FAIL: primary reply got the child block"; echo "$out"; exit 1; }
echo "OpenCode subagent e2e passed"
