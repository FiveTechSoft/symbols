"""V2 refusal and policy tests. Never build or run a target source."""
import base64
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'/'typed_c_contract'))
from preview import snapshot
from verify import verify, shape, apply_diff, choose, eligible, interpret, sha

class VerifyTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.base=Path(self.tmp.name);self.work=self.base/'work';self.work.mkdir()
        self.source=self.work/'main.c'
        self.before=b'#include <stdio.h>\nint main(void){int n=1;printf("n=%d\\n",n);return 0;}\n'
        self.source.write_bytes(self.before)
        self.contract=self.base/'v2.json'
        self.obj={'schema':'symbols.c-repair-contract.v2','workspace_digest':snapshot(self.work)[1],
            'stdout':{'bytes_b64':base64.b64encode(b'n=8\n').decode(),'length':4,'termination':'exact'},
            'exit_code':0,'probe':{'kind':'single-c-main','timeout_ms':5000},
            'edit_scope':{'allow':['main.c'],'deny':[]},'source_predicates':[]}
    def call(self,**kwargs):
        self.contract.write_text(json.dumps(self.obj))
        return verify(self.contract,self.work,**kwargs)
    def test_real_cli_fails_closed(self):
        result=self.call()
        self.assertEqual(result['status'],'probe_unavailable')
        self.assertFalse(result['executed'])
        self.assertIsNone(result['observation'])
        self.assertEqual(result['candidate']['rule'],'answer_value')
        self.assertEqual(self.source.read_bytes(),self.before)
        p=subprocess.run([sys.executable,str(Path(__file__).resolve().parents[1]/'tools/typed_c_contract/verify.py'),
                          '--verify-c-contract',str(self.contract),'-w',str(self.work)],capture_output=True)
        self.assertEqual((p.returncode,p.stderr),(0,b''))
        self.assertEqual(json.loads(p.stdout)['status'],'probe_unavailable')
    def test_v1_cannot_activate(self):
        self.obj['schema']='symbols.c-repair-contract.v1'
        with self.assertRaisesRegex(ValueError,'version'):self.call()
        self.contract.write_text(json.dumps(self.obj))
        p=subprocess.run([sys.executable,str(Path(__file__).resolve().parents[1]/'tools/typed_c_contract/preview.py'),
                          '--preview-c-contract',str(self.contract),'-w',str(self.work)],capture_output=True)
        self.assertEqual(p.returncode,0)
        self.assertTrue(json.loads(p.stdout)['not_executed'])
        self.obj['schema']='symbols.c-repair-contract.v2'
        self.contract.write_text(json.dumps(self.obj))
        p=subprocess.run([sys.executable,str(Path(__file__).resolve().parents[1]/'tools/typed_c_contract/preview.py'),
                          '--preview-c-contract',str(self.contract),'-w',str(self.work)],capture_output=True)
        self.assertEqual((p.returncode,p.stdout,p.stderr),(2,b'',b'refused: version\n'))
    def test_ineligible_and_no_broker(self):
        class Spy:
            def observe(self,request):raise AssertionError('broker called')
        self.source.write_bytes(self.before.replace(b'int n=1;',b'int n=1;n++;'))
        self.obj['workspace_digest']=snapshot(self.work)[1]
        self.assertEqual(self.call(broker=Spy())['status'],'no_eligible_candidate')
        self.source.write_bytes(self.before)
        (self.work/'other.c').write_text('int z;')
        self.obj['workspace_digest']=snapshot(self.work)[1]
        self.assertEqual(self.call(broker=Spy())['status'],'no_eligible_candidate')
    def test_candidate_forgery_and_ambiguity(self):
        import difflib
        after=self.before.replace(b'int n=1;',b'int n=8;')
        good={'path':'main.c','rule':'answer_value','tier':4,'before_sha256':sha(self.before),
              'after_sha256':sha(after),'unified_diff':''.join(difflib.unified_diff(
                  self.before.decode().splitlines(True),after.decode().splitlines(True),
                  fromfile='before/main.c',tofile='after/main.c',n=2))}
        producer=lambda files,contract,expected:[good]
        self.assertEqual(self.call(producer=producer)['status'],'probe_unavailable')
        fake=dict(good,after_sha256='0'*64)
        self.assertEqual(self.call(producer=lambda *_:[fake])['status'],'no_eligible_candidate')
        rogue=self.before.replace(b'int n=1;',b'int n=8;system("bad");')
        fake=dict(good,after_sha256=sha(rogue),unified_diff=''.join(difflib.unified_diff(
            self.before.decode().splitlines(True),rogue.decode().splitlines(True),
            fromfile='before/main.c',tofile='after/main.c',n=2)))
        self.assertEqual(self.call(producer=lambda *_:[fake])['status'],'no_eligible_candidate')
        self.assertEqual(self.call(producer=lambda *_:[good,good])['status'],'ambiguous_candidate')
        self.assertIsNone(self.call(producer=lambda *_:[good,good])['candidate'])
    def test_broker_fake_all_classes(self):
        class Broker:
            def __init__(self,response):self.response=response
            def observe(self,request):
                self_request=request
                assert self_request['after'] and self_request['goal']==b'n=8\n'
                return self.response
        for outcome in ('compile_error','compile_timeout','run_timeout','run_crash',
                        'exit_mismatch','stdout_mismatch','output_limit','infrastructure_error'):
            response={'outcome':outcome,'started':not outcome.startswith('compile_')}
            if outcome in ('exit_mismatch','stdout_mismatch'):
                response.update(stdout=b'wrong',complete=True,exit_code=1)
            with self.subTest(outcome=outcome):
                result=self.call(broker=Broker(response))
                self.assertEqual(result['status'],outcome)
                self.assertEqual(result['executed'],response['started'])
        observed={'outcome':'observed_match','started':True,'stdout':b'n=8\n','complete':True,
                  'exit_code':0,'image_id':'test-image','compiler_id':'test-cc','platform_id':'test-platform',
                  'limits':{'wall_ms':5000,'memory_bytes':67108864,'output_bytes':4096}}
        self.assertEqual(self.call(broker=Broker(observed))['status'],'observed_match')
        for stdout in (b'n=8',b'n=8\r\n',b'n=8\n\x00'):
            self.assertEqual(self.call(broker=Broker(dict(observed,stdout=stdout)))['status'],'infrastructure_error')
    def test_v1_golden_invariant_and_no_broker(self):
        from preview import validate
        self.obj['schema']='symbols.c-repair-contract.v1'
        self.contract.write_text(json.dumps(self.obj))
        with mock.patch('verify.UnavailableBroker.observe',side_effect=AssertionError('broker')):
            result=validate(self.contract,self.work)
        self.assertEqual(set(result),{'status','promise','not_executed','provenance',
                                     'contract_sha256','workspace_digest','candidate_count','candidates'})
        self.assertEqual(result['promise'],'contract validated + static candidate preview; stdout NOT verified, nothing executed')
        self.assertEqual((result['status'],result['candidate_count'],result['candidates'][0]['rule']),
                         ('static_candidate_unverified',1,'answer_value'))
        self.assertEqual(result['candidates'][0]['after_sha256'],sha(self.before.replace(b'int n=1;',b'int n=8;')))
    def test_crossed_rule_and_trigraph_rejected(self):
        import difflib
        after=self.before.replace(b'int n=1;',b'int n=8;')
        fake={'path':'main.c','rule':'answer_literal','tier':4,'before_sha256':sha(self.before),
              'after_sha256':sha(after),'unified_diff':''.join(difflib.unified_diff(
                  self.before.decode().splitlines(True),after.decode().splitlines(True),
                  fromfile='before/main.c',tofile='after/main.c',n=2))}
        self.assertEqual(self.call(producer=lambda *_:[fake])['status'],'no_eligible_candidate')
        self.assertIsNone(shape(self.before.replace(b'int n=1;',b'int n=1;??=include "hidden.h";')))
    def test_predicate_and_snapshot_guard(self):
        self.obj['source_predicates']=[{'kind':'file_bytes_equal','path':'main.c','sha256':sha(self.before)}]
        self.assertEqual(self.call()['status'],'no_eligible_candidate')
        self.obj['source_predicates']=[]
        self.source.write_bytes(b'changed')
        with self.assertRaisesRegex(ValueError,'workspace_digest'):self.call()

if __name__=='__main__':unittest.main()
