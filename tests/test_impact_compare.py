#!/usr/bin/env python3
"""Golden one-TU impact comparisons; optional compiler binding, no edit authority."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from importlib import metadata

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / 'tools/ast_inspect/compare_impact.py'


def pinned_installed():
    try:
        return metadata.version('libclang') == '18.1.1'
    except metadata.PackageNotFoundError:
        return False


@unittest.skipUnless(pinned_installed(), 'pinned libclang 18.1.1 not installed')
class ImpactCompareTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.lexical = Path(os.environ.get('LEXICAL_IMPACT_BIN', ''))
        if not cls.lexical.is_file():
            raise unittest.SkipTest('build lexical_impact and set LEXICAL_IMPACT_BIN')

    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.root = Path(tmp.name)
        self.src = self.root / 'main.c'
        self.db = self.root / 'compile_commands.json'
        self.entries = [dict(directory=str(self.root), file=str(self.src),
                             arguments=['cc', '-c', str(self.src), '-o', 'main.o'])]

    def compare(self, symbol='target', line=1, variant=None):
        self.db.write_text(json.dumps(self.entries))
        command = [sys.executable, str(SCRIPT), '--db', str(self.db), '--source', str(self.src),
                   '--symbol', symbol, '--line', str(line), '--lexical-bin', str(self.lexical)]
        if variant is not None:
            command += ['--variant', str(variant)]
        before = self.src.read_bytes()
        p = subprocess.run(command, cwd=self.root, text=True, capture_output=True)
        self.assertEqual(p.stderr, '')
        self.assertEqual(self.src.read_bytes(), before)
        self.assertFalse((self.root / '.symbols').exists())
        return p.returncode, json.loads(p.stdout)

    def test_direct_golden_call_and_provenance(self):
        self.src.write_text('int target(void){return 1;}\nint caller(void){return target();}\n')
        rc, r = self.compare()
        self.assertEqual((rc, r['status'], r['ast_only'], r['lexical_only']), (0, 'complete', [], []))
        self.assertEqual([x['name'] for x in r['ast_callers']], ['caller'])
        self.assertEqual(r['ast_callers'][0]['call_sites'][0]['line'], 2)
        self.assertEqual(r['provenance']['source_sha256'], hashlib.sha256(self.src.read_bytes()).hexdigest())

    def test_conditional_variant_disagreement_is_not_resolved(self):
        self.src.write_text('int target(void){return 1;}\n'
                            '#ifdef ACTIVE\nint selected(void){return target();}\n'
                            '#else\nint inactive(void){return target();}\n#endif\n')
        self.entries.append(dict(directory=str(self.root), file=str(self.src),
                                 arguments=['cc', '-DACTIVE', '-c', str(self.src), '-o', 'active.o']))
        rc, r = self.compare()
        self.assertEqual((rc, r['reason']), (2, 'ast_ambiguous_variant'))
        rc, inactive = self.compare(variant=0)
        rc2, active = self.compare(variant=1)
        self.assertEqual((rc, rc2), (0, 0))
        self.assertEqual([x['name'] for x in inactive['ast_callers']], ['inactive'])
        self.assertEqual([x['name'] for x in active['ast_callers']], ['selected'])
        self.assertNotEqual(inactive['provenance']['command_sha256'], active['provenance']['command_sha256'])
        for result in (inactive, active):
            self.assertTrue(result['ast_only'] or result['lexical_only'])
            self.assertFalse(result['agreement_on_names'])
            self.assertIn('disagreement_not_resolved', result['limitations'])

    def test_shadowed_reference_not_a_caller(self):
        self.src.write_text('int target(void){return 1;}\nint local(void){int target=1;return target;}\n')
        rc, r = self.compare(line=1)
        self.assertEqual(rc, 0)
        self.assertEqual(r['ast_callers'], [])
        self.assertEqual(r['ast_references'], [])

    def test_cross_tu_is_explicitly_incomplete(self):
        other = self.root / 'other.c'
        self.src.write_text('int target(void){return 1;}\n')
        other.write_text('extern int target(void);\nint outside(void){return target();}\n')
        rc, r = self.compare()
        self.assertEqual((rc, [x['name'] for x in r['ast_callers']]), (0, []))
        self.assertEqual(r['scope'], 'selected_tu_direct_calls_only')
        self.assertIn('single_tu_not_project_complete', r['limitations'])

    def test_missing_db_unsupported_and_indirect_are_unknown(self):
        self.src.write_text('int target(void){return 1;}\nint (*p)(void)=target;\nint local(void){return p();}\n')
        rc, r = self.compare()
        self.assertEqual((rc, r['reason']), (2, 'ast_unresolved_bindings'))
        self.entries[0]['arguments'].insert(1, '@secret.rsp')
        rc, r = self.compare()
        self.assertEqual((rc, r['reason']), (2, 'ast_response_file_unsupported'))


if __name__ == '__main__':
    unittest.main()
