#!/usr/bin/env bash
# Regression for the OpenCode 1.18.32 repository-task wire shape. A streaming
# request must receive one terminated SSE tool-call turn, never JSON.
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
EXE="$ROOT/build/symbols-server"; [ -x "$EXE" ] || EXE="$ROOT/build-gcc/symbols-server"
[ -x "$EXE" ] || { echo "SKIP: symbols-server not built"; exit 77; }
command -v curl >/dev/null || { echo "SKIP: curl missing"; exit 77; }
PORT=$((19700 + ($$ % 200))); TMPD=$(mktemp -d); SRV=""
cleanup() { [ -n "$SRV" ] && kill "$SRV" 2>/dev/null; rm -rf "$TMPD"; }; trap cleanup EXIT
(cd "$ROOT" && exec "$EXE" "$PORT" "$TMPD" "data/c_lang/c_corpus.txt" >"$TMPD/server.log" 2>&1) & SRV=$!
for _ in $(seq 1 60); do curl -sf -o /dev/null "http://127.0.0.1:$PORT/v1/models" && break; sleep .1; done
cat >"$TMPD/request.json" <<'JSON'
{"model":"symbols","max_tokens":4096,"messages":[{"role":"system","content":"<env>\n  Working directory: /tmp/opencode-fixture\n  Is directory a git repo: yes\n  Platform: linux\n  Today's date: Tue Sep 22 2026\n</env>"},{"role":"user","content":"El programa falla al ejecutarlo con argumentos. Encuentra el fallo y corrígelo."}],"tools":[{"type":"function","function":{"name":"bash","parameters":{"type":"object","properties":{"command":{"type":"string"}}}}},{"type":"function","function":{"name":"glob","parameters":{"type":"object","properties":{"pattern":{"type":"string"}},"required":["pattern"]}}},{"type":"function","function":{"name":"read","parameters":{"type":"object","properties":{"filePath":{"type":"string"}},"required":["filePath"]}}},{"type":"function","function":{"name":"edit","parameters":{"type":"object","properties":{"filePath":{"type":"string"},"oldString":{"type":"string"},"newString":{"type":"string"}}}}}],"tool_choice":"auto","stream":true,"stream_options":{"include_usage":true}}
JSON
curl -sf -D "$TMPD/headers" -o "$TMPD/body" -X POST "http://127.0.0.1:$PORT/v1/chat/completions" -H 'Content-Type: application/json' --data-binary @"$TMPD/request.json"
grep -Eqi '^Content-Type: text/event-stream' "$TMPD/headers" || { echo "FAIL: streaming request returned non-SSE"; cat "$TMPD/headers"; exit 1; }
[ "$(grep -o '"finish_reason":"tool_calls"' "$TMPD/body" | wc -l)" -eq 1 ] || { echo "FAIL: expected one tool_calls finish"; cat "$TMPD/body"; exit 1; }
[ "$(grep -o '"name":"glob"' "$TMPD/body" | wc -l)" -eq 1 ] || { echo "FAIL: expected one glob dispatch"; cat "$TMPD/body"; exit 1; }
[ "$(grep -o 'data: \[DONE\]' "$TMPD/body" | wc -l)" -eq 1 ] || { echo "FAIL: expected one stream terminator"; cat "$TMPD/body"; exit 1; }
! grep -q '"object":"chat.completion"' "$TMPD/body" || { echo "FAIL: received non-stream completion"; exit 1; }
echo "OpenCode repository stream regression passed"
