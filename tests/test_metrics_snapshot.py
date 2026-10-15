"""Frozen pre-code scenario contract; no efficacy measurements."""
import copy
import importlib.util
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]

def module(name, file):
    spec = importlib.util.spec_from_file_location(name, file)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod

m = module('snapshot', ROOT / 'tools/metrics_snapshot.py')
GOLD = json.loads((ROOT / 'tests/metrics_snapshot_gold.json').read_text())
BASE = GOLD['base_snapshot']
A, B = 'a'*40, 'b'*40
EXPECTED = (
    'Evidence snapshot for subject commit `' + A + '`.\n\n'
    'Curated source claims, not new measurements. CI success is not semantic efficacy.\n\n'
    '| Metric | Evidence class | Value | Measurement context | Source |\n'
    '|---|---|---|---|---|\n'
    '| CI jobs | Current technical | 5 of 5 jobs success | 2026-10-15; Linux and Windows; `' + A + '` | '
    '[source](https://github.com/FiveTechSoft/symbols/actions/runs/123) |\n')

class Tests(unittest.TestCase):
    def load(self, obj):
        return m.load(json.dumps(obj).encode())

    def test_freshness_exact(self):
        # This validates production schema, not the truth of its curated claims.
        m.load((ROOT / "tools/metrics/current.json").read_bytes())
        self.assertEqual(m.render(self.load(BASE)), EXPECTED)
        self.assertEqual(m.render(self.load(BASE)), m.render(self.load(BASE)))
        for kind, sha, expected in [
            ('technical_result', B, 'Historical technical'),
            ('developer_contract', B, 'Historical developer'),
            ('developer_contract', A, 'Current developer'),
            ('historical_efficacy', A, 'Historical efficacy'),
            ('historical_efficacy', B, 'Historical efficacy')]:
            with self.subTest(kind=kind, sha=sha):
                o = copy.deepcopy(BASE)
                o['entries'][0].update(claim_type=kind, measured_sha=sha)
                rendered = m.render(self.load(o))
                self.assertEqual(rendered, EXPECTED.replace('Current technical', expected).replace('`'+A+'` |', '`'+sha+'` |'))
        o = copy.deepcopy(BASE)
        o['entries'][0].update(claim_type='unknown', measured_sha=None, value=None)
        self.assertEqual(m.render(self.load(o)), EXPECTED.replace('Current technical', 'Unmeasured on subject SHA').replace('5 of 5 jobs success', 'Unmeasured'))

    def test_invalid_snapshots_no_write(self):
        mutations = [
            lambda o:o['entries'].append(copy.deepcopy(o['entries'][0])),
            lambda o:o['entries'][0].pop('date'),
            lambda o:o.update(extra=True),
            lambda o:o.update(version=True),
            lambda o:o.update(entries=[]),
            lambda o:o.update(entries=[dict(o['entries'][0], id='id'+str(i)) for i in range(33)]),
            lambda o:o['entries'][0].update(date='2026-02-30'),
            lambda o:o.update(subject_sha='ABC'),
            lambda o:o['entries'][0].update(title='one|two'),
            lambda o:o['entries'][0].update(value='line\nbreak'),
            lambda o:o['entries'][0].update(source_url='https://evil.invalid/x'),
            lambda o:o['entries'][0].update(source_url='https://u@github.com/FiveTechSoft/symbols/actions/runs/123'),
            lambda o:o['entries'][0].update(source_url='https://github.com/FiveTechSoft/symbols/actions/runs/123/evil'),
            lambda o:o['entries'][0].update(claim_type='entailment'),
            lambda o:o['entries'][0].update(value=5),
        ]
        raw = [b'{"version":1,"version":1}', b'{"value":NaN}', b'{"value":Infinity}', b'\xff', b' '*65537]
        for mutate in mutations:
            o = copy.deepcopy(BASE); mutate(o); raw.append(json.dumps(o).encode())
        with tempfile.TemporaryDirectory() as d:
            rd, snap = Path(d)/'README', Path(d)/'snapshot'
            rd.write_bytes(GOLD['readme_lf'].encode())
            before = rd.read_bytes()
            for i, bad in enumerate(raw):
                with self.subTest(i=i):
                    snap.write_bytes(bad)
                    with self.assertRaises(SystemExit) as err:
                        m.main(['--snapshot', str(snap), '--readme', str(rd), '--write'])
                    self.assertEqual(err.exception.code, 2)
                    self.assertEqual(rd.read_bytes(), before)
                    self.assertEqual(list(Path(d).glob('.metrics-*')), [])

    def test_write_exact_lf_crlf_bom(self):
        with tempfile.TemporaryDirectory() as d:
            rd = Path(d)/'README'
            for eol, bom in [(b'\n', b''), (b'\r\n', b'\xef\xbb\xbf')]:
                with self.subTest(eol=eol):
                    old = bom + GOLD['readme_lf'].encode().replace(b'\n', eol)
                    rd.write_bytes(old)
                    os.chmod(rd, 0o640)
                    mode = rd.stat().st_mode & 0o777
                    m.rewrite(rd, EXPECTED)
                    self.assertEqual(rd.read_bytes(), old.replace(b'old'+eol, EXPECTED.encode().replace(b'\n', eol)))
                    self.assertEqual(rd.stat().st_mode & 0o777, mode)
                    # Idempotence and outside-block bytes.
                    once = rd.read_bytes(); m.rewrite(rd, EXPECTED)
                    self.assertEqual(rd.read_bytes(), once)

    def test_render_only(self):
        with tempfile.TemporaryDirectory() as d:
            snap, rd = Path(d)/'snapshot', Path(d)/'README'
            snap.write_text(json.dumps(BASE));rd.write_text('unchanged')
            with mock.patch('builtins.print') as pr:
                self.assertEqual(m.main(['--snapshot', str(snap)]), 0)
            pr.assert_called_once_with(EXPECTED, end='')
            self.assertEqual(rd.read_text(), 'unchanged')
            self.assertEqual(sorted(x.name for x in Path(d).iterdir()), ['README', 'snapshot'])

    def test_bad_markers_unchanged(self):
        raw = GOLD['readme_lf'].encode()
        variants = [raw.replace(m.END, b'no marker'), raw.replace(m.BEGIN, m.BEGIN+b'\n'+m.BEGIN),
                    b'prefix\n'+m.END+b'\nold\n'+m.BEGIN+b'\nsuffix\n',
                    raw.replace(m.END+b'\n', m.END+b'\r\n'), b'\0'+raw]
        with tempfile.TemporaryDirectory() as d:
            rd=Path(d)/'README'
            for data in variants:
                with self.subTest(data=data):
                    rd.write_bytes(data)
                    with self.assertRaises(ValueError):m.rewrite(rd, EXPECTED)
                    self.assertEqual(rd.read_bytes(), data)
            # Replace failure after temp creation must not change README or leave temp.
            rd.write_bytes(raw)
            with mock.patch.object(m.os, 'replace', side_effect=OSError('injected replace failure')):
                with self.assertRaises(OSError):m.rewrite(rd, EXPECTED)
            self.assertEqual(rd.read_bytes(),raw)
            self.assertEqual(list(Path(d).glob('.metrics-*')),[])

    def test_symlink_refused(self):
        with tempfile.TemporaryDirectory() as d:
            rd, target = Path(d)/'README', Path(d)/'target'
            target.write_bytes(GOLD['readme_lf'].encode())
            try:rd.symlink_to(target)
            except OSError:
                # Windows may disallow creating links; nonregular-mode gate still
                # tested through a mocked lstat. No claim of native symlink test.
                with mock.patch.object(Path,'lstat',return_value=type('S',(),{'st_mode':0o120777})()):
                    with self.assertRaises(ValueError):m.rewrite(rd, EXPECTED)
            else:
                with self.assertRaises(ValueError):m.rewrite(rd, EXPECTED)
                self.assertTrue(rd.is_symlink())
            self.assertEqual(target.read_bytes(),GOLD['readme_lf'].encode())

    def test_legacy_write_no_measurements(self):
        legacy=module('legacy_metrics',ROOT/'tools/metrics.py')
        with mock.patch.object(sys,'argv',['metrics.py','--write','--ci']):
            with mock.patch.object(legacy,'git',side_effect=AssertionError('measurement called')):
                with self.assertRaises(SystemExit) as err:legacy.main()
        self.assertIn('deprecated',str(err.exception))

if __name__ == '__main__':unittest.main()
