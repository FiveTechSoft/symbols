#!/usr/bin/env python3
"""Contract tests for the opt-in C source-fact CLI; no C program runs."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from importlib import metadata

SCRIPT = Path(__file__).resolve().parents[1] / 'tools/ast_inspect/qa.py'


def pinned():
    try:
        return metadata.version('libclang') == '18.1.1'
    except metadata.PackageNotFoundError:
        return False


@unittest.skipUnless(pinned(), 'pinned libclang 18.1.1 not installed')
class SourceQATest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.source = self.root / 'main.c'
        self.db = self.root / 'compile_commands.json'
        self.source.write_bytes(b'int foo(void){return 1;}\nint main(void){return foo();}\n')
        self.db.write_text(json.dumps([dict(directory=str(self.root), file=str(self.source),
                                            arguments=['cc', '-c', str(self.source), '-o', 'main.o'])]))

    def query(self, kind, name, extra=()):
        p = subprocess.run([sys.executable, str(SCRIPT), '--db', str(self.db), '--source', str(self.source),
                            '--fact', kind, '--symbol', name, *extra], cwd=self.root,
                           capture_output=True, text=True, timeout=30)
        self.assertFalse(p.stderr, p.stderr)
        return p.returncode, json.loads(p.stdout)

    def test_source_fact_provenance_and_no_mutation(self):
        original = self.source.read_bytes()
        rc, result = self.query('calls', 'foo')
        self.assertEqual(rc, 0)
        self.assertEqual(result['status'], 'complete')
        self.assertEqual(result['source_sha256'], hashlib.sha256(original).hexdigest())
        self.assertEqual(len(result['matches']), 1)
        self.assertEqual(result['matches'][0]['target_name'], 'foo')
        self.assertEqual(self.source.read_bytes(), original)
        self.assertEqual(set(p.name for p in self.root.iterdir()), {'main.c', 'compile_commands.json'})

    def test_absence_ambiguity_and_bad_context_fail_closed(self):
        self.assertEqual(self.query('declaration', 'missing')[1]['reason'], 'symbol_absent_or_ambiguous')
        self.assertEqual(self.query('calls', 'foo;system')[1]['reason'], 'invalid_symbol')
        self.source.write_bytes(b'int foo(void){return 1;}\nint main(void){int foo=0;return foo;}\n')
        self.assertEqual(self.query('references', 'foo')[1]['reason'], 'symbol_absent_or_ambiguous')
        self.source.write_bytes(b'#include "missing.h"\nint foo(void){return 1;}\n')
        self.assertEqual(self.query('declaration', 'foo')[1]['reason'], 'diagnostics')

    def test_variant_and_header_provenance(self):
        header = self.root / 'value.h'
        header.write_bytes(b'#define VALUE 1\n')
        self.source.write_bytes(b'#include "value.h"\nint foo(void){return VALUE;}\n')
        rc, r = self.query('declaration', 'foo')
        self.assertEqual(rc, 0)
        self.assertEqual(r['includes'][0]['sha256'], hashlib.sha256(header.read_bytes()).hexdigest())
        entries = json.loads(self.db.read_text())
        entries.append(dict(directory=str(self.root), file=str(self.source),
                            arguments=['cc', '-DOTHER', '-c', str(self.source), '-o', 'second.o']))
        self.db.write_text(json.dumps(entries))
        self.assertEqual(self.query('declaration', 'foo')[1]['reason'], 'ambiguous_variant')
        self.assertEqual(self.query('declaration', 'foo', ('--variant', '1'))[0], 0)


if __name__ == '__main__':
    unittest.main()
