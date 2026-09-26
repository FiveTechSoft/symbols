#!/usr/bin/env python3
"""Isolated AST consumer checks; skip only when optional pinned package is absent."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from importlib import metadata

SCRIPT = Path(__file__).resolve().parents[1] / "tools/ast_inspect/inspect.py"


def pinned_installed():
    try:
        return metadata.version('libclang') == '18.1.1'
    except metadata.PackageNotFoundError:
        return False


@unittest.skipUnless(pinned_installed(), "pinned libclang 18.1.1 not installed")
class AstInspectTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.source = self.root / 'main.c'
        self.db = self.root / 'compile_commands.json'
        self.source.write_text('int foo(void){return 1;}\nint main(void){return foo();}\n')
        self.entries = [dict(directory=str(self.root), file=str(self.source), arguments=['cc', '-c', str(self.source), '-o', 'main.o'])]

    def run_inspect(self, *args):
        self.db.write_text(json.dumps(self.entries))
        p = subprocess.run([sys.executable, str(SCRIPT), '--db', str(self.db), '--source', str(self.source), *args],
                           cwd=self.root, text=True, capture_output=True)
        self.assertFalse(p.stderr, p.stderr)
        return p.returncode, json.loads(p.stdout)

    def test_direct_call_and_source_provenance(self):
        before = self.source.read_bytes()
        rc, r = self.run_inspect()
        self.assertEqual((rc, r['status']), (0, 'complete'))
        self.assertEqual([x['target_name'] for x in r['facts'] if x['kind'] == 'call'], ['foo'])
        self.assertEqual(r['source_sha256'], __import__('hashlib').sha256(before).hexdigest())
        self.assertEqual(self.source.read_bytes(), before)
        self.assertFalse((self.root / '.symbols').exists())

    def test_variant_is_explicit_and_define_changes_active_target(self):
        self.source.write_text('#ifdef WIN\nint plat(void){return 1;}\n#else\nint plat(void){return 2;}\n#endif\nint main(void){return plat();}\n')
        self.entries.append(dict(directory=str(self.root), file=str(self.source),
                                 arguments=['cc', '-DWIN', '-c', str(self.source), '-o', 'win.o']))
        rc, r = self.run_inspect()
        self.assertEqual((rc,r['reason']), (2,'ambiguous_variant'))
        _, a = self.run_inspect('--variant','0')
        _, b = self.run_inspect('--variant','1')
        self.assertEqual([x['location']['line'] for x in a['facts'] if x['name']=='plat' and x['kind']=='declaration'], [4])
        self.assertEqual([x['location']['line'] for x in b['facts'] if x['name']=='plat' and x['kind']=='declaration'], [2])
        self.assertNotEqual(a['command_sha256'], b['command_sha256'])
        self.assertNotEqual(a['facts'][1]['location'], b['facts'][1]['location'])

    def test_missing_header_and_error_fail_unknown(self):
        self.source.write_text('#include "missing.h"\nint main(void){return 0;}\n')
        rc, r = self.run_inspect()
        self.assertEqual((rc,r['reason']), (2,'diagnostics'))
        self.source.write_text('int main(void){return non_existent();}\n')
        rc, r = self.run_inspect()
        self.assertEqual((rc,r['reason']), (2,'diagnostics'))

    def test_header_closure_and_missing_db(self):
        (self.root/'helper.h').write_text('static int helper(void){return 4;}\n')
        self.source.write_text('#include "helper.h"\nint main(void){return helper();}\n')
        rc,r = self.run_inspect()
        self.assertEqual((rc,r['status']), (0,'complete'))
        self.assertEqual(r['facts'][-2]['target']['file'], str(self.root/'helper.h'))
        self.assertEqual([x['file'] for x in r['includes']], [str(self.root/'helper.h')])
        (self.root/'helper.h').unlink()
        rc,r = self.run_inspect()
        self.assertEqual((rc,r['reason']), (2,'diagnostics'))
        self.db.unlink()
        p = subprocess.run([sys.executable, str(SCRIPT), '--db', str(self.db), '--source', str(self.source)], capture_output=True, text=True)
        self.assertEqual((p.returncode,json.loads(p.stdout)['reason']), (2,'missing_or_invalid_db'))

    def test_unsupported_response_file_refused(self):
        self.entries[0]['arguments'].insert(1, '@private-flags.rsp')
        rc,r = self.run_inspect()
        self.assertEqual((rc,r['reason']), (2,'response_file_unsupported'))

    def test_indirect_call_never_reported_as_direct(self):
        self.source.write_text('int f(void){return 1;}\nint main(void){int (*p)(void)=f; return p();}\n')
        rc,r = self.run_inspect()
        self.assertEqual((rc,r['reason']), (2,'unresolved_bindings'))
        self.assertTrue(any(x.get('reason')=='indirect_call' for x in r['unresolved']))

    def test_shadowed_reference_targets(self):
        self.source.write_text('int x;\nint f(void){int x=1; return x;}\n')
        rc,r = self.run_inspect()
        self.assertEqual((rc,r['status']), (0,'complete'))
        refs = [x for x in r['facts'] if x['kind']=='reference' and x['name']=='x']
        self.assertEqual([x['target']['line'] for x in refs], [2])


from test_c_source_qa import SourceQATest  # Also run the source-fact QA corpus in native Windows AST CI.


if __name__ == '__main__':
    unittest.main()
