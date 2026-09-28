"""Binary preview tests run only generated candidates from fixed test fixtures."""
import base64
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'typed_c_contract'))
from binary_preview import EMPTY, generate, recognize, validate
from preview import snapshot, sha


class BinaryPreviewTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(); self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.work = self.root / 'work'; self.work.mkdir()
        self.source = self.work / 'main.c'
        self.source.write_bytes(b'#include <stdio.h>\nint main(void){printf("old");return 0;}\n')
        self.contract = self.root / 'contract.json'
        self.obj = {'schema': 'symbols.c-binary-preview.v1',
                    'workspace_digest': snapshot(self.work)[1],
                    'stdout': {'bytes_b64': '', 'length': 0, 'termination': 'exact'},
                    'exit_code': 0, 'probe': {'kind': 'single-c-main', 'timeout_ms': 5000},
                    'edit_scope': {'allow': ['main.c'], 'deny': []}, 'source_predicates': []}

    def check(self, goal):
        self.obj['workspace_digest'] = snapshot(self.work)[1]
        self.obj['stdout'] = {'bytes_b64': base64.b64encode(goal).decode(),
                              'length': len(goal), 'termination': 'exact'}
        self.contract.write_text(json.dumps(self.obj), encoding='utf-8')
        return validate(self.contract, self.work)

    def test_all_binary_boundaries_and_candidate_execution(self):
        compiler = shutil.which('cc') if os.name != 'nt' else shutil.which('cmake')
        self.assertIsNotNone(compiler, 'fixture compiler is required by this CI test')
        goals = [b'', b'\x00', b'a\x00b', b'\x80\xff', b'\r\n', b'no LF', b'LF\n',
                 b'100% \\ \x0f\xaf', bytes(range(127)), b'\xff' * 127]
        for goal in goals:
            with self.subTest(goal=repr(goal)):
                result = self.check(goal)
                self.assertEqual(result['candidate_count'], 1)
                item = result['candidates'][0]
                after = generate(goal)
                self.assertEqual(item['after_sha256'], sha(after))
                self.assertEqual(recognize(after), goal)
                self.assertLessEqual(len(item['unified_diff'].encode('utf-8')), 4096)
                self.assertEqual(self.source.read_bytes(),
                                 b'#include <stdio.h>\nint main(void){printf("old");return 0;}\n')
                generated = self.root / 'candidate.c'; generated.write_bytes(after)
                if os.name == 'nt':
                    # CMake configures MSVC's INCLUDE/LIB/SDK environment. Plain cl.exe
                    # on this PowerShell runner has PATH but no stdio.h include path.
                    project = self.root / 'candidate-fixture'
                    project.mkdir(exist_ok=True)
                    (project / 'CMakeLists.txt').write_text(
                        'cmake_minimum_required(VERSION 3.16)\n'
                        'project(binary_preview_fixture C)\n'
                        'add_executable(candidate ../candidate.c)\n', encoding='ascii')
                    build = self.root / 'candidate-build'
                    commands = [[compiler, '-S', str(project), '-B', str(build)],
                                [compiler, '--build', str(build), '--config', 'Release']]
                    executable = build / 'Release' / 'candidate.exe'
                else:
                    executable = self.root / 'candidate'
                    commands = [[compiler, '-std=c11', '-pedantic-errors',
                                 str(generated), '-o', str(executable)]]
                for command in commands:
                    compile_result = subprocess.run(command, cwd=self.root,
                                                    capture_output=True, timeout=60)
                    if compile_result.returncode:
                        self.fail('fixture compiler exit %d: stdout=%r stderr=%r' % (
                            compile_result.returncode, compile_result.stdout[-2048:],
                            compile_result.stderr[-2048:]))
                output = subprocess.run([str(executable)], capture_output=True, timeout=5)
                self.assertEqual((output.returncode, output.stdout), (0, goal))

    def test_identity_and_abstention(self):
        self.assertEqual(self.check(b'old')['candidate_count'], 0)
        self.source.write_bytes(generate(b'hello'))
        self.assertEqual(self.check(b'hello')['candidate_count'], 0)
        self.source.write_bytes(EMPTY)
        self.assertEqual(self.check(b'')['candidate_count'], 0)
        for bad in (b'// comment\n#include <stdio.h>\nint main(void){puts("hi");return 0;}\n',
                    b'#include <stdio.h>\nint main(void){printf("%d",12);return 0;}\n',
                    b'#include <stdio.h>\nint main(void){printf("hi");puts("x");return 0;}\n',
                    b'#include <stdio.h>\nint main(void){printf("hi");return 0;}\n' + b' ' * 770):
            self.source.write_bytes(bad)
            self.assertEqual(self.check(b'new')['candidate_count'], 0)

    def test_whitespace_tolerant_closed_origins(self):
        examples = [
            (b'#include <stdio.h>\n\nint main(void)\n{\n'
             b'    printf("old");\n    return 0;\n}\n', b'old', b'new'),
            (b' #include <stdio.h>\nint  main ( void ) { puts ( "old" ) ; return  0 ; } ',
             b'old\n', b'new\x00'),
            (b'#include <stdio.h>\nint main(void)\n{ return 0; }\n', b'', b'new'),
        ]
        for source, identity, changed in examples:
            with self.subTest(source=source):
                self.source.write_bytes(source)
                self.assertEqual(self.check(identity)['candidate_count'], 0)
                proposal = self.check(changed)
                self.assertEqual((proposal['status'], proposal['candidate_count']),
                                 ('static_candidate_unverified', 1))
                self.assertEqual(proposal['candidates'][0]['rule'], 'answer_bytes')
        invalid = [
            b'//comment\n#include <stdio.h>\nint main(void){puts("x");return 0;}\n',
            b'#include <stdio.h>\n#include <stdlib.h>\nint main(void){puts("x");return 0;}\n',
            b'#include <stdio.h>\nint main(void){printf("%d",3);return 0;}\n',
            b'#include <stdio.h>\nint main(void){puts("x");puts("y");return 0;}\n',
            b'#include <stdio.h>\nint main(void){puts("x");return 1;}\n',
            b'#include <stdio.h>\nint main(void){puts("x"); return 0;}\nint main(void){return 0;}\n',
            b'#include <stdio.h>\nint main(void){printf("%s");return 0;}\n',
            b'#include <stdio.h>\nint main(void){printf("x");return 0;}\n' + b'\n' * 32,
        ]
        for source in invalid:
            with self.subTest(invalid=source[:80]):
                self.source.write_bytes(source)
                self.assertEqual(self.check(b'new')['candidate_count'], 0)

    def test_closed_schema_and_v1_invariance(self):
        self.obj['schema'] = 'symbols.c-repair-contract.v1'
        self.contract.write_text(json.dumps(self.obj))
        with self.assertRaisesRegex(ValueError, 'version'):
            validate(self.contract, self.work)
        self.obj['schema'] = 'symbols.c-binary-preview.v1'
        self.obj['stdout'] = {'bytes_b64': base64.b64encode(b'x'*128).decode(),
                              'length': 128, 'termination': 'exact'}
        self.contract.write_text(json.dumps(self.obj))
        with self.assertRaisesRegex(ValueError, 'stdout'):
            validate(self.contract, self.work)
        from schema import parse as v1_parse
        with self.assertRaisesRegex(ValueError, 'version'):
            v1_parse(self.contract.read_bytes())

    def test_cli_exact_lf_success_and_refusal(self):
        script = Path(__file__).resolve().parents[1] / 'tools' / 'typed_c_contract' / 'binary_preview.py'
        self.check(b'x\x00y')
        command = [sys.executable, str(script), '--preview-c-binary-contract',
                   str(self.contract), '-w', str(self.work)]
        ok = subprocess.run(command, capture_output=True, timeout=10)
        self.assertEqual(ok.returncode, 0)
        self.assertEqual(ok.stderr, b'')
        self.assertTrue(ok.stdout.endswith(b'\n'))
        self.assertFalse(ok.stdout.endswith(b'\r\n'))
        self.assertEqual(ok.stdout.count(b'\n'), 1)
        self.assertEqual(json.loads(ok.stdout)['candidate_count'], 1)
        self.obj['schema'] = 'symbols.c-repair-contract.v1'
        self.contract.write_text(json.dumps(self.obj), encoding='utf-8')
        refused = subprocess.run(command, capture_output=True, timeout=10)
        self.assertEqual((refused.returncode, refused.stdout, refused.stderr),
                         (2, b'', b'refused: version\n'))

    def test_preserved_predicate_blocks_candidate(self):
        self.obj['source_predicates'] = [{'kind': 'file_bytes_equal', 'path': 'main.c',
                                          'sha256': sha(self.source.read_bytes())}]
        self.assertEqual(self.check(b'new')['candidate_count'], 0)

if __name__ == '__main__':
    unittest.main()
