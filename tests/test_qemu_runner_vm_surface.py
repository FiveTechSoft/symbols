"""Offline surface policy tests. No live runner observation or secret reads."""
import contextlib
import io
import json
import os
from pathlib import Path
import stat
import sys
import tempfile
import unittest
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'qemu_closure'))
import runner_vm_surface as m

SHA = 'a' * 40
IMAGE = '20260920.314.1'


class RunnerVMSurfaceTests(unittest.TestCase):
    def env(self, root):
        return {'GITHUB_SHA': SHA, 'ImageVersion': IMAGE, 'GITHUB_REF': 'refs/heads/master',
                'RUNNER_ENVIRONMENT': 'github-hosted', 'RUNNER_OS': 'Linux', 'RUNNER_ARCH': 'X64',
                'SYMBOLS_AUTOMATIC_TOKEN_PRESENT': 'true', 'HOME': root,
                'GITHUB_WORKSPACE': root, 'RUNNER_TEMP': root, 'GITHUB_TOKEN': 'PRIVATE_TOKEN'}

    def assert_blocked(self, result):
        self.assertEqual(result['classification'], 'blocked')
        for key in ('boot_attempted', 'runtime_complete', 'isolation_accredited'):
            self.assertIs(result[key], False)
        self.assertTrue(all(g['status'] == 'not_proven' for g in result['gates']))
        self.assertNotIn('PRIVATE_TOKEN', json.dumps(result))

    def test_identity_exact_fields_and_nonroot(self):
        env = self.env('/tmp')
        with patch.object(m.os, 'getuid', return_value=1001), patch.object(m.os, 'geteuid', return_value=1001), \
                patch.object(m.sys, 'platform', 'linux'):
            self.assertTrue(m.identity(env, SHA, IMAGE))
            for key in ('GITHUB_SHA', 'GITHUB_REF', 'RUNNER_ENVIRONMENT', 'RUNNER_OS', 'RUNNER_ARCH', 'ImageVersion'):
                changed = dict(env); changed[key] = 'other'
                self.assertFalse(m.identity(changed, SHA, IMAGE))
            self.assertFalse(m.identity(env, 'bad', IMAGE))
            self.assertFalse(m.identity(env, SHA, 'bad'))
        with patch.object(m.os, 'getuid', return_value=0):
            self.assertFalse(m.identity(env, SHA, IMAGE))

    def test_metadata_no_value_output_and_no_gate_pass(self):
        with tempfile.TemporaryDirectory() as root, patch.dict(os.environ, self.env(root), clear=True), \
                patch.object(m, 'identity', return_value=True), patch.object(m, 'fd_inventory', return_value=[{'class': 'stdio'}]):
            result = m.measure(SHA, IMAGE)
            self.assert_blocked(result)
            self.assertEqual(result['reason'], 'metadata_only_not_reachability_proof')
            self.assertEqual(result['credentials']['automatic_token_context'], 'present')
            self.assertNotIn(root, json.dumps(result))
            self.assertEqual(len(result['host_content']), len(m.PATH_KEYS))

    def test_bad_context_and_environment_bounds(self):
        with patch.object(m, 'identity', return_value=True):
            for context in ('PRIVATE_TOKEN', '', 'True'):
                with patch.dict(os.environ, {'SYMBOLS_AUTOMATIC_TOKEN_PRESENT': context}, clear=True):
                    result = m.measure(SHA, IMAGE)
                    self.assert_blocked(result)
                    self.assertEqual(result['reason'], 'token_context')
            with patch.dict(os.environ, {str(i): 'PRIVATE_TOKEN' for i in range(513)}, clear=True):
                self.assertEqual(m.measure(SHA, IMAGE)['reason'], 'environment_bounds')

    def test_identity_failure_no_measurements(self):
        with patch.object(m, 'identity', return_value=False), patch.object(m, 'anchors') as anchors:
            result = m.measure(SHA, IMAGE)
            self.assert_blocked(result)
            self.assertEqual(result['identity'], 'not_measured')
            anchors.assert_not_called()

    def test_path_metadata_never_opens_or_follows_symlink(self):
        with tempfile.TemporaryDirectory() as root:
            root = Path(root)
            file = root / 'credentials'; file.write_text('PRIVATE_TOKEN')
            link = root / 'link'; link.symlink_to(file)
            with patch('builtins.open', side_effect=AssertionError('no content reads')), \
                    patch.object(Path, 'open', side_effect=AssertionError('no content reads')):
                self.assertEqual(m.path_state(link), 'symlink_unresolved')
                self.assertEqual(m.path_state(root), 'directory_searchable')
                self.assertEqual(m.path_state(file), 'regular_readable')
                self.assertEqual(m.path_state(root / 'missing'), 'missing')
            with patch.object(m.os, 'lstat', side_effect=PermissionError('PRIVATE_TOKEN')):
                self.assertEqual(m.path_state(file), 'unknown')

    def test_anchor_symlink_and_invalid_refusal(self):
        with tempfile.TemporaryDirectory() as root:
            link = Path(root) / 'link'; link.symlink_to(root)
            for name in ('HOME', 'GITHUB_WORKSPACE', 'RUNNER_TEMP'):
                for bad in (str(link), 'relative', '', '/tmp/\x00'):
                    env = self.env(root); env[name] = bad
                    self.assertIsNone(m.anchors(env))

    def test_fd_types_bounds_and_races_no_target_read(self):
        modes = (stat.S_IFREG, stat.S_IFDIR, stat.S_IFSOCK, stat.S_IFIFO, stat.S_IFCHR, 0)
        with patch.object(m.os, 'listdir', return_value=[str(i) for i in range(3, 9)]), \
                patch.object(m.os, 'fstat', side_effect=[type('S', (), {'st_mode': mode})() for mode in modes]), \
                patch.object(m.os, 'readlink', side_effect=AssertionError('no targets')):
            self.assertEqual([r['class'] for r in m.fd_inventory()], ['regular', 'directory', 'socket', 'pipe', 'device', 'other'])
        with patch.object(m.os, 'listdir', return_value=['3']), patch.object(m.os, 'fstat', side_effect=OSError()):
            self.assertEqual(m.fd_inventory(), [{'class': 'unstable'}])
        for names in (['x'], list(map(str, range(65)))):
            with patch.object(m.os, 'listdir', return_value=names):
                self.assertIsNone(m.fd_inventory())

    def test_cli_errors_closed_and_exit_two(self):
        for args in ([], ['one'], ['one', 'two', 'three']):
            with contextlib.redirect_stdout(io.StringIO()) as output:
                self.assertEqual(m.main(args), 2)
                self.assert_blocked(json.loads(output.getvalue()))
        with patch.object(m, 'measure', side_effect=OSError('PRIVATE_TOKEN')), \
                contextlib.redirect_stdout(io.StringIO()) as output:
            self.assertEqual(m.main([SHA, IMAGE]), 2)
            self.assert_blocked(json.loads(output.getvalue()))


if __name__ == '__main__':
    unittest.main()
