"""Offline parser and refusal tests; never launch namespace operations."""
import sys
from pathlib import Path
import unittest
from unittest.mock import patch
import subprocess
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'qemu_closure'))
from runner_b_namespace_diag import errno_class, classify_process, validate_child, diagnose, run, child


class RunnerBNamespaceDiagnosticTests(unittest.TestCase):
    def test_errno_closed_vocabulary(self):
        for text, expected in (
            ('unshare: Operation not permitted', 'eperm'),
            ('mount: Permission denied', 'eacces'),
            ('Invalid argument', 'einval'),
            ('No space left on device', 'enospc'),
            ('Resource temporarily unavailable', 'eagain'),
            ('Out of memory', 'enomem'),
            ('No such file or directory', 'enoent'),
            ('Not supported', 'unsupported'),
            ('arbitrary secret /home/runner/foo', 'other'),
        ):
            with self.subTest(text=text): self.assertEqual(errno_class(text), expected)

    def test_phase_errors_do_not_echo_stderr(self):
        self.assertEqual(classify_process('user_only', (1, '', 'unshare: Operation not permitted /private', False)),
                         {'phase': 'user_only', 'status': 'failed', 'errno_class': 'eperm'})
        self.assertEqual(classify_process('user_only', (-1, '', '', False))['errno_class'], 'timeout')
        self.assertEqual(classify_process('user_only', (0, '', '', False))['status'], 'ok')
        self.assertEqual(classify_process('user_only', (0, 'bad', '', False))['status'], 'failed')

    def test_child_closed_schema(self):
        ok = '{"phase":"proc","status":"ok"}'
        self.assertEqual(validate_child((0, ok, '', False))['status'], 'ok')
        self.assertEqual(validate_child((2, '{"phase":"tmpfs","status":"failed","errno_class":"eperm"}', '', False))['phase'], 'tmpfs')
        for rc, body in ((0, '{"phase":"proc","status":"ok","secret":1}'),
                         (0, '{"phase":"proc","status":"failed","errno_class":"eperm"}'),
                         (2, '{"phase":"proc","status":"ok"}'),
                         (2, '{"phase":"tmpfs","status":"failed","errno_class":"SECRET"}'),
                         (0, 'not json')):
            with self.subTest(body=body): self.assertEqual(validate_child((rc, body, '', False))['phase'], 'full')
        self.assertEqual(validate_child((-1, '', '', False))['errno_class'], 'timeout')
        self.assertEqual(validate_child((0, ok, '', True))['phase'], 'full')

    def test_timeout_kills_process_group_and_reaps(self):
        class Timed:
            pid = 1234
            def __init__(self): self.calls = 0
            def __enter__(self): return self
            def __exit__(self, *a): return False
            def communicate(self, timeout):
                self.calls += 1
                if self.calls == 1: raise subprocess.TimeoutExpired('unshare', timeout)
                return '', ''
        p = Timed()
        with patch('runner_b_namespace_diag.subprocess.Popen', return_value=p) as popen, \
             patch('runner_b_namespace_diag.os.killpg') as killpg:
            self.assertEqual(run(('/usr/bin/unshare',)), (-1, '', '', False))
            self.assertTrue(popen.call_args.kwargs['start_new_session'])
            killpg.assert_called_once()
            self.assertEqual(p.calls, 2)

    def test_child_cleanup_after_successful_staging(self):
        from tempfile import TemporaryDirectory
        def fake_run(cmd):
            if cmd[0] == '/usr/bin/mount' and cmd[1] == '-t' and cmd[2] == 'tmpfs':
                # Stand in for the tmpfs boundary; child creates proc under it.
                return (0, '', '', False)
            return (0, '', '', False)
        with TemporaryDirectory() as tmp, patch('runner_b_namespace_diag.run', side_effect=fake_run):
            root = Path(tmp) / 'synthetic'
            self.assertEqual(child(root), {'phase': 'proc', 'status': 'ok'})
            self.assertFalse(root.exists())

    def test_no_live_environment_refuses(self):
        r = diagnose('0' * 40, '20260920.314.1')
        self.assertEqual(r['reason'], 'identity')
        self.assertEqual(r['phases'], [])
        self.assertFalse(r['boot_attempted'])
        self.assertFalse(r['runtime_complete'])
        self.assertFalse(r['isolation_accredited'])


if __name__ == '__main__': unittest.main()
