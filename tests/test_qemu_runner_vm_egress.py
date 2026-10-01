"""Offline mocks only. Never bind, connect, fork, or execute native helpers."""
import contextlib
import io
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import MagicMock,patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools/qemu_closure'))
import runner_vm_egress as m

class Tests(unittest.TestCase):
    def test_exact_schemas(self):
        for expected in (m.CHILD,m.CONTROL):
            self.assertTrue(m.exact(json.dumps(expected).encode(),expected))
            for k in expected:
                value=dict(expected);value[k]='PRIVATE';self.assertFalse(m.exact(json.dumps(value),expected))
            self.assertFalse(m.exact(b'{"x":1,"x":2}',expected))
            self.assertFalse(m.exact(b'PRIVATE',expected))
    def test_no_identity_no_work(self):
        with patch.object(m,'identity',return_value=False),patch.object(m,'experiment') as ex,contextlib.redirect_stdout(io.StringIO()) as out:
            self.assertEqual(m.main(['a','b']),2);ex.assert_not_called()
            self.assertEqual(json.loads(out.getvalue())['reason'],'identity')
    def test_report_gate_types(self):
        for reason in (*m.REASONS,'PRIVATE'):
            r=m.report(reason)
            self.assertEqual(r['classification'],'blocked');self.assertNotIn('PRIVATE',json.dumps(r))
            self.assertTrue(all(g['status']=='not_proven' for g in r['gates']))
            self.assertIs(r['isolation_accredited'],False)
            self.assertEqual(len(r['classes']),11)
    def test_receipt_and_bounds(self):
        f=object.__new__(m.Fixtures);f.bytes=0;f.negative=False;f.receipts={}
        for tag,data,key in (('tcp4',m.MARK,'tcp4'),('udp6',m.DNS,'dns_udp6'),('tcp6',m.FRAMED,'dns_tcp6')):
            f.receipt(tag,data);self.assertEqual(f.receipts[key],1)
        with self.assertRaises(m.Refusal):f.receipt('udp4',b'PRIVATE')
        f.negative=True
        with self.assertRaises(m.Refusal):f.receipt('udp4',m.MARK)
        f.negative=False;f.bytes=512
        with self.assertRaises(m.Refusal):f.receipt('udp4',m.MARK)
    def test_baseline_requires_all_receipts(self):
        f=object.__new__(m.Fixtures);f.accepts=4;f.receipts={k:1 for k in m.CLASSES if k!='process_samples'}
        f.receipts['inherited_tcp']=2;f.receipts['inherited_udp']=2
        self.assertTrue(f.baseline_complete());del f.receipts['tcp6'];self.assertFalse(f.baseline_complete())
    def child_case(self,pieces,expected=m.CONTROL,rc=0,empty_events=False):
        f=MagicMock();fds=[MagicMock(),MagicMock()];fds[0].fileno.return_value=10;fds[1].fileno.return_value=11
        f.prepare_inherited.return_value=fds;child=MagicMock();child.pid=123;child.poll.return_value=rc;child.wait.return_value=rc
        key=MagicMock();key.data=('output','',b'')
        f.poll.select.return_value=[] if empty_events else [(key,1)]
        with patch.object(m.subprocess,'Popen',return_value=child) as spawn,patch.object(m.os,'pidfd_open',side_effect=OSError()),patch.object(m.os,'read',side_effect=pieces):
            try:r=m.run_child(Path('/never-run'),f,expected,False,m.time.monotonic()+15)
            except m.Refusal as ex:r=ex.reason
            self.assertEqual(spawn.call_args.kwargs['env'],{})
            self.assertEqual(spawn.call_args.kwargs['pass_fds'],(10,11));self.assertTrue(spawn.call_args.kwargs['close_fds'])
            self.assertNotIn('shell',spawn.call_args.kwargs)
            for fd in fds:fd.close.assert_called_once()
        return r,child,f
    def test_child_success(self):
        r,c,f=self.child_case([json.dumps(m.CONTROL).encode(),b'']);self.assertIs(r,True);self.assertTrue(f.last_reaped)
    def test_child_output_failure(self):
        r,c,f=self.child_case([b'PRIVATE',b'']);self.assertEqual(r,'child_schema');self.assertTrue(f.last_reaped)
    def test_child_overflow(self):
        r,c,f=self.child_case([b'x'*1025]);self.assertEqual(r,'output_bounds');self.assertTrue(f.last_reaped)
    def test_timeout_kill_reap(self):
        r,c,f=self.child_case([],rc=None,empty_events=True);self.assertEqual(r,'timeout');c.kill.assert_called_once();self.assertTrue(f.last_reaped)
    def test_spawn_failure_closes_passed(self):
        f=MagicMock();a=MagicMock();b=MagicMock();f.prepare_inherited.return_value=[a,b]
        with patch.object(m.subprocess,'Popen',side_effect=OSError('PRIVATE')):
            with self.assertRaises(OSError):m.run_child(Path('/never-run'),f,m.CONTROL,False,m.time.monotonic()+15)
        a.close.assert_called_once();b.close.assert_called_once()
    def test_owned_cleanup(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);(root/m.SCRATCH).mkdir()
            for name in m.BINARIES:(root/name).write_text('trusted')
            self.assertTrue(m.remove_owned(root));self.assertTrue(m.remove_owned(root))
    def test_no_follow_and_unknown_entry(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);(root/m.SCRATCH).mkdir();(root/m.SCRATCH/'unknown').write_text('x')
            self.assertFalse(m.remove_owned(root));(root/m.SCRATCH/'unknown').unlink();(root/m.SCRATCH).rmdir()
            outside=root/'outside';outside.write_text('PRIVATE');(root/m.BINARIES[0]).symlink_to(outside)
            self.assertFalse(m.remove_owned(root));self.assertEqual(outside.read_text(),'PRIVATE')
    def test_finalizer_missing_evidence(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);self.assertEqual(m.finalizer(root),0)
            r=json.loads((root/m.CLEANUP).read_text());self.assertEqual(r['direct_children'],'not_proven');self.assertEqual(r['fixture_descriptors'],'not_proven')
            self.assertEqual(r['owned_paths'],'confirmed_owned_direct')
    def test_report_file_nofollow(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);outside=root/'outside';outside.write_text('PRIVATE');dest=root/'report';dest.symlink_to(outside)
            with self.assertRaises(OSError):m.write_report(dest,m.clean_report())
            self.assertEqual(outside.read_text(),'PRIVATE')
    def test_setup_failure_reports_no_false_cleanup(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d)
            for name in m.BINARIES:(root/name).write_text('trusted')
            with patch.object(m,'Fixtures',side_effect=m.Refusal('fixture_setup')):
                self.assertEqual(m.experiment(root)['reason'],'fixture_setup')
            r=json.loads((root/m.SUPERVISOR).read_text());self.assertEqual(r['direct_children'],'not_proven')
    def test_delete_failure(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);(root/m.SCRATCH).mkdir()
            with patch.object(Path,'rmdir',side_effect=OSError('PRIVATE')):self.assertFalse(m.remove_owned(root))


# Run with unittest discovery as well as the explicit invocation below.
class PolicyTests(unittest.TestCase):
    def test_filter_block_unchanged(self):
        root=Path(__file__).resolve().parents[1]/'tools/qemu_closure'
        old=(root/'runner_vm_harness.c').read_text();new=(root/'runner_vm_egress_child.c').read_text()
        start='    struct sock_filter code[] = {';end='    if (syscall(SYS_getpid) <= 0) fail();'
        self.assertEqual(old[old.index(start):old.index(end)],new[new.index(start):new.index(end)])
        for s in ('bound(RLIMIT_AS, 64U*1024U*1024U);','bound(RLIMIT_CPU, 1);','bound(RLIMIT_FSIZE, 1024);','bound(RLIMIT_NOFILE, 16);','bound(RLIMIT_CORE, 0);'):
            self.assertIn(s,new)
        self.assertLess(new.index('fcntl(value[i],F_GETFD)'),new.index('struct sock_filter'))
    def test_no_resolver_or_public_baseline(self):
        root=Path(__file__).resolve().parents[1]/'tools/qemu_closure'
        c=(root/'runner_vm_egress_control.c').read_text()
        self.assertIn('INADDR_LOOPBACK',c);self.assertIn('IN6ADDR_LOOPBACK_INIT',c)
        for word in ('getaddrinfo','gethostbyname','system(','popen(','INADDR_ANY'):
            self.assertNotIn(word,c)
    def test_reap_failure_closed(self):
        f=MagicMock();fd=MagicMock();fd.fileno.return_value=10
        f.prepare_inherited.return_value=[fd];f.poll.select.return_value=[]
        child=MagicMock();child.poll.return_value=None;child.wait.side_effect=m.subprocess.TimeoutExpired('fixed',2)
        with patch.object(m.subprocess,'Popen',return_value=child),patch.object(m.os,'pidfd_open',side_effect=OSError()):
            with self.assertRaises(m.Refusal) as e:m.run_child(Path('/never-run'),f,m.CONTROL,False,m.time.monotonic()+15)
        self.assertEqual(e.exception.reason,'cleanup_refusal');self.assertFalse(f.last_reaped)
    def test_partial_tcp_and_eof(self):
        f=object.__new__(m.Fixtures);f.bytes=0;f.negative=False;f.receipts={};f.poll=MagicMock()
        sock=MagicMock();sock.recv.return_value=b'LO';sock.send.return_value=1
        f.service(sock,('stream','tcp4',b''));self.assertFalse(f.receipts)
        sock.recv.return_value=b'CL';f.service(sock,('stream','tcp4',b'LO'))
        self.assertEqual(f.receipts,{'tcp4':1})
        sock.recv.return_value=b''
        with self.assertRaises(m.Refusal):f.service(sock,('stream','tcp4',b'L'))
    def test_unknown_address_refuses(self):
        f=object.__new__(m.Fixtures);f.bytes=0;f.negative=False;f.receipts={};sock=MagicMock()
        sock.recvfrom.return_value=(m.MARK,('192.0.2.1',99))
        with self.assertRaises(m.Refusal):f.service(sock,('udp','udp4',b''))
    def test_finalizer_rejects_duplicate_evidence(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);(root/m.SUPERVISOR).write_text('{"schema":"PRIVATE","schema":"PRIVATE"}')
            self.assertEqual(m.finalizer(root),0)
            r=json.loads((root/m.CLEANUP).read_text());self.assertEqual(r['direct_children'],'not_proven')

if __name__=='__main__':unittest.main()
