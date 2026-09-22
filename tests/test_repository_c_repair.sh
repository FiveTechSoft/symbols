#!/usr/bin/env bash
# End-to-end repository repair protocol with an unseen scratch identifier typo.
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
EXE="$ROOT/build/symbols-server"; [ -x "$EXE" ] || EXE="$ROOT/build-gcc/symbols-server"
[ -x "$EXE" ] || { echo "SKIP: symbols-server not built"; exit 77; }
command -v curl >/dev/null || { echo "SKIP: curl missing"; exit 77; }
command -v python3 >/dev/null || { echo "SKIP: python3 missing"; exit 77; }
PORT=$((19500 + ($$ % 200))); TMPD=$(mktemp -d); SRV=""
cleanup(){ [ -n "$SRV" ] && kill "$SRV" 2>/dev/null; rm -rf "$TMPD"; }; trap cleanup EXIT
cat >"$TMPD/sample.c" <<'C'
#include <stdio.h>
int main(void){int observed_value=7; printf("%d\n",observd_value); return 0;}
C
(cd "$TMPD" && exec "$EXE" "$PORT" "$TMPD" >server.log 2>&1) & SRV=$!
for _ in $(seq 1 50); do curl -sf "http://127.0.0.1:$PORT/v1/models" >/dev/null && break; sleep .1; done
python3 - "$PORT" "$TMPD" <<'PY'
import json,sys,urllib.request
port=int(sys.argv[1]); root=sys.argv[2]; url=f'http://127.0.0.1:{port}/v1/chat/completions'
tools=[{'type':'function','function':{'name':n}} for n in ('glob','read','bash','edit')]
messages=[{'role':'system','content':f'<env>\n  Working directory: {root}\n</env>'},{'role':'user','content':'This C program does not compile. Fix it without changing its calculation.'}]
def post():
 d=json.dumps({'model':'symbols','messages':messages,'tools':tools,'stream':False}).encode();
 return json.load(urllib.request.urlopen(urllib.request.Request(url,d,{'Content-Type':'application/json'})))
def tool(name,call_id,content): messages.extend([answer['choices'][0]['message'],{'role':'tool','tool_call_id':call_id,'content':content}])
for step in range(5):
 answer=post(); msg=answer['choices'][0]['message']; calls=msg.get('tool_calls') or []
 if not calls: break
 c=calls[0]; name=c['function']['name']; args=json.loads(c['function']['arguments'])
 if name=='glob': content=f'{root}/sample.c'
 elif name=='read': content=open(root+'/sample.c').read()
 elif name=='bash':
  import subprocess
  p=subprocess.run(args['command'],shell=True,cwd=root,text=True,capture_output=True); content=p.stdout+p.stderr+f'\nexit_code: {p.returncode}'
 elif name=='edit':
  p=root+'/sample.c'; text=open(p).read(); assert args['oldString'] in text; open(p,'w').write(text.replace(args['oldString'],args['newString'],1)); content='Edit applied successfully.'
 else: raise AssertionError(name)
 tool(name,c['id'],content)
else: raise AssertionError('did not terminate')
assert 'verification' in msg.get('content','').lower() or 'verificación' in msg.get('content','').lower()
assert 'observed_value' in open(root+'/sample.c').read()
PY
grep -q 'observed_value);' "$TMPD/sample.c" || { echo 'FAIL: repair not applied'; exit 1; }
echo 'repository C repair protocol passed'
