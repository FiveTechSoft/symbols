"""Directed source-specific PowerShell subset, not independent holdout."""
import pathlib,subprocess,tempfile,json,sys,re
r=pathlib.Path(__file__).resolve().parents[2];cli=sys.argv[1];cmd=r/'docs/research/parameter_facts/windows-timeout.json';ps=r/'docs/research/parameter_facts/powershell-start-sleep.json';corpus=r/'data/shells/shell_corpus.txt';rows=[]
def run(name,q,expect,c=corpus,f=ps):
 out=subprocess.check_output([cli,str(c),str(cmd),q,str(f)],text=True);assert expect in out,(name,out);rows.append(dict(case=name,query=q,output=out));return out
q='Which PowerShell command sleeps for five seconds?'
out=run('original',q,'Command from corpus: Start-Sleep -Seconds 5');assert 'platform=unspecified' in out and 'bytes[15597,15619)' in out and 'NOT verified' in out
run('how','How do I sleep for five seconds in PowerShell?','Command from corpus: Start-Sleep -Seconds 5')
run('cmd_unchanged','Which CMD command sleeps for five seconds?','Command from corpus: timeout /t 5')
for p in ['Windows','Linux']:
 run('explicit_'+p,f'Which {p} PowerShell command sleeps for five seconds?','UNSUPPORTED_PLATFORM')
run('envelope_old_platform','duration-fact platform=Windows shell=PowerShell value=5 unit=seconds qualifier=default','UNSUPPORTED_PLATFORM')
run('envelope_unspecified','duration-fact platform=unspecified shell=PowerShell value=5 unit=seconds qualifier=default','Command from corpus: Start-Sleep -Seconds 5')
for platform in ['Windows','Linux']:
 run('how_platform_'+platform,f'How do I sleep for five seconds in {platform} PowerShell?','UNSUPPORTED_NATURAL_REQUEST')
for name,q2,expect in [('exact','Which PowerShell command sleeps for exactly five seconds?','NO_SUPPORT'),('keyboard','Which PowerShell command sleeps for five seconds without keyboard interruption?','NO_SUPPORT'),('milliseconds','Which PowerShell command sleeps for five milliseconds?','UNSUPPORTED_UNIT'),('fraction','Which PowerShell command sleeps for 1.5 seconds?','UNSUPPORTED_NATURAL_REQUEST'),('bash','Which Bash command sleeps for five seconds?','OUT_OF_SCOPE'),('absent','Which PowerShell command sleeps for six seconds?','NO_SUPPORT')]:run(name,q2,expect)
with tempfile.TemporaryDirectory() as d:
 p=pathlib.Path(d)/'source.txt';f=pathlib.Path(d)/'fact.json';f.write_bytes(ps.read_bytes()+b' ');run('tampered_fact',q,'UNVERIFIED_FACT',f=f)
 for n in [0,1,5,99,100]:
  p.write_text(f'Waiting uses Start-Sleep -Seconds {n} in PowerShell.\n');run(f'integer_{n}',f'Which PowerShell command sleeps for {n} seconds?','Command from corpus:' if n<100 else 'NO_SUPPORT',p)
 for command in ['Start-Sleep 5','Start-Sleep -s 5','Start-Sleep -Milliseconds 5000','Start-Sleep -Seconds 5.0','Start-Sleep -Seconds 05','Start-Sleep -Seconds 5 -Verbose','sleep 5']:
  p.write_text(f'Waiting uses {command} in PowerShell.\n');run(command,q,'NO_SUPPORT',p)
 for reverse in [False,True]:
  pos='Waiting uses Start-Sleep -Seconds 5 in PowerShell';neg='Waiting never uses sleep 6 in Bash, and Start-Sleep -Seconds 5 in PowerShell.';text='\r\n'.join([neg,pos] if reverse else [pos,neg])+'\r\n';p.write_bytes(text.encode());out=run(f'conflict_{reverse}',q,'SOURCE_CONFLICT',p)
  ranges={l:(int(a),int(b)) for l,a,b in re.findall(r'(Positive|Negative) assertion bytes\[(\d+),(\d+)\)',out)};assert ranges=={label:(text.index(line),text.index(line)+len(line)) for label,line in [('Positive',pos),('Negative',neg)]}
pathlib.Path(sys.argv[2]).write_text(json.dumps(rows,indent=2));print(len(rows),'PowerShell checks PASS')
