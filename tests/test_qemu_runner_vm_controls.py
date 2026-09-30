"""Offline controls tests. Never run live runner, network or process policy tests."""
import contextlib
import io
import json
import os
from pathlib import Path
import resource
import sys
import tempfile
import unittest
from unittest.mock import patch, mock_open
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools/qemu_closure'))
import runner_vm_controls as m


class ControlsTests(unittest.TestCase):
    def test_limits_closed(self):
        self.assertEqual(m.limit_class(resource.RLIM_INFINITY), 'unlimited')
        self.assertEqual(m.limit_class(0), 'zero')
        self.assertEqual(m.limit_class(7), 'finite')
        self.assertEqual(m.limit_class('SECRET'), 'unknown')
        with patch.object(resource, 'getrlimit', side_effect=OSError('SECRET')):
            self.assertTrue(all(r['soft'] == r['hard'] == 'unknown' for r in m.limits()))

    def test_cgroup_bounds_format_and_traversal(self):
        for raw in (b'1:cpu:/x\n', b'0::/../secret\n', b'0::/./x\n', b'x' * 4097,
                    b'0::/x\n0::/y\n', b'\xff'):
            with patch('builtins.open', mock_open(read_data=raw)):
                self.assertIsNone(m.current_cgroup())
        with patch('builtins.open', mock_open(read_data=b'0::/job\n')), \
                patch.object(m.os, 'lstat', return_value=type('S', (), {'st_mode': 0o040700})()):
            self.assertEqual(m.current_cgroup(), Path('/sys/fs/cgroup/job'))
        with patch('builtins.open', mock_open(read_data=b'0::/job\n')), \
                patch.object(m.os, 'lstat', return_value=type('S', (), {'st_mode': 0o120777})()):
            self.assertIsNone(m.current_cgroup())

    def test_cgroup_metadata_is_not_enforcement(self):
        with patch.object(m, 'current_cgroup', return_value=Path('/secret')), \
                patch.object(m, 'access_class', return_value='write_access_reported'):
            r = m.cgroup()
            self.assertNotIn('/secret', json.dumps(r))
            self.assertEqual(len(r['surfaces']), len(m.CG_FILES))
        with patch.object(m, 'current_cgroup', return_value=None):
            self.assertEqual(m.cgroup(), {'version': 'unknown', 'surfaces': []})

    def test_access_does_not_read(self):
        with tempfile.TemporaryDirectory() as root:
            p = Path(root) / 'x'; p.write_text('SECRET')
            link = Path(root) / 'link'; link.symlink_to(p)
            with patch('builtins.open', side_effect=AssertionError('no read')):
                self.assertEqual(m.access_class(link), 'symlink_unresolved')
                self.assertEqual(m.access_class(p), 'write_access_reported')
                self.assertEqual(m.access_class(Path(root)), 'unexpected_type')
                self.assertEqual(m.access_class(Path(root) / 'absent'), 'missing')

    def test_owned_scratch_only_and_cleanup(self):
        with tempfile.TemporaryDirectory() as root:
            keep = Path(root) / 'keep'; keep.write_text('SECRET')
            self.assertEqual(m.scratch(root), 'owned_marker_removed')
            self.assertEqual(list(Path(root).iterdir()), [keep])
            self.assertEqual(keep.read_text(), 'SECRET')
        with patch.object(m.tempfile, 'mkdtemp', side_effect=OSError('SECRET')):
            self.assertEqual(m.scratch('/tmp'), 'failed_or_unknown')

    def test_cleanup_failure_blocks(self):
        with tempfile.TemporaryDirectory() as root:
            with patch.object(Path, 'rmdir', side_effect=OSError('SECRET')):
                self.assertEqual(m.scratch(root), 'failed_or_unknown')
            # Remove our known empty owned directory after injected test failure.
            for p in Path(root).iterdir(): p.rmdir()

    def test_identity_refusal_no_observations(self):
        with patch.object(m, 'identity', return_value=False), patch.object(m, 'scratch') as scratch:
            self.assertEqual(m.measure('bad', 'bad'), m.base('identity'))
            scratch.assert_not_called()

    def test_measurement_never_passes_gates(self):
        with patch.object(m, 'identity', return_value=True), \
                patch.object(m, 'anchors', return_value={'runner_temp': Path('/tmp')}), \
                patch.object(m, 'limits', return_value=[]), patch.object(m, 'cgroup', return_value={'version':'unknown','surfaces':[]}), \
                patch.object(m, 'scratch', return_value='owned_marker_removed'):
            r = m.measure('x', 'y')
            self.assertEqual(r['classification'], 'blocked')
            self.assertTrue(all(g['status'] == 'not_proven' for g in r['gates']))
            for k in ('boot_attempted', 'runtime_complete', 'isolation_accredited'): self.assertIs(r[k], False)
        with contextlib.redirect_stdout(io.StringIO()) as output:
            self.assertEqual(m.main([]), 2)
            self.assertNotIn('SECRET', output.getvalue())


if __name__ == '__main__': unittest.main()
