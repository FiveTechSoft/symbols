"""Controlled assertion conflict tests, not general prose contradiction tests."""
import pathlib,tempfile,subprocess,json,sys,re
root=pathlib.Path(__file__).resolve().parents[2];cli=sys.argv[1];fact=root/'docs/research/parameter_facts/windows-timeout.json';rows=[]
q='Which CMD command sleeps for five seconds?'
def check(name,text,expected):
 with tempfile.TemporaryDirectory() as d:
  p=pathlib.Path(d)/'corpus.txt';p.write_bytes(text.encode());out=subprocess.check_output([cli,str(p),str(fact),q],text=True)
  assert expected in out,(name,out)
  if expected=='SOURCE_CONFLICT':
   assert 'Command from corpus:' not in out
   ranges=re.findall(r'(Positive|Negative) assertion bytes\[(\d+),(\d+)\)',out);assert len(ranges)==2
   extracted={label:text.encode()[int(a):int(b)].decode() for label,a,b in ranges}
   assert extracted['Positive']=='Waiting uses timeout /t 5 in CMD.',extracted
   assert extracted['Negative']=='Waiting never uses timeout /t 5 in CMD.',extracted
  rows.append(dict(case=name,source=text,expected=expected,output=out))
pos='Waiting uses timeout /t 5 in CMD.';neg='Waiting never uses timeout /t 5 in CMD.'
check('forward',pos+'\n'+neg+'\n','SOURCE_CONFLICT')
check('reverse',neg+'\n'+pos+'\n','SOURCE_CONFLICT')
check('crlf',neg+'\r\n'+pos+'\r\n','SOURCE_CONFLICT')
check('offset','Introductory text.\n'+pos+'\nUnrelated.\n'+neg+'\n','SOURCE_CONFLICT')
for name,other in [('subject','Sleeping never uses timeout /t 5 in CMD.'),('shell','Waiting never uses timeout /t 5 in Bash.'),('quantity','Waiting never uses timeout /t 6 in CMD.'),('quoted','"'+neg+'"'),('conditional_prefix','If ready, '+neg),('conditional_suffix',neg[:-1]+' if ready.'),('option','Waiting never uses timeout /t 5 /nobreak in CMD.')]:
 check(name,pos+'\n'+other+'\n','Command from corpus: timeout /t 5')
check('negative_only',neg+'\n','NO_SUPPORT')
check('conditional_only','Waiting uses timeout /t 5 in CMD if ready.\n','NO_SUPPORT')
check('duplicate_positive',pos+'\n'+pos+'\n','AMBIGUOUS')
pathlib.Path(sys.argv[2]).write_text(json.dumps(rows,indent=2));print(len(rows),'controlled source-conflict checks PASS')
