#!/usr/bin/env python3
"""Synthetic protocol bank: mechanical gate evidence, never native evidence."""
import json
import pathlib
import subprocess
import sys
import tempfile
import unittest
from shell_spawn_v2_gate import FINAL, validate, run

HERE = pathlib.Path(__file__).resolve().parent


def complete():
    rows=['positive ok=1 exit=0 launch=0 timeout=0 failure_mask=5','private runner mode=positive exit=0','RESOURCE_CONTROLS complete=1']
    def cap(arm,call,n):rows.append(f'PSS_CAPTURE arm={arm} stage={call} child_count={n} entries={n} walk_status=259 complete=1')
    for kind,arm in enumerate(['observer_no_call','count_only_invalid','observed_invalid']):
     if kind:rows.append(f'SPAWN_PROBE kind={kind} ok=0 error=216 witness=0')
     for call in range(28):
      n=80+(1 if kind and call else 0)
      rows.append(f'SPAWN_CHECKPOINT arm={arm} call={call} count={n}')
      if kind and call:rows.append(f'SPAWN_RESULT arm={arm} call={call} mask=0 ok=0 exit=-1 launch=1 timeout=0 stored=0/0 total=0/0 truncated=0/0 first=0/0 witness=0')
      if kind!=1:
       cap(arm,call,n)
       if call==0:cap(arm,call,n)
       elif kind:
        rows.append(f'SPAWN_TABLE call={call} baseline=80 entries=81 baseline_stable=1 extras=1 candidate=0xb8 type=File type_bytes=10 flags=5 candidate_stable=1')
        if call in (1,27):
         for mode in (0,1):
          cap('spawn_query_before',call*10+mode,n)
          rows.append(f'RESOURCE_HELPER mode={mode} timeout=0 duplicate_error=0 cleanup=1')
          name='' if mode==0 else r'\\Device\\NamedPipe\\'
          rows.append(f'RESOURCE_QUERY context=spawn_call_{call} mode={mode} attempted=1 returned=1 raw_return={3 if mode==0 else 18} harness_error=0 complete=1 type={3 if mode==0 else 0} error=0 ntstatus=0x00000000 iosb_status=0x00000000 iosb_bytes=0 pending=0 timeout=0 duplicate_error=0 close=1 name_chars={0 if mode==0 else 18} name="{name}"')
          cap('spawn_query_after',call*10+mode,n)
         rows.append(f'SPAWN_RESOURCE call={call} candidate=0xb8 exact_nt_root=1 file_type=3 target_stable=1 candidate_not_closed=1')
     rows.append(f'SPAWN_ARM arm={arm} complete=1 calls=27 baseline=80 final={80 if kind==0 else 81} child_exit=0 cleanup=1')
    rows.append(FINAL)
    return '\n'.join(rows)+'\n'

class Gate(unittest.TestCase):
    def test_complete(self):
        self.assertTrue(validate(complete(), 0))

    def test_gold_negatives(self):
        good = complete()
        def replace(a, b):
            self.assertIn(a, good)
            return good.replace(a, b, 1)
        cases = {
            'flat0': replace('count_only_invalid call=1 count=81', 'count_only_invalid call=1 count=80'),
            'firstdelta2': replace('observed_invalid call=1 count=81', 'observed_invalid call=1 count=82'),
            'delayeddelta1': replace('observed_invalid call=1 count=81', 'observed_invalid call=1 count=80'),
            'latergrowth': replace('observed_invalid call=27 count=81', 'observed_invalid call=27 count=82'),
            'latershrink': replace('observed_invalid call=27 count=81', 'observed_invalid call=27 count=80'),
            'netstable_baseline_replacement': replace('baseline_stable=1', 'baseline_stable=0'),
            'candidate_present_baseline': replace('extras=1', 'extras=0'),
            'candidate_identity_changes': replace('candidate=0xb8', 'candidate=0xbc'),
            'wrongtype': replace('type=File', 'type=Event'),
            'no_type_flag': replace('flags=5', 'flags=4'),
            'odd_typebytes': replace('type_bytes=10', 'type_bytes=9'),
            'extra_type_NUL': replace('type_bytes=10', 'type_bytes=12'),
            'missing_checkpoint': replace('SPAWN_CHECKPOINT arm=observed_invalid call=4 count=81\n', ''),
            'duplicate_checkpoint': replace('SPAWN_CHECKPOINT arm=observed_invalid call=4 count=81\n', 'SPAWN_CHECKPOINT arm=observed_invalid call=4 count=81\n'*2),
            'out_of_order_checkpoint': replace('arm=observed_invalid call=4 count=81', 'arm=observed_invalid call=5 count=81'),
            'wrong_result_status': replace('mask=0 ok=0 exit=-1', 'mask=0 ok=1 exit=0'),
            'capture_not_empty': replace('stored=0/0', 'stored=1/0'),
            'witness_created': replace('first=0/0 witness=0', 'first=0/0 witness=1'),
            'wrongpath_prefix': replace('name="\\\\Device\\\\NamedPipe\\\\"', 'name="\\\\Device\\\\Other\\\\NamedPipe\\\\"'),
            'wrongpath_suffix': replace('name="\\\\Device\\\\NamedPipe\\\\"', 'name="\\\\Device\\\\NamedPipe\\\\child"'),
            'relative_name_only': replace('name="\\\\Device\\\\NamedPipe\\\\"', 'name="\\\\"'),
            'wrongpath_case': replace('name="\\\\Device\\\\NamedPipe\\\\"', 'name="\\\\device\\\\NamedPipe\\\\"'),
            'wrongpath_NUL': replace('name="\\\\Device\\\\NamedPipe\\\\"', 'name="\\\\Device\\\\NamedPipe\\\\\\x00"'),
            'wrongpath_length': replace('name_chars=18', 'name_chars=19'),
            'PSS_incomplete': replace('walk_status=259 complete=1', 'walk_status=259 complete=0'),
            'PSS_walk_fail': replace('walk_status=259', 'walk_status=5'),
            'PSS_free_fail': good.replace(FINAL, 'PSS_ERROR snapshot_free=5\n'+FINAL),
            'PSS_target_count_changed': replace('child_count=81 entries=81', 'child_count=82 entries=81'),
            'duplicate_fail': replace('RESOURCE_HELPER mode=0 timeout=0 duplicate_error=0', 'RESOURCE_HELPER mode=0 timeout=0 duplicate_error=5'),
            'helper_cleanup_fail': replace('RESOURCE_HELPER mode=0 timeout=0 duplicate_error=0 cleanup=1', 'RESOURCE_HELPER mode=0 timeout=0 duplicate_error=0 cleanup=0'),
            'query_pending': replace('pending=0 timeout=0 duplicate_error=0 close=1', 'pending=1 timeout=0 duplicate_error=0 close=1'),
            'query_timeout': replace('pending=0 timeout=0 duplicate_error=0 close=1', 'pending=0 timeout=1 duplicate_error=0 close=1'),
            'bad_controls': replace('RESOURCE_CONTROLS complete=1', 'RESOURCE_CONTROLS complete=0'),
            'native_exit1_with_complete_output': good,
            'only_control_complete1': 'RESOURCE_CONTROLS complete=1\n',
            'missing_final_marker': replace(FINAL, ''),
            'invalidUTF8': good+'\udcff',
        }
        gold = json.loads((HERE/'shell_spawn_win_v2_gold.json').read_text())
        self.assertEqual(set(gold['negative_bank']), set(cases) | {'runner_timeout'})
        for name, text in cases.items():
            with self.subTest(name=name):
                with self.assertRaises((ValueError, UnicodeError)):
                    validate(text, 1 if name == 'native_exit1_with_complete_output' else 0)

    def test_runner_subprocess(self):
        with tempfile.TemporaryDirectory() as directory:
            d=pathlib.Path(directory); stream=d/'stream.txt';stream.write_text(complete(),encoding='utf-8')
            producer=d/'producer.py';producer.write_text('import pathlib,sys\nsys.stdout.buffer.write(pathlib.Path(sys.argv[1]).read_bytes())\nsys.exit(int(sys.argv[2]))\n')
            for code in (0,1):
                p=subprocess.run([sys.executable,str(HERE/'shell_spawn_v2_gate.py'),'--',sys.executable,str(producer),str(stream),str(code)],capture_output=True)
                self.assertEqual(p.returncode,code)
            stream.write_bytes(b'\xff')
            p=subprocess.run([sys.executable,str(HERE/'shell_spawn_v2_gate.py'),'--',sys.executable,str(producer),str(stream),'0'],capture_output=True)
            self.assertEqual(p.returncode,1)
            self.assertEqual(run([sys.executable,'-c','import time;time.sleep(5)'],timeout=0.05),1)

    def test_ctest_nonzero_not_passed(self):
        with tempfile.TemporaryDirectory() as directory:
            d=pathlib.Path(directory);(d/'stream.txt').write_text(complete(),encoding='utf-8')
            (d/'producer.py').write_text('import pathlib,sys\nprint(pathlib.Path(sys.argv[1]).read_text(),end="")\nsys.exit(int(sys.argv[2]))\n')
            q=lambda s: str(s).replace('\\','/')
            (d/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.10)\nproject(SpawnV2Gate NONE)\nenable_testing()\n'+''.join('add_test(NAME '+name+' COMMAND "'+q(sys.executable)+'" "'+q(HERE/'shell_spawn_v2_gate.py')+'" -- "'+q(sys.executable)+'" "'+q(d/'producer.py')+'" "'+q(d/'stream.txt')+'" '+str(code)+')\n' for name,code in [('zero',0),('nonzero',1)]))
            subprocess.run(['cmake','-S',str(d),'-B',str(d/'build')],check=True,capture_output=True)
            p=subprocess.run(['ctest','--test-dir',str(d/'build'),'-C','Release','--output-on-failure'],capture_output=True,text=True)
            self.assertNotEqual(p.returncode,0);self.assertRegex(p.stdout,r'zero\s+\.+\s+Passed');self.assertRegex(p.stdout,r'nonzero\s+\.+\*\*\*Failed')


if __name__ == '__main__':
    unittest.main()
