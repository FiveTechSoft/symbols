"""Directed compositional request-frame matrix. Not an independent holdout."""
import pathlib,subprocess,tempfile,json,sys
root=pathlib.Path(__file__).resolve().parents[2];cli=sys.argv[1];fact=root/'docs/research/parameter_facts/windows-timeout.json';rows=[]
def check(q,expected,corpus):
 out=subprocess.check_output([cli,str(corpus),str(fact),q],text=True);assert expected in out,(q,expected,out);rows.append(dict(query=q,expected=expected,output=out))
with tempfile.TemporaryDirectory() as d:
 p=pathlib.Path(d)/'corpus.txt'
 for number,word in [(1,'one'),(5,'five'),(6,'six'),(21,'twenty one')]:
  p.write_text(f'Waiting uses timeout /t {number} in CMD.\n')
  for verb in ['sleep','wait','pause']:
   for form in [f'How do I {verb} for {word} seconds in CMD?',f'Which CMD command {verb}s {word} seconds?']:
    check(form,f'Command from corpus: timeout /t {number}',p)
    check(form.replace(word,str(number+1)),'NO_SUPPORT',p)
    check(form.replace('seconds','milliseconds'),'UNSUPPORTED_UNIT',p)
    check(form.replace('CMD','PowerShell'),'OUT_OF_SCOPE',p)
    check(form.replace(word,'exactly '+word),'NO_SUPPORT',p)
    check(form[:-1]+' and delete files?','UNSUPPORTED_NATURAL_REQUEST',p)
    check(form[:-1]+' if ready?','UNSUPPORTED_NATURAL_REQUEST',p)
  check(f'How do I not wait for {word} seconds in CMD?','UNSUPPORTED_NATURAL_REQUEST',p)
  check(f'Which CMD command does not wait {word} seconds?','UNSUPPORTED_NATURAL_REQUEST',p)
  check(f'How do I wait for {word} seconds in CMD without keyboard interruption?','NO_SUPPORT',p)
 p.write_text('Waiting uses timeout /t 5 /nobreak in CMD.\n')
 check('How do I pause for five seconds in CMD without keyboard interruption?','Command from corpus: timeout /t 5 /nobreak',p)
for q in ['How do I wait for five seconds in CMD?','Which CMD command waits five seconds?']:
 check(q,'Command from corpus: timeout /t 5',root/'data/shells/shell_corpus.txt')
for q in ['How do I wait for six seconds in CMD?','Which CMD command waits six seconds?']:
 check(q,'NO_SUPPORT',root/'data/shells/shell_corpus.txt')
pathlib.Path(sys.argv[2]).write_text(json.dumps(rows,indent=2));print(len(rows),'directed request-frame matrix checks PASS')
