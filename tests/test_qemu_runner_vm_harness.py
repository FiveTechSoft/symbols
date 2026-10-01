"""Offline parser/supervisor fault tests. Never launch confined helper."""
import contextlib
import io
import json
from pathlib import Path
import sys
import unittest
from unittest.mock import patch, MagicMock
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools/qemu_closure'))
import runner_vm_harness as m


class HarnessTests(unittest.TestCase):
    def test_ceiling_loop_and_filter_unchanged(self):
        root=Path(__file__).resolve().parents[1]/'tools/qemu_closure'
        c=(root/'runner_vm_harness.c').read_text()
        e=(root/'runner_vm_egress_child.c').read_text()
        start='    struct sock_filter code[] = {'
        end='    if (syscall(SYS_getpid) <= 0) fail();'
        self.assertEqual(c[c.index(start):c.index(end)],e[e.index(start):e.index(end)])
        self.assertIn('struct rlimit raise = {values[i]+1,values[i]+1}',c)
        self.assertIn('SYS_setrlimit,kinds[i],&raise',c)
        self.assertIn('errno != EPERM',c)
        self.assertIn('observed.rlim_cur!=values[i] || observed.rlim_max!=values[i]',c)
    def test_ceiling_claims_require_exact_success(self):
        for reason in m.REASONS:
            r=m.report(reason)
            self.assertEqual(r['consumption_violations'],'not_tested')
            self.assertEqual([x['limit'] for x in r['ceiling_raise_denials']],
                             ['as','cpu','fsize','nofile','core'])
            for row in r['ceiling_raise_denials']:
                self.assertEqual(row['status'],'measured_only' if
                                 reason=='capabilities_only_not_enforcement' else 'not_proven')
    def test_old_schema_not_accepted(self):
        value=dict(m.EXPECTED);value['schema']='symbols.runner-vm-harness-child.v1'
        self.assertFalse(m.validate(json.dumps(value).encode()))
    def test_exact_child_schema(self):
        self.assertTrue(m.validate(json.dumps(m.EXPECTED).encode()))
        for key in m.EXPECTED:
            value=dict(m.EXPECTED);value[key]='PRIVATE'
            self.assertFalse(m.validate(json.dumps(value).encode()))
        value=dict(m.EXPECTED);value['secret']=1
        self.assertFalse(m.validate(json.dumps(value).encode()))
        for raw in (b'bad',b'\xff',b'{"x":1,"x":2}',b'[]'):
            self.assertFalse(m.validate(raw))
        value=dict(m.EXPECTED);value['limits_readback']=1
        self.assertFalse(m.validate(json.dumps(value).encode()))

    def test_no_live_identity_refuses(self):
        with patch.object(m,'identity',return_value=False),patch.object(m,'supervise') as run, \
                contextlib.redirect_stdout(io.StringIO()) as output:
            self.assertEqual(m.main(['a','b']),2)
            run.assert_not_called()
            self.assertEqual(json.loads(output.getvalue())['reason'],'identity')

    def test_spawn_error_closed(self):
        with patch.object(m.subprocess,'Popen',side_effect=OSError('PRIVATE')):
            r=m.supervise(Path('/never-execute'))
            self.assertEqual(r['reason'],'supervisor_error')
            self.assertNotIn('PRIVATE',json.dumps(r))

    def test_timeout_kill_reap_no_shell(self):
        child=MagicMock();child.pid=123;child.poll.return_value=None
        poll=MagicMock();poll.__enter__.return_value=poll;poll.select.return_value=[]
        with patch.object(m.subprocess,'Popen',return_value=child) as spawn, \
                patch.object(m.selectors,'DefaultSelector',return_value=poll), \
                patch.object(m.os,'pidfd_open',side_effect=OSError()):
            self.assertEqual(m.supervise(Path('/never-execute'))['reason'],'timeout')
            child.kill.assert_called_once();child.wait.assert_called_once_with(timeout=2)
            self.assertEqual(spawn.call_args.kwargs['env'],{})
            self.assertTrue(spawn.call_args.kwargs['close_fds'])
            self.assertNotIn('shell',spawn.call_args.kwargs)

    def test_output_bound_cleanup(self):
        child=MagicMock();child.pid=123;child.poll.return_value=None
        poll=MagicMock();poll.__enter__.return_value=poll;poll.select.return_value=[1]
        with patch.object(m.subprocess,'Popen',return_value=child), \
                patch.object(m.selectors,'DefaultSelector',return_value=poll), \
                patch.object(m.os,'pidfd_open',side_effect=OSError()), \
                patch.object(m.os,'read',return_value=b'x'*1025):
            self.assertEqual(m.supervise(Path('/never-execute'))['reason'],'output_bounds')
            child.kill.assert_called_once()

    def test_child_success_and_invalid_output(self):
        for body,expected in ((json.dumps(m.EXPECTED).encode(),'capabilities_only_not_enforcement'),
                              (b'PRIVATE','child_schema')):
            child=MagicMock();child.pid=123;child.poll.return_value=0;child.wait.return_value=0
            poll=MagicMock();poll.__enter__.return_value=poll;poll.select.return_value=[1]
            with patch.object(m.subprocess,'Popen',return_value=child), \
                    patch.object(m.selectors,'DefaultSelector',return_value=poll), \
                    patch.object(m.os,'pidfd_open',side_effect=OSError()), \
                    patch.object(m.os,'read',side_effect=[body,b'']):
                self.assertEqual(m.supervise(Path('/never-execute'))['reason'],expected)
                child.kill.assert_not_called()

    def test_all_reports_not_proven(self):
        for reason in (*m.REASONS,'PRIVATE'):
            r=m.report(reason)
            self.assertEqual(r['classification'],'blocked')
            for key in ('boot_attempted','runtime_complete','isolation_accredited'):self.assertIs(r[key],False)
            self.assertTrue(all(g['status']=='not_proven' for g in r['gates']))
            self.assertNotIn('PRIVATE',json.dumps(r))


if __name__=='__main__': unittest.main()
