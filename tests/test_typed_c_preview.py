"""Static preview fixtures: no target build, run or workspace mutation."""
import base64
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'/'typed_c_contract'))
from preview import snapshot,validate,sha,installed_generator


class PreviewTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name);self.work=self.root/'work';self.work.mkdir()
        self.source=self.work/'main.c'
        self.source.write_text('#include <stdio.h>\nint main(void){int x=0; x*=3; printf("%d\\n",x); return 0;}\n')
        self.initial=self.source.read_bytes()
        self.contract=self.root/'contract.json'
        self.obj={'schema':'symbols.c-repair-contract.v1','workspace_digest':snapshot(self.work)[1],
                  'stdout':{'bytes_b64':'Mwo=','length':2,'termination':'exact'},
                  'exit_code':0,'probe':{'kind':'single-c-main','timeout_ms':5000},
                  'edit_scope':{'allow':['main.c'],'deny':[]},'source_predicates':[]}
    def run_preview(self):
        self.contract.write_text(json.dumps(self.obj))
        return validate(self.contract,self.work)
    def test_generator_is_release_fixed(self):
        generator=installed_generator()
        relative=generator.relative_to(Path(__file__).resolve().parents[1]).as_posix()
        expected={'build/c_contract_static'} if os.name!='nt' else {
            'build/c_contract_static.exe','build/Release/c_contract_static.exe',
            'build/Debug/c_contract_static.exe','build-asan/Debug/c_contract_static.exe'}
        self.assertIn(relative,expected)
        self.assertFalse(generator.is_symlink())
    def test_genuine_candidates_and_no_execution(self):
        generator=installed_generator()
        self.assertTrue(generator.is_file(),f'build c_contract_static first: {generator}')
        marker=self.root/'executed'
        self.source.write_text(self.initial.decode()+f'/* If executed, write {marker} */\n')
        self.obj['workspace_digest']=snapshot(self.work)[1]
        before=self.source.read_bytes();r=self.run_preview()
        self.assertEqual(r['status'],'static_candidate_unverified')
        self.assertTrue(r['not_executed'])
        self.assertGreater(r['candidate_count'],0)
        self.assertEqual(r['candidate_count'],len(r['candidates']))
        self.assertIn('init_mul',[c['rule'] for c in r['candidates']])
        self.assertTrue(any('int x=1' in c['unified_diff'] for c in r['candidates']))
        self.assertFalse(marker.exists())
        self.assertEqual(self.source.read_bytes(),before)
    def test_old_atime_is_not_a_source_race(self):
        if os.name=='nt': self.skipTest('POSIX atime fixture')
        # A read of an old file may advance atime under relatime; contents and identity are stable.
        mtime=self.source.stat().st_mtime_ns
        os.utime(self.source,ns=(mtime-7*24*3600*10**9,mtime))
        r=self.run_preview()
        self.assertEqual(r['status'],'static_candidate_unverified')
        self.assertGreater(r['candidate_count'],0)
        self.assertEqual(self.source.read_bytes(),self.initial)
    def test_simple_goal_literal_and_identity(self):
        self.source.write_text('#include <stdio.h>\nint main(void){ printf("ship\\n"); return 0; }\n')
        self.obj['workspace_digest']=snapshot(self.work)[1]
        self.obj['stdout']={'bytes_b64':base64.b64encode(b'arrive\n').decode(),
                            'length':7,'termination':'exact'}
        result=self.run_preview()
        self.assertEqual(result['candidate_count'],1)
        self.assertEqual(result['candidates'][0]['rule'],'answer_literal')
        self.assertIn('printf("arrive\\n")',result['candidates'][0]['unified_diff'])
        self.obj['stdout']={'bytes_b64':base64.b64encode(b'ship\n').decode(),
                            'length':5,'termination':'exact'}
        result=self.run_preview()
        self.assertEqual((result['status'],result['candidate_count']),('no_static_candidate',0))
    def test_directed_value_exact_candidate_and_abstention(self):
        self.source.write_text('#include <stdio.h>\nint main(void){int total=3; printf("paid=%d\\n",total); return 0;}\n')
        original=self.source.read_bytes()
        self.obj['workspace_digest']=snapshot(self.work)[1]
        def goal(data):
            self.obj['stdout']={'bytes_b64':base64.b64encode(data).decode(),
                                'length':len(data),'termination':'exact'}
            return self.run_preview()
        result=goal(b'paid=-12\n')
        self.assertEqual((result['status'],result['candidate_count']),('static_candidate_unverified',1))
        item=result['candidates'][0]
        self.assertEqual((item['rule'],item['tier'],item['path']),('answer_value',4,'main.c'))
        self.assertIn('-int main(void){int total=3;',item['unified_diff'])
        self.assertIn('+int main(void){int total=-12;',item['unified_diff'])
        self.assertEqual(item['before_sha256'],sha(original))
        self.assertEqual(item['after_sha256'],sha(original.replace(b'int total=3;',b'int total=-12;')))
        for data in (b'paid=3\n',b'paid=+12\n',b'paid=999999999999999999999\n',b'paid=12',b'nope=12\n'):
            result=goal(data)
            self.assertEqual((result['status'],result['candidate_count']),('no_static_candidate',0))
        self.assertEqual(self.source.read_bytes(),original)

    def test_scope_and_predicates(self):
        self.obj['source_predicates']=[{'kind':'file_bytes_equal','path':'main.c','sha256':sha(self.initial)}]
        self.assertEqual(self.run_preview()['status'],'no_static_candidate')
        self.obj['source_predicates']=[{'kind':'span_bytes_equal','path':'main.c','start':0,'length':8,'sha256':sha(self.initial[:8])}]
        self.assertGreater(self.run_preview()['candidate_count'],0)
        self.obj['source_predicates'][0]['sha256']='0'*64
        with self.assertRaisesRegex(ValueError,'predicate_mismatch'):self.run_preview()
    def test_workspace_and_path_refusals(self):
        self.source.write_text('int main(void){return 0;}')
        with self.assertRaisesRegex(ValueError,'workspace_digest'):self.run_preview()
        self.source.write_bytes(self.initial)
        self.obj['edit_scope']['allow']=['helper.c']
        with self.assertRaisesRegex(ValueError,'allow_path'):self.run_preview()
        self.obj['edit_scope']['allow']=['main.c']
        link=self.work/'link.c'
        try:link.symlink_to(self.source)
        except OSError as exc:self.skipTest(f'symlinks unavailable: {exc}')
        with self.assertRaisesRegex(ValueError,'symlink'):self.run_preview()
    def test_multifile_allow_deny_and_preserve(self):
        helper=self.work/'helper.c'
        helper.write_text('int helper(void){int y=0;y*=3;return y;}\n')
        self.obj['workspace_digest']=snapshot(self.work)[1]
        self.obj['edit_scope']={'allow':['main.c'],'deny':['helper.c']}
        self.obj['source_predicates']=[{'kind':'file_bytes_equal','path':'helper.c',
                                        'sha256':sha(helper.read_bytes())}]
        r=self.run_preview()
        self.assertGreater(r['candidate_count'],0)
        self.assertEqual({x['path'] for x in r['candidates']},{'main.c'})
        self.assertEqual(helper.read_text(),'int helper(void){int y=0;y*=3;return y;}\n')
    def test_no_static_candidate_is_not_runtime_failure(self):
        self.obj['stdout']={'bytes_b64':'','length':0,'termination':'exact'}
        r=self.run_preview()
        self.assertTrue(r['not_executed'])
        self.assertIn(r['status'],('static_candidate_unverified','no_static_candidate'))
        self.assertIn('stdout NOT verified',r['promise'])
    def test_span_guard_filters_shortened_candidate(self):
        self.source.write_text('#include <stdio.h>\nint main(void){puts("noise");puts("OK");return 0;}\n')
        original=self.source.read_bytes()
        self.obj['workspace_digest']=snapshot(self.work)[1]
        self.obj['source_predicates']=[{'kind':'span_bytes_equal','path':'main.c','start':0,
                                       'length':len(original),'sha256':sha(original)}]
        r=self.run_preview()
        self.assertEqual((r['status'],r['candidate_count']),('no_static_candidate',0))
        self.assertEqual(self.source.read_bytes(),original)
    def test_hardlink_refused_on_both_platforms(self):
        linked=self.root/'hardlink.c'
        try:os.link(self.source,linked)
        except OSError as exc:self.skipTest(f'hard links unavailable: {exc}')
        with self.assertRaisesRegex(ValueError,'file_limit'):
            self.run_preview()
        linked.unlink()
        linked_inside=self.work/'second.c'
        try:os.link(self.source,linked_inside)
        except OSError as exc:self.skipTest(f'workspace hard links unavailable: {exc}')
        with self.assertRaisesRegex(ValueError,'file_limit'):
            self.run_preview()
    def test_contract_hardlink_refused_on_both_platforms(self):
        self.contract.write_text(json.dumps(self.obj))
        linked=self.root/'contract_link.json'
        try:os.link(self.contract,linked)
        except OSError as exc:self.skipTest(f'hard links unavailable: {exc}')
        with self.assertRaisesRegex(ValueError,'contract_link'):
            validate(self.contract,self.work)
    def test_binary_goal_refuses_candidate_without_downgrade(self):
        self.obj['stdout']={'bytes_b64':base64.b64encode(b'A\x00B').decode(),
                            'length':3,'termination':'exact'}
        with self.assertRaisesRegex(ValueError,'literal_guard_unrepresentable'):
            self.run_preview()
        self.assertEqual(self.source.read_bytes(),self.initial)
    def test_no_output_literal_injection(self):
        self.obj['stdout']={'bytes_b64':base64.b64encode(b'MAGIC_NEVER_IN_SOURCE').decode(),
                            'length':len(b'MAGIC_NEVER_IN_SOURCE'),'termination':'exact'}
        r=self.run_preview()
        self.assertTrue(all('MAGIC_NEVER_IN_SOURCE' not in x['unified_diff'] for x in r['candidates']))
    def test_cli_refuses_mixed_or_unknown_flags(self):
        self.contract.write_text(json.dumps(self.obj))
        script=Path(__file__).resolve().parents[1]/'tools'/'typed_c_contract'/'preview.py'
        p=subprocess.run([sys.executable,str(script),'--preview-c-contract',str(self.contract),'-w',str(self.work),
                          '--continue-stdout-goal'],capture_output=True)
        self.assertNotEqual(p.returncode,0)
        self.assertEqual(self.source.read_bytes(),self.initial)

if __name__=='__main__':unittest.main()
