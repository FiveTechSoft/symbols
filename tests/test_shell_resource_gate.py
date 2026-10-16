#!/usr/bin/env python3
"""Synthetic gate mechanics, not Windows/resource efficacy evidence."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from shell_resource_gate import FINAL, validate, run

HERE=Path(__file__).resolve().parent


def capture(arm,stage,count=63):
    return f'PSS_CAPTURE arm={arm} stage={stage} child_count={count} entries={count} walk_status=259 complete=1'


def positive():
    out=['RESOURCE_CONTROLS complete=1','PSS_ARM arm=count_only_pipe complete=1 deltas=0,3,1,3,1']
    for arm in ('snapshot_only_no_pipe','external_snapshot_pipe'):
        for stage in (0,0,1,2,3,4):out.append(capture(arm,stage))
        if arm=='snapshot_only_no_pipe':out.append(f'PSS_ARM arm={arm} complete=1 deltas=0,0,0,0,0')
    out.append('PSS_IDENTIFICATION arm=external_snapshot_pipe new=3 persistent=1 candidate_never_closed=1 complete=1')
    for mode in range(3):
        out.append(capture('resource_before',mode))
        out.append(f'RESOURCE_HELPER mode={mode} timeout=0 duplicate_error=0 cleanup=1')
        name='\\Device\\Example' if mode==1 else ''
        escaped=name.replace('\\','\\\\')
        out.append(f'RESOURCE_QUERY context=candidate mode={mode} attempted=1 returned=1 raw_return={len(name) if mode==1 else 0} harness_error=0 complete=1 type=0 error=0 ntstatus=0x00000000 iosb_status=0x00000000 iosb_bytes=0 pending=0 timeout=0 duplicate_error=0 close=1 name_chars={len(name)} name="{escaped}"')
        out.append(capture('resource_after',mode))
    out.extend(['RESOURCE_RESULT resource=full_path_observed owner=unproven target_stable=1 candidate_not_closed=1','PSS_ARM arm=external_snapshot_pipe complete=1 deltas=0,3,1,3,1',FINAL])
    return '\n'.join(out)+'\n'


class GateTests(unittest.TestCase):
    def setUp(self):self.text=positive()
    def rejects(self,text,code=0):
        with self.assertRaises((ValueError,UnicodeError)):validate(text,code)
    def test_positive_is_mechanics_only(self):self.assertTrue(validate(self.text,0))
    def test_nonzero_complete_looking(self):self.rejects(self.text,1)
    def test_controls_only(self):self.rejects('RESOURCE_CONTROLS complete=1\n'+FINAL+'\n')
    def test_frozen_455_failure(self):
        self.rejects((HERE/'fixtures/shell_resource_gate/ci455-red.txt').read_text(),1)
        self.rejects((HERE/'fixtures/shell_resource_gate/ci455-red.txt').read_text(),0)
    def test_missing_duplicate_reordered_query(self):
        lines=self.text.splitlines();queries=[x for x in lines if x.startswith('RESOURCE_QUERY ')]
        self.rejects(self.text.replace(queries[1]+'\n',''))
        self.rejects(self.text.replace(queries[1],queries[1]+'\n'+queries[1]))
        self.rejects(self.text.replace(queries[0],'PLACEHOLDER').replace(queries[2],queries[0]).replace('PLACEHOLDER',queries[2]))
    def test_negative_fields(self):
        for old,new in [('pending=0','pending=1'),('timeout=0','timeout=1'),('cleanup=1','cleanup=0'),('close=1','close=0'),('target_stable=1','target_stable=0'),('duplicate_error=0','duplicate_error=5'),('harness_error=0','harness_error=6'),('attempted=1','attempted=0'),('returned=1','returned=0')]:
            with self.subTest(new=new):self.rejects(self.text.replace(old,new))
    def test_missing_capture_marker_partial_or_invalid_name(self):
        self.rejects(self.text.replace(capture('resource_before',0)+'\n',''))
        self.rejects(self.text.replace(FINAL,'complete=1'))
        self.rejects(self.text+FINAL+'\n')
        self.rejects(self.text.replace('\\\\Device\\\\Example','partial'))
        self.rejects(self.text.replace('\\\\Device\\\\Example','\\\\Device\\\\Example\\x00'))
        self.rejects(self.text.replace('name_chars=15','name_chars=999'))
        self.rejects(self.text+'\ud800')
    def test_observer_or_incomplete(self):
        self.rejects('PSS_OBSERVER mismatch\n'+self.text)
        self.rejects(self.text.replace('walk_status=259','walk_status=5'))
        self.rejects(self.text.replace('entries=63','entries=64'))
        self.rejects(self.text.replace('new=3 persistent=1','new=2 persistent=1'))
    def test_subprocess_runner(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory); producer=root/'producer.py'
            producer.write_text('import sys\nfrom pathlib import Path\nsys.stdout.buffer.write(Path(sys.argv[1]).read_bytes())\nsys.exit(int(sys.argv[2]))\n')
            evidence=root/'evidence.txt';evidence.write_text(self.text,encoding='utf-8')
            command=[sys.executable,str(HERE/'shell_resource_gate.py'),'--',sys.executable,str(producer),str(evidence)]
            for code,expected in [(0,0),(1,1)]:
                p=subprocess.run(command+[str(code)],capture_output=True,timeout=15)
                self.assertEqual(p.returncode,expected)
            evidence.write_bytes(b'\xff\xfe');p=subprocess.run(command+['0'],capture_output=True,timeout=15);self.assertNotEqual(p.returncode,0)
    def test_timeout(self):
        self.assertNotEqual(run([sys.executable,'-c','import time;time.sleep(10)'],timeout=0.05),0)
    def test_real_ctest_mini_project(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);evidence=root/'evidence.txt';evidence.write_text(self.text,encoding='utf-8')
            producer=root/'producer.py';producer.write_text('import sys\nfrom pathlib import Path\nsys.stdout.write(Path(sys.argv[1]).read_text())\nsys.exit(int(sys.argv[2]))\n')
            def q(path):return '[=['+str(path).replace('\\','/')+']=]'
            script=['cmake_minimum_required(VERSION 3.16)','project(Gate NONE)','enable_testing()']
            for name,code in [('zero',0),('nonzero',1)]:
                script.append(f'add_test(NAME {name} COMMAND {q(sys.executable)} {q(HERE/"shell_resource_gate.py")} -- {q(sys.executable)} {q(producer)} {q(evidence)} {code})')
            (root/'CMakeLists.txt').write_text('\n'.join(script)+'\n')
            subprocess.run(['cmake','-S',str(root),'-B',str(root/'build')],capture_output=True,check=True,timeout=30)
            for name,expected in [('zero',0),('nonzero',8)]:
                p=subprocess.run(['ctest','--test-dir',str(root/'build'),'-C','Release','-R','^'+name+'$','--output-on-failure'],capture_output=True,timeout=30)
                self.assertEqual(p.returncode,expected,p.stdout.decode(errors='replace'))


if __name__=='__main__':unittest.main()
