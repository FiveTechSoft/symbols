"""Directed grammar tests, not an independent natural-language holdout."""
import subprocess,pathlib,tempfile,json,sys
root=pathlib.Path(__file__).resolve().parents[2]; cli=sys.argv[1] if len(sys.argv)>1 else '/tmp/duration-join-cli'; corpus=root/'data/shells/shell_corpus.txt'; fact=root/'docs/research/parameter_facts/windows-timeout.json';rows=[]
def check(q,expected,c=corpus):
 out=subprocess.check_output([cli,str(c),str(fact),q],text=True)
 assert expected in out,(q,expected,out)
 rows.append(dict(query=q,expected=expected,output=out))
for q in ['Which CMD command sleeps for five seconds?','Which CMD command sleeps for five seconds?\n','What CMD command waits for 5 seconds?','Which command in CMD pauses for five seconds?','What Windows CMD command pauses for 5 seconds?','WHICH CMD COMMAND SLEEPS FOR FIVE SECONDS?']:
 check(q,'Command from corpus: timeout /t 5')
for q,e in [('Which CMD command sleeps for six seconds?','NO_SUPPORT'),('Which CMD command sleeps for five milliseconds?','UNSUPPORTED_UNIT'),('Which PowerShell command sleeps for five seconds?','OUT_OF_SCOPE'),('Which Linux CMD command sleeps for five seconds?','OUT_OF_SCOPE'),('Which CMD command sleeps for exactly five seconds?','NO_SUPPORT'),('Which CMD command sleeps for five seconds without keyboard interruption?','NO_SUPPORT'),('Which CMD command does not sleep for five seconds?','UNSUPPORTED_NATURAL_REQUEST'),('Which CMD command sleeps for five seconds and deletes files?','UNSUPPORTED_NATURAL_REQUEST'),('Which CMD command sleeps for five seconds if the disk is full?','UNSUPPORTED_NATURAL_REQUEST'),('Which CMD command sleeps for five or six seconds?','UNSUPPORTED_NATURAL_REQUEST'),('Which CMD command sleeps for -1 seconds?','UNSUPPORTED_NATURAL_REQUEST'),('Which CMD command sleeps for one hundred seconds?','UNSUPPORTED_NATURAL_REQUEST'),('Which CMD command sleeps for 999999999999999999999999 seconds?','UNSUPPORTED_NATURAL_REQUEST'),('Which CMD command sleeps for five seconds? do more','UNSUPPORTED_NATURAL_REQUEST'),('Which CMD command sleeps for five seconds '+('and '*60),'UNSUPPORTED_NATURAL_REQUEST')]:check(q,e)
with tempfile.TemporaryDirectory() as d:
 p=pathlib.Path(d)/'corpus.txt'
 for value,word,unit in [(0,'zero','seconds'),(1,'one','second'),(6,'six','seconds'),(21,'twenty one','seconds'),(99,'ninety nine','seconds'),(99999,'99999','seconds')]:
  p.write_text(f'Waiting uses timeout /t {value} in CMD.\n')
  check(f'Which CMD command waits for {word} {unit}?',f'Command from corpus: timeout /t {value}',p)
 p.write_text('Waiting uses timeout /t 5 /nobreak in CMD.\n')
 check('Which CMD command pauses for five seconds without keyboard interruption?','Command from corpus: timeout /t 5 /nobreak',p)
 p.write_text('Waiting uses timeout /t -1 in CMD.\n')
 check('Which CMD command sleeps for five seconds?','NO_SUPPORT',p)
 p.write_text('Waiting uses timeout /t 100000 in CMD.\n')
 check('Which CMD command sleeps for 100000 seconds?','NO_SUPPORT',p)
pathlib.Path(sys.argv[2] if len(sys.argv)>2 else '/tmp/duration-natural-tests.json').write_text(json.dumps(rows,indent=2));print(len(rows),'directed natural checks PASS')
