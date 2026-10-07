#!/usr/bin/env python3
"""S11: duplicated registered JSON keys, portable mock boundary, exact mutants.

No real Windows CTest producer claim. No production switch or WILL_FAIL.
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
CASES = {
    'tests_equal': '{"tests":[{"name":"pass"}],"tests":[{"name":"pass"}]}',
    'tests_conflict': '{"tests":[],"tests":[{"name":"pass"}]}',
    'name_equal': '{"tests":[{"name":"pass","name":"pass"}]}',
    'name_conflict': '{"tests":[{"name":"other","name":"pass"}]}',
    'name_escaped': '{"tests":[{"na\\u006de":"other","name":"pass"}]}',
    'extra_root': '{"extra":1,"extra":2,"tests":[{"name":"pass"}]}',
    'extra_nested': '{"tests":[{"name":"pass","extra":{"x":1,"x":2}}]}',
}


def checks(tool, xml):
    spec = importlib.util.spec_from_file_location('report_json_duplicates', tool)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    bad = set()
    for name, raw in list(CASES.items()) + [('valid', '{"tests":[{"name":"pass"}],"extra":{"x":1},"x":2}')]:
        out, err = io.StringIO(), io.StringIO()
        response = subprocess.CompletedProcess(['ctest'], 0, raw, '')
        try:
            with patch.object(mod.subprocess, 'run', return_value=response):
                with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
                    rc = mod.main(['ctest_report.py', str(xml), '--registered', 'scratch'])
            if name == 'valid':
                d = json.loads(out.getvalue())
                ok = rc == 0 and err.getvalue() == '' and d['counts']['passed'] == 1 and \
                    len(d['tests']) == 1 and d['tests'][0]['name'] == 'pass'
            else:
                ok = rc == 2 and out.getvalue() == '' and err.getvalue().startswith('ctest_report:') and \
                    'Traceback' not in err.getvalue()
        except Exception:
            ok = False
        if not ok:
            bad.add(name)
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
        mutants = {
            'hook_removed': ('json.loads(p.stdout, object_pairs_hook=unique_json_object)', 'json.loads(p.stdout)'),
            'guard_removed': ('if key in out:', 'if False:'),
        }
        for name, (old, new) in mutants.items():
            assert source.count(old) == 1, name
            mutant = tmp / (name + '.py')
            mutant.write_text(source.replace(old, new))
            got = checks(mutant, xml)
            if got != set(CASES):
                print('mutant exact-set mismatch', name, sorted(got), sorted(CASES))
                return 1
            print('MUTANT', name, 'killed by exactly', ' '.join(sorted(got)))
    print('PASS registered JSON keys: 8 cells, 2 exact-set mutants')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
