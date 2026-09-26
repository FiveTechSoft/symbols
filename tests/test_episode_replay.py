#!/usr/bin/env python3
"""Contract tests, not task-performance evidence."""
import hashlib
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import episode_replay as replay


class ReplayContractTest(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.root = Path(tmp.name)

    def test_private_bounded_snapshot_and_digest(self):
        self.root.joinpath('a.c').write_bytes(b'int x;\n')
        digest, files = replay.tree_digest(self.root)
        self.assertEqual(files['a.c'], hashlib.sha256(b'int x;\n').hexdigest())
        self.assertEqual(digest, replay.tree_digest(self.root)[0])
        self.root.joinpath('alias').symlink_to('a.c')
        with self.assertRaises(replay.Unavailable):
            replay.tree_digest(self.root)
        self.root.joinpath('alias').unlink()
        self.root.joinpath('large.c').write_bytes(b'a' * (replay.MAX_FILE + 1))
        with self.assertRaises(replay.Unavailable):
            replay.tree_digest(self.root)

    def test_sealed_digest_changes_on_oracle_and_preflight_refuses_setup(self):
        case = self.root / 'case'
        (case / 'before').mkdir(parents=True)
        (case / 'before' / 'main.c').write_bytes(b'int main(void){return 1;}\n')
        (case / 'task.md').write_text('Fix this')
        (case / 'check.py').write_text('raise SystemExit(1)')
        (self.root / 'index.tsv').write_text('id\tcategory\tpath\ncase1\tfix\tcase\n')
        old = replay.sealed_digest(self.root)
        self.assertEqual(len(replay.collect(self.root, self.root / 'index.tsv')), 1)
        (case / 'check.py').write_text('raise SystemExit(0)')
        self.assertNotEqual(old, replay.sealed_digest(self.root))
        (case / 'setup.py').write_text('print(1)')
        with self.assertRaises(replay.Unavailable):
            replay.collect(self.root, self.root / 'index.tsv')

    def test_changes_and_prior_only_veto(self):
        self.assertEqual(replay.source_changes({'a': 'old'}, {'a': 'new'}), ['a'])
        self.assertEqual(replay.source_changes({'a': 'old'}, {'a': 'old'}), [])
        prior = ({'operator': 'rename_symbol', 'wrong_edit': True},)
        self.assertTrue(replay.should_veto(prior, 'rename_symbol', True))
        self.assertFalse(replay.should_veto(prior, 'rename_symbol', False))
        self.assertFalse(replay.should_veto(prior, 'c_contract', True))
        self.assertFalse(replay.should_veto((), 'rename_symbol', True))
        # The evaluator freezes prior history before executing both target modes.
        source = Path(replay.__file__).read_text()
        self.assertIn("prior = tuple(h for h in history if h['initial_sha256'] != before_digest)", source)
        self.assertLess(source.index('prior = tuple('), source.index("for mode in ('baseline', 'replay')"))


if __name__ == '__main__':
    unittest.main()
