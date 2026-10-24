"""Controlled polarity symmetry, with explicit original-byte range identity."""
import pathlib,subprocess,tempfile,json,sys,re,itertools
root=pathlib.Path(__file__).resolve().parents[2];cli=sys.argv[1];fact=root/'docs/research/parameter_facts/windows-timeout.json';q='Which CMD command sleeps for five seconds?';rows=[]
def check(name,positive,negative,reverse,conflict=True):
 text='\r\n'.join(([negative,positive] if reverse else [positive,negative]))+'\r\n'
 with tempfile.TemporaryDirectory() as d:
  p=pathlib.Path(d)/'corpus.txt';p.write_bytes(text.encode());out=subprocess.check_output([cli,str(p),str(fact),q],text=True)
  if conflict:
   assert 'SOURCE_CONFLICT' in out and 'Command from corpus:' not in out,(name,out)
   got={label:(int(a),int(b)) for label,a,b in re.findall(r'(Positive|Negative) assertion bytes\[(\d+),(\d+)\)',out)}
   expected={label:(text.index(line),text.index(line)+len(line)) for label,line in [('Positive',positive),('Negative',negative)]}
   assert got==expected,(name,got,expected)
  else:assert 'Command from corpus: timeout /t 5' in out and 'SOURCE_CONFLICT' not in out,(name,out)
  rows.append(dict(case=name,source=text,output=out))
for dotp,dotn,position,reverse in itertools.product(['','.'],['','.'],['single','first','last'],[False,True]):
 pos='Waiting uses timeout /t 5 in CMD'+dotp
 neg={'single':'Waiting never uses timeout /t 5 in CMD','first':'Waiting never uses timeout /t 5 in CMD, and sleep 6 in Bash','last':'Waiting never uses sleep 6 in Bash, and timeout /t 5 in CMD'}[position]+dotn
 check(f'{dotp!r}-{dotn!r}-{position}-{reverse}',pos,neg,reverse)
for name,neg in [('subject','Sleeping never uses timeout /t 5 in CMD'),('shell','Waiting never uses timeout /t 5 in Bash'),('quantity','Waiting never uses timeout /t 6 in CMD'),('quoted','"Waiting never uses timeout /t 5 in CMD"'),('condition','Waiting never uses timeout /t 5 in CMD if ready')]:
 check(name,'Waiting uses timeout /t 5 in CMD',neg,False,False)
for name,neg in [('malformed_coordinate','Waiting never uses timeout /t 5 in CMD, and broken'),('conditional_coordinate','Waiting never uses timeout /t 5 in CMD, and sleep 6 in Bash if ready')]:
 check(name,'Waiting uses timeout /t 5 in CMD',neg,False,False)
# Positive coordination must use the same reader and identify the complete original line.
check('positive_coordinate','Waiting uses sleep 5 in Bash, and timeout /t 5 in CMD.','Waiting never uses timeout /t 5 in CMD',True)
# Duplicates preserve last positive / earliest negative range identity.
text='Waiting never uses timeout /t 5 in CMD\r\nWaiting uses timeout /t 5 in CMD\r\nWaiting never uses timeout /t 5 in CMD\r\nWaiting uses timeout /t 5 in CMD\r\n'
with tempfile.TemporaryDirectory() as d:
 p=pathlib.Path(d)/'corpus.txt';p.write_bytes(text.encode());out=subprocess.check_output([cli,str(p),str(fact),q],text=True)
 got={label:(int(a),int(b)) for label,a,b in re.findall(r'(Positive|Negative) assertion bytes\[(\d+),(\d+)\)',out)}
 pos='Waiting uses timeout /t 5 in CMD';neg='Waiting never uses timeout /t 5 in CMD'
 assert got=={'Positive':(text.rindex(pos),text.rindex(pos)+len(pos)),'Negative':(0,len(neg))},got
 rows.append(dict(case='duplicate_exact_offsets',source=text,output=out))
pathlib.Path(sys.argv[2]).write_text(json.dumps(rows,indent=2));print(len(rows),'symmetric source checks PASS')
