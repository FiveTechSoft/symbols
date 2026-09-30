"""Offline hosted-runner capability policy vectors; no VM or namespace mutation."""
import sys
from pathlib import Path
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'/'qemu_closure'))
from runner_b_preflight import NAMESPACES,verify

class RunnerBPreflightTests(unittest.TestCase):
    def fixtures(self):
        host={'uid':1001,'euid':1001,'runner_image':'20260920.314.1',
              'expected_runner_image':'20260920.314.1',
              'namespace_ids':{x:f'{x}:[10]' for x in NAMESPACES}}
        child={'euid':0,'pid':1,'mount_propagation_private':True,'staged_proc':True,
               'staged_empty_sys':True,'namespace_ids':{x:f'{x}:[20]' for x in NAMESPACES}}
        return host,child

    def test_capability_is_not_isolation(self):
        host,child=self.fixtures();r=verify(host,child)
        self.assertEqual(r['classification'],'measured_only')
        self.assertIs(r['runtime_complete'],False)
        self.assertIs(r['isolation_accredited'],False)
        self.assertEqual(len(r['namespaces_distinct']),6)

    def test_refusals(self):
        for mutation,where,reason in (
            (('euid',0),'host','host_root'),
            (('runner_image','20260921.314.1'),'host','runner_image'),
            (('pid',2),'child','namespace_effects'),(('staged_proc',False),'child','namespace_effects'),
            (('staged_empty_sys',False),'child','namespace_effects'),
            (('mount_propagation_private',False),'child','namespace_effects')):
            with self.subTest(mutation=mutation):
                host,child=self.fixtures();(host if where=='host' else child)[mutation[0]]=mutation[1]
                r=verify(host,child);self.assertEqual(r['reason'],reason)
                self.assertFalse(r['runtime_complete']);self.assertFalse(r['isolation_accredited'])
        for name in NAMESPACES:
            host,child=self.fixtures();child['namespace_ids'][name]=host['namespace_ids'][name]
            self.assertEqual(verify(host,child)['reason'],'namespace_'+name)

if __name__=='__main__':unittest.main()
