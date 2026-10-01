"""Offline mocks/parser tests only; no host discovery or workflow execution."""
import json
import os
from pathlib import Path
import sys
import unittest
from unittest.mock import patch,MagicMock
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools/qemu_closure'))
import runner_vm_discovery as m

class Tests(unittest.TestCase):
    def test_observe_mock_coverage(self):
        r=MagicMock();r.call.side_effect=lambda fn,*a,**k: MagicMock(st_mode=0o100600) if fn is os.stat else MagicMock(st_mode=0o40700) if fn is os.fstat else 123
        r.directory.return_value=9;r.open.return_value=10
        values={'cgroup':'0::/owned\n','mountinfo':'1 2 0:1 / /sys/fs/cgroup rw - cgroup2 cgroup rw',
                'status':'Uid: 123 123 123 123\nGid: 456 456 456 456\nCapEff: 0000\nNoNewPrivs: 1\nSeccomp: 2',
                'cgroup.type':'domain','cgroup.controllers':'cpu memory pids','cgroup.subtree_control':'cpu memory pids'}
        r.text.side_effect=lambda fd,name: values[name]
        result=m.observe(r,{'RUNNER_TEMP':'/owned'})
        self.assertEqual(result['scope']['coverage'],'prioritized_not_complete')
        self.assertEqual(result['scope']['other_delegations'],'not_excluded')
        self.assertEqual(len(result['scope']['interfaces']),6)
        self.assertNotIn('123',json.dumps(result));self.assertNotIn('/owned',json.dumps(result))
        self.assertTrue(all(g['status']=='not_proven' for g in result['gates']))
        self.assertNotIn('cgroup.procs',[c.args[1] for c in r.text.call_args_list])
    def test_all_gates_closed(self):
        for reason in m.REASONS:
            r=m.base(reason)
            self.assertTrue(all(g['status']=='not_proven' for g in r['gates']))
            self.assertFalse(r['isolation_accredited']);self.assertFalse(r['boot_attempted'])
    def test_membership(self):
        self.assertEqual(m.membership('0::/owned\n'),'/sys/fs/cgroup/owned')
        self.assertEqual(m.membership('0::/\n'),'/sys/fs/cgroup')
        for raw in ('0::/../PRIVATE','1::/PRIVATE','0::/a\n0::/b'):
            with self.assertRaises(m.Stop):m.membership(raw)
    def test_closed_parsers(self):
        self.assertEqual(m.value('cgroup.type','domain'),'domain_candidate')
        self.assertEqual(m.value('cgroup.controllers','cpu memory pids'),'required_present')
        self.assertEqual(m.value('memory.max','PRIVATE'),'unknown')
        self.assertTrue(m.mount('1 2 0:1 / /sys/fs/cgroup rw - cgroup2 cgroup rw'))
        self.assertFalse(m.mount('1 2 0:1 / /PRIVATE rw - cgroup2 cgroup rw'))
    def test_own_status_no_disclosure(self):
        r=m.own_status('Uid:\t123 123 123 123\nGid:\t456 456 456 456\nCapEff:\t0000\nNoNewPrivs:\t1\nSeccomp:\t2\nName:\tPRIVATE\n')
        self.assertEqual(r['identity_relation'],'ids_equal');self.assertNotIn('123',json.dumps(r));self.assertNotIn('PRIVATE',json.dumps(r))
        with self.assertRaises(m.Stop):m.own_status('Uid: 1\nUid: 2')
    def test_operation_bound_reserves_closes(self):
        r=m.Reader();r.ops=62;r.fds=[10]
        self.assertEqual(r.call(lambda:1),1)
        with self.assertRaises(m.Stop):r.call(lambda:1)
        with patch.object(m.os,'close') as close:r.finish();close.assert_called_once_with(10)
        self.assertEqual(r.ops,64)
    def test_file_bound_and_nofollow(self):
        r=m.Reader()
        with patch.object(m.os,'open',return_value=10) as op,patch.object(m.os,'fstat') as st,patch.object(m.os,'read',return_value=b'x'*4097),patch.object(m.os,'close'):
            st.return_value.st_mode=0o100600
            with self.assertRaises(m.Stop):r.text(9,'status')
            self.assertTrue(op.call_args.args[1]&os.O_NOFOLLOW)
            self.assertFalse(r.fds);self.assertEqual(r.bytes,4097)
    def test_no_task_contents(self):
        r=m.Reader()
        for name in ('cgroup.procs','cgroup.threads','PRIVATE'):
            with self.assertRaises(m.Stop):r.text(9,name)
    def test_no_mutation_surface(self):
        source=Path(m.__file__).read_text()
        for term in ('os.O_WRONLY','os.O_RDWR','os.O_CREAT','os.write(','subprocess','quotactl','os.listdir','os.scandir','os.mkdir','os.unlink'):
            self.assertNotIn(term,source)
    def test_open_reserves_its_close(self):
        r=m.Reader();r.ops=63
        with patch.object(m.os,'open') as op:
            with self.assertRaises(m.Stop):r.open('/never',os.O_RDONLY)
            op.assert_not_called()
    def test_total_read_bound(self):
        r=m.Reader();r.bytes=32768
        with patch.object(m.os,'open',return_value=10),patch.object(m.os,'fstat') as st,patch.object(m.os,'read') as read,patch.object(m.os,'close'):
            st.return_value.st_mode=0o100600
            with self.assertRaises(m.Stop):r.text(9,'status')
            read.assert_not_called()
    def test_unsafe_paths(self):
        r=m.Reader()
        for p in ('relative','/a/../b','/a//b','/a/./b'):
            with self.assertRaises(m.Stop):r.directory(p)

if __name__=='__main__':unittest.main()
