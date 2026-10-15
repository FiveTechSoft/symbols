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

    def test_v2_multi_attempt_identity_and_damage(self):
        import os
        import shutil
        import subprocess
        report = os.environ.get('ATTEMPT_CAPTURE_REPORT_BIN')
        agent = os.environ.get('SYMBOLS_AGENT_BIN')
        if not report or not agent:
            self.skipTest('integration executables not configured')
        src = self.root / 'initial'
        src.mkdir()
        (src / 'main.c').write_text('int old_name(void){return 1;} int main(void){return old_name()-1;}\n')
        task = 'Rename old_name to new_name.'
        # A real task_ops capture; perturbation tests below use only an isolated
        # copy of the captured tree, never a sealed evaluation task.
        branch = self.root / 'work'
        shutil.copytree(src, branch)
        cap = self.root / 'private'
        cap.mkdir(mode=0o700)
        env = os.environ.copy()
        env.update(SYMBOLS_ENGINEERING_EPISODES='1', SYMBOLS_ATTEMPT_CAPTURE=str(cap),
                   SYMBOLS_REFLEXION='1', SYMBOLS_TASK_OPS_MEMORY='0')
        p = subprocess.run([agent, '-w', str(branch), task], cwd=branch, env=env,
                           capture_output=True, text=True, timeout=60)
        self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
        rows = (branch / '.symbols' / 'engineering_episodes.v1').read_text().splitlines()
        self.assertEqual(len(rows), 3)  # header, record, checksum
        nodes = replay.capture_v2(cap, Path(report),
                                  branch / '.symbols' / 'engineering_episodes.v1',src,branch)
        self.assertEqual(len(nodes), 1)
        self.assertEqual(nodes[0]['outcome'], 'verified')
        run = next(cap.iterdir())
        marker = run / '001' / 'manifest.v2'
        original = marker.read_bytes()
        marker.write_bytes(original.replace(b'outcome\tverified',b'outcome\tabstained'))
        with self.assertRaises(replay.Unavailable):
            replay.capture_v2(cap, Path(report),
                              branch / '.symbols' / 'engineering_episodes.v1',src,branch)
        marker.write_bytes(original)
        before=run/'001'/'before'/'main.c'
        before.write_bytes(before.read_bytes()+b'\n')
        with self.assertRaises(replay.Unavailable):
            replay.capture_v2(cap, Path(report),
                              branch / '.symbols' / 'engineering_episodes.v1',src,branch)

    def test_v2_two_attempt_chain(self):
        import os
        import shutil
        import subprocess
        agent = os.environ.get('SYMBOLS_AGENT_BIN')
        report = os.environ.get('ATTEMPT_CAPTURE_REPORT_BIN')
        if not agent or not report:
            self.skipTest('integration executables not configured')
        src = self.root / 'source'
        src.mkdir()
        (src / 'util.c').write_text('int max_of(const int *a, int n) { int m = a[0]; for (int i = 1; i < n; i++) if (a[i] < m) m = a[i]; return m; }\n')
        (src / 'other.c').write_text('int bonus(int k) { return k < 3 ? 8 : 0; }\n')
        (src / 'main.c').write_text('#include <stdio.h>\nint max_of(const int *a, int n);\nint bonus(int k);\n'
          'int main(void) { int a[] = {3, 9, 1, 4}; printf("%d\\n", max_of(a, 4) + bonus(3)); return 0; }\n')
        branch=self.root / 'work'
        shutil.copytree(src,branch)
        cap=self.root / 'capture'
        cap.mkdir(mode=0o700)
        env=os.environ.copy()
        env.update(SYMBOLS_ENGINEERING_EPISODES='1',SYMBOLS_ATTEMPT_CAPTURE=str(cap),
                   SYMBOLS_REFLEXION='1',SYMBOLS_TASK_OPS_MEMORY='0')
        task='The bug is in util.c: the program must print 9.'
        p=subprocess.run([agent,'-w',str(branch),task],cwd=branch,env=env,
                         capture_output=True,text=True,timeout=60)
        self.assertEqual(p.returncode,0,p.stdout+p.stderr)
        nodes=replay.capture_v2(cap,Path(report),
              branch/'.symbols'/'engineering_episodes.v1',src,branch)
        self.assertEqual([n['outcome'] for n in nodes],['refuted','verified'])
        self.assertEqual(nodes[0]['after'],nodes[1]['before'])
        self.assertEqual(nodes[0]['before'],nodes[0]['after'])
        run=next(cap.iterdir())
        modified=run/'002'/'after'/'util.c'
        modified.write_bytes(modified.read_bytes()+b'\n')
        with self.assertRaises(replay.Unavailable):
            replay.capture_v2(cap,Path(report),branch/'.symbols'/'engineering_episodes.v1',src,branch)

    def test_structured_oracle_strict_contract(self):
        import json
        import subprocess
        good = {'class': 'pass', 'phase': 'run', 'rc': 0,
                'timeout': False, 'diagnostic_hash': '0' * 64}
        def result(obj, rc=0):
            return subprocess.CompletedProcess([], rc, json.dumps(obj), '')
        self.assertEqual(replay.parse_oracle(result(good)), good)
        for cls, phase in [('semantic_reject', 'run'), ('compile_reject', 'compile')]:
            obj = dict(good, **{'class': cls, 'phase': phase, 'rc': 1})
            self.assertEqual(replay.parse_oracle(result(obj, 1)), obj)
        bad = [dict(good, extra=1), dict(good, rc=True), dict(good, timeout=1),
               dict(good, diagnostic_hash='bad'), dict(good, phase='compile'),
               dict(good, **{'class': 'other'}), dict(good, timeout=True),
               dict(good, **{'class': 'infrastructure_error', 'rc': 127})]
        for obj in bad:
            with self.subTest(obj=obj), self.assertRaises(replay.OracleFailure):
                replay.parse_oracle(result(obj))
        for text in ['not json', '{}', json.dumps(good) + '{}',
                     json.dumps(good)[:-1] + ',"rc":0}']:
            with self.assertRaises(replay.OracleFailure):
                replay.parse_oracle(subprocess.CompletedProcess([], 0, text, ''))
        with self.assertRaises(replay.OracleFailure):
            replay.parse_oracle(result(good, 1))
        with self.assertRaises(replay.OracleFailure):
            replay.parse_oracle(result(dict(good, **{'class': 'infrastructure_error',
                                                       'phase': 'compile', 'rc': 127}), 2))

    def test_readonly_root_private_copy_and_oracle_evidence(self):
        import json
        src = self.root / 'before'; src.mkdir()
        (src / 'main.c').write_text('int main(void){return 1;}')
        src.chmod(0o500)
        task = self.root / 'task'; task.mkdir()
        obj = {'class': 'semantic_reject', 'phase': 'run', 'rc': 1,
               'timeout': False, 'diagnostic_hash': 'a' * 64}
        (task / 'check.py').write_text('import json\nprint(' + repr(json.dumps(obj)) + ')\nraise SystemExit(1)\n')
        report = replay.oracle(task, src, self.root / 'private-logs', 'cell')
        self.assertEqual(report, obj)
        self.assertTrue((self.root / 'private-logs/cell.stdout').is_file())
        self.assertFalse((src / 'check.py').exists())
        src.chmod(0o700)

    def test_outcome_disposition_does_not_hide_refuted_attempts(self):
        self.assertEqual(replay.attempt_disposition([{'outcome': 'abstained'}], False), 'no_candidate')
        self.assertEqual(replay.attempt_disposition([{'outcome': 'refuted'}, {'outcome': 'abstained'}], False), 'refuted_rollback')
        self.assertEqual(replay.attempt_disposition([{'outcome': 'verified'}], True), 'emitted_edit')
        self.assertEqual(replay.attempt_disposition([{'outcome': 'verification_incomplete'}], False), 'unchanged_other')

    def test_technical_pair_retains_revalidatable_raw_capture(self):
        import os
        agent = os.environ.get('SYMBOLS_AGENT_BIN')
        cap_report = os.environ.get('ATTEMPT_CAPTURE_REPORT_BIN')
        episode_report = os.environ.get('EPISODE_REPORT_BIN')
        if not all((agent, cap_report, episode_report)):
            self.skipTest('integration executables not configured')
        task = self.root / 'technical'; (task / 'before').mkdir(parents=True)
        (task / 'before/main.c').write_text('int old_name(void){return 1;} int main(void){return old_name()-1;}\n')
        (task / 'task.md').write_text('Rename old_name to new_name.')
        (task / 'check.py').write_text(
            "import json,pathlib,hashlib\n"
            "ok='new_name' in pathlib.Path('main.c').read_text()\n"
            "print(json.dumps({'class':'pass' if ok else 'semantic_reject','phase':'run',"
            "'rc':0 if ok else 1,'timeout':False,'diagnostic_hash':hashlib.sha256(b'fixture').hexdigest()}))\n"
            "raise SystemExit(0 if ok else 1)\n")
        out = self.root / 'output'
        result = replay.evaluate(self.root, [('technical', task)], Path(agent),
                                 Path(episode_report), Path(cap_report), out)
        self.assertEqual(result['aggregate']['baseline']['resolved'], 1)
        self.assertEqual(result['aggregate']['replay']['resolved'], 1)
        for mode in ('baseline', 'replay'):
            branch = result['paired'][0][mode]
            evidence = out / branch['evidence']
            self.assertTrue((evidence / 'agent.stdout').is_file())
            nodes = replay.capture_v2(evidence / 'capture', Path(cap_report),
                                     evidence / 'engineering_episodes.v1',
                                     out / 'snapshots/000-initial',
                                     self._with_audit_copy(out / ('snapshots/000-' + mode + '-post'), evidence, mode))
            self.assertEqual(nodes, branch['attempts'])
            self.assertIsNotNone(branch['probe'])

    def _with_audit_copy(self, snapshot, evidence, mode):
        import shutil
        root = self.root / ('revalidate-' + mode)
        shutil.copytree(snapshot, root)
        (root / '.symbols').mkdir()
        shutil.copy2(evidence / 'engineering_episodes.v1', root / '.symbols/engineering_episodes.v1')
        return root

    def test_infrastructure_oracle_stops_before_agent(self):
        from unittest.mock import patch
        task = self.root / 'task'; (task / 'before').mkdir(parents=True)
        (task / 'before/main.c').write_text('int main(void){return 0;}')
        with patch.object(replay, 'oracle', side_effect=replay.OracleFailure('oracle_infrastructure_error')), patch.object(replay, 'run_agent') as run:
            with self.assertRaises(replay.OracleFailure):
                replay.evaluate(self.root, [('technical', task)], Path('agent'), Path('report'), Path('cap'), self.root / 'out')
            run.assert_not_called()

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
