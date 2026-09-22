#!/usr/bin/env bash
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
EXE="$ROOT/build-gcc/symbols-server"; [ -x "$EXE" ] || EXE="$ROOT/build/symbols-server"
[ -x "$EXE" ] || { echo "SKIP: symbols-server not built"; exit 77; }
command -v curl >/dev/null || { echo "SKIP: curl missing"; exit 77; }
PORT=$((19200 + ($$ % 500))); TMPD=$(mktemp -d); SRV=""
cleanup() { [ -n "$SRV" ] && kill "$SRV" 2>/dev/null; rm -rf "$TMPD"; }; trap cleanup EXIT
(cd "$ROOT" && exec "$EXE" "$PORT" "$TMPD" "data/c_lang/c_corpus.txt" >"$TMPD/server.log" 2>&1) & SRV=$!
for _ in $(seq 1 60); do curl -sf -o /dev/null "http://127.0.0.1:$PORT/v1/models" && break; sleep .1; done
cat >"$TMPD/user.json" <<'JSON'
{"model":"symbols","messages":[{"role":"system","content":"<env>\n  Working directory: /tmp/oc-uname\n  Platform: linux\n</env>"},{"role":"user","content":"uname -a"}],"tools":[{"type":"function","function":{"name":"bash"}},{"type":"function","function":{"name":"edit"}},{"type":"function","function":{"name":"glob"}},{"type":"function","function":{"name":"grep"}},{"type":"function","function":{"name":"question"}},{"type":"function","function":{"name":"read"}},{"type":"function","function":{"name":"skill"}},{"type":"function","function":{"name":"task"}},{"type":"function","function":{"name":"todowrite"}},{"type":"function","function":{"name":"webfetch"}},{"type":"function","function":{"name":"write"}}],"tool_choice":"auto","stream":false}
JSON
curl -sf -X POST "http://127.0.0.1:$PORT/v1/chat/completions" -H 'Content-Type: application/json' --data-binary @"$TMPD/user.json" >"$TMPD/first.json"
grep -q '"name":"bash"' "$TMPD/first.json" || { echo "FAIL: bash tool not selected"; exit 1; }
grep -q '\\"command\\":\\"uname -a\\"' "$TMPD/first.json" || { echo "FAIL: exact command not emitted"; exit 1; }
python3 - "$TMPD/user.json" "$TMPD/tool.json" <<'PY'
import json,sys
x=json.load(open(sys.argv[1])); x['messages'] += [
 {'role':'assistant','content':'','tool_calls':[{'id':'call_shell_test','type':'function','function':{'name':'bash','arguments':'{"command":"uname -a"}'}}]},
 {'role':'tool','tool_call_id':'call_shell_test','name':'bash','content':'{"exit_code":0,"stdout":"Linux e2b.local TEST GNU/Linux\\n","stderr":""}'}]
json.dump(x,open(sys.argv[2],'w'))
PY
curl -sf -X POST "http://127.0.0.1:$PORT/v1/chat/completions" -H 'Content-Type: application/json' --data-binary @"$TMPD/tool.json" >"$TMPD/second.json"
grep -q 'Linux e2b.local TEST GNU/Linux' "$TMPD/second.json" || { echo "FAIL: stdout not relayed"; cat "$TMPD/second.json"; exit 1; }
grep -q 'Exit status: 0' "$TMPD/second.json" || { echo "FAIL: exit status not relayed"; exit 1; }
if grep -Eqi 'tests passed|regression|compiled successfully|build succeeded' "$TMPD/second.json"; then echo "FAIL: unsupported claim"; exit 1; fi
echo "OpenCode uname shell routing passed"
