#!/usr/bin/env python3
"""S9: registered JSON schema and subprocess failures, portable mock boundary.

No real Windows CTest reason or timeout timing claim. No production test switch.
Each source mutant must fail exactly its expected check set; no WILL_FAIL.
"""
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import tempfile
from unittest.mock import patch

ROOT = Path(__file__).resolve().parent.parent
TOOL = ROOT / 'tools/ctest_report.py'
XML = '''<Site><Testing><Test Status="passed"><Name>pass</Name><Results>
<NamedMeasurement name="Completion Status"><Value>Completed</Value></NamedMeasurement>
</Results></Test></Testing></Site>'''


def checks(tool, xml):
    spec = importlib.util.spec_from_file_location('report_under_test', tool)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    bad = set()

    def run(name, payload=None, error=None, rc=0, raw=None, valid=False):
        out, err = io.StringIO(), io.StringIO()
        response = subprocess.CompletedProcess(['ctest'], rc, raw if raw is not None else json.dumps(payload), '')
        try:
            with patch.dict(mod.os.environ, {'CTEST_COMMAND': 'ctest'}), patch.object(mod.subprocess, 'run', side_effect=error, return_value=response) as stub:
                with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
                    code = mod.main(['ctest_report.py', str(xml), '--registered', 'scratch'])
            stub.assert_called_once_with(['ctest', '--test-dir', 'scratch', '--show-only=json-v1'],
                                         capture_output=True, text=True, timeout=60)
            if valid:
                report = json.loads(out.getvalue())
                ok = code == 0 and err.getvalue() == '' and report['counts']['passed'] == 1 and \
                    report['counts']['not_run'] == 1 and sum(report['counts'].values()) == 2 and \
                    [(t['name'], t['class'], t['detail']) for t in report['tests']] == \
                    [('absent', 'not_run', 'absent from Test.xml'), ('pass', 'passed', '')]
            else:
                ok = code == 2 and out.getvalue() == '' and err.getvalue().startswith('ctest_report:') and \
                    'Traceback' not in err.getvalue()
        except Exception:
            ok = False
        if not ok:
            bad.add(name)

    run('valid', {'tests': [{'name': 'pass'}, {'name': 'absent', 'extra': 9}], 'extra': True}, valid=True)
    run('root_list', [])
    run('root_null', None)
    run('tests_missing', {})
    for label, value in [('null', None), ('object', {'name': 'pass'}), ('string', 'pass')]:
        run('tests_' + label, {'tests': value})
    for label, value in [('null', None), ('string', 'pass'), ('list', ['pass'])]:
        run('entry_' + label, {'tests': [value]})
    run('name_missing', {'tests': [{}]})
    run('name_number', {'tests': [{'name': 'pass'}, {'name': 7}]})
    run('name_empty', {'tests': [{'name': 'pass'}, {'name': ' '}]})
    run('name_duplicate', {'tests': [{'name': 'pass'}, {'name': 'pass'}]})
    run('xml_unregistered', {'tests': []})
    run('invalid_json', raw='not json')
    run('nonzero', {'tests': [{'name': 'pass'}]}, rc=1)
    run('launch_error', error=FileNotFoundError('fixture missing ctest'))
    run('timeout', error=subprocess.TimeoutExpired(['ctest'], 60))
    return bad


def main():
    with tempfile.TemporaryDirectory() as td:
        tmp = Path(td)
        xml = tmp / 'test.xml'
        xml.write_text(XML)
        got = checks(TOOL, xml)
        if got:
            print('baseline failed', sorted(got))
            return 1
        source = TOOL.read_text()
        mutations = {
            'root_schema': ('if not isinstance(data, dict):', 'if False:', {'root_list', 'root_null'}),
            'tests_schema': ('if not isinstance(data.get("tests"), list):', 'if False:', {'tests_null'}),
            'entry_schema': ('if any(not isinstance(t, dict) or "name" not in t for t in data["tests"]):', 'if False:', {'entry_null', 'entry_string', 'entry_list'}),
            'nonzero': ('if p.returncode != 0:', 'if False:', {'nonzero'}),
            'timeout': ('OSError, subprocess.TimeoutExpired, ValueError', 'OSError, ValueError', {'timeout'}),
            'names': ('if any(not isinstance(n, str) or not n.strip() for n in names):', 'if False:', {'name_number', 'name_empty'}),
            'duplicates': ('if len(names) != len(set(names)):', 'if False:', {'name_duplicate'}),
            'coverage': ('if not (seen - {""}) <= set(names):', 'if False:', {'xml_unregistered'}),
        }
        for name, (old, new, want) in mutations.items():
            assert source.count(old) == 1, name
            mutant = tmp / (name + '.py')
            mutant.write_text(source.replace(old, new))
            got = checks(mutant, xml)
            if got != want:
                print('mutant exact-set mismatch', name, sorted(got), sorted(want))
                return 1
            print('MUTANT', name, 'killed by exactly', ' '.join(sorted(want)))
    print('PASS registered boundary: 19 cells, 8 exact-set mutants')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
