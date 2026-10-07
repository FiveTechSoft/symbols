#!/usr/bin/env python3
"""S8: reject ambiguous structure, preserve unknown reasons, exact source mutants."""
import copy
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent.parent
TOOL = ROOT / 'tools/ctest_report.py'


def checks(tool, tmp, root, env):
    bad = set()

    def check(name, changed, registered=None):
        path = tmp / (name + '.xml')
        ET.ElementTree(changed).write(path)
        e = dict(env)
        args = [sys.executable, str(tool), str(path)]
        if registered is not None:
            e['S8_REGISTERED'] = json.dumps({'tests': [{'name': n} for n in registered]})
            args += ['--registered', 'scratch']
        p = subprocess.run(args, env=e, capture_output=True, text=True, timeout=30)
        if p.returncode != 2 or p.stdout != '' or not p.stderr.startswith('ctest_report:'):
            bad.add(name)

    def clone():
        r = copy.deepcopy(root)
        return r, r.find('Testing'), r.find('Testing/Test')

    r, sec, t = clone(); sec.append(copy.deepcopy(t)); check('duplicate_test', r)
    r, sec, t = clone(); ET.SubElement(t, 'Name').text = 'other'; check('duplicate_name', r)
    r, sec, t = clone(); t.find('Name').text = ' '; check('empty_name', r)
    r, sec, t = clone(); ET.SubElement(t.find('Name'), 'nested').text = 'x'; check('nested_name', r)
    for key in ['Completion Status', 'Exit Code', 'Exit Value', 'Execution Time']:
        for equal in [True, False]:
            r, sec, t = clone()
            res = t.find('Results')
            n = next((n for n in res if n.get('name') == key), None)
            if n is None:
                n = ET.SubElement(res, 'NamedMeasurement', {'name': key})
                ET.SubElement(n, 'Value').text = 'Completed' if key == 'Exit Code' else '0'
            other = copy.deepcopy(n)
            if not equal: other.find('Value').text = 'contradiction'
            res.insert(0, other)
            check('duplicate_measurement_' + key.replace(' ', '_') + ('_equal' if equal else '_conflict'), r)
    r, sec, t = clone(); ET.SubElement(t.find('Results/NamedMeasurement'), 'Value').text = 'other'; check('duplicate_value', r)
    r, sec, t = clone(); r.tag = 'Bogus'; check('wrong_root', r)
    r, sec, t = clone(); ET.SubElement(r, 'Testing'); check('multiple_testing', r)
    r, sec, t = clone(); ET.SubElement(t, 'Testing'); check('nested_testing', r)
    r, sec, t = clone(); sec.remove(t); r.append(t); check('misplaced_test', r)
    r, sec, t = clone(); entry = ET.SubElement(ET.SubElement(sec, 'TestList'), 'Test'); ET.SubElement(entry, 'Name').text = 'pass'; check('nonbare_testlist', r)
    check('registered_duplicate', root, ['pass', 'pass'])
    check('registered_empty', root, ['pass', ''])
    check('registered_wrong_type', root, ['pass', 7])
    check('registered_missing', root, ['other'])
    r, sec, t = clone()
    t.set('Status', 'weird')
    p = tmp / 'unknown.xml'; ET.ElementTree(r).write(p)
    run = subprocess.run([sys.executable, str(tool), str(p)], capture_output=True, text=True, timeout=30)
    try: ok = run.returncode == 0 and json.loads(run.stdout)['tests'][0]['class'] == 'unknown'
    except ValueError: ok = False
    if not ok: bad.add('unknown_preserved')
    return bad


def main():
    if os.name == 'nt':
        print('SKIP: real CTest XML structure fixture not measured on Windows')
        return 77
    with tempfile.TemporaryDirectory() as td:
        tmp = Path(td); b = tmp / 'b'
        subprocess.run([sys.argv[1], '-S', str(ROOT / 'tests/fixtures/ctest_report'), '-B', str(b)], check=True, capture_output=True)
        subprocess.run([sys.argv[1], '--build', str(b)], check=True, capture_output=True)
        subprocess.run([sys.argv[2], '--test-dir', str(b), '-T', 'Test', '-R', '^pass$'], check=True, capture_output=True)
        root = ET.parse(next((b / 'Testing').rglob('Test.xml'))).getroot()
        stub = tmp / 'ctest'
        stub.write_text('#!/usr/bin/env python3\nimport os\nprint(os.environ["S8_REGISTERED"])\n')
        stub.chmod(0o755)
        env = dict(os.environ, CTEST_COMMAND=str(stub))
        got = checks(TOOL, tmp, root, env)
        if got: print('baseline failed', sorted(got)); return 1
        source = TOOL.read_text()
        mutations = {
            'root': ('if root.tag != "Site":', 'if False:', {'wrong_root'}),
            'sections': ('if len(sections) != 1 or len(list(root.iter("Testing"))) != 1:', 'if False:', {'multiple_testing', 'nested_testing'}),
            'location': ('if len(real) + len(listed) != len(list(root.iter("Test"))) or any(len(t) or t.attrib for t in listed):', 'if False:', {'misplaced_test', 'nonbare_testlist'}),
            'name_shape': ('if len(names) != 1 or len(names[0]):', 'if False:', {'duplicate_name', 'nested_name'}),
            'name_empty': ('if any(not isinstance(n, str) or not n.strip() for n in names):', 'if False:', {'empty_name', 'registered_empty', 'registered_wrong_type'}),
            'name_duplicate': ('if len(names) != len(set(names)):', 'if False:', {'duplicate_test', 'registered_duplicate'}),
            'measurement_duplicate': ('if key in critical:', 'if False:', {'duplicate_measurement_' + k.replace(' ', '_') + suffix for k in ['Completion Status', 'Exit Code', 'Exit Value', 'Execution Time'] for suffix in ['_equal', '_conflict']}),
            'value_duplicate': ('if len(n.findall("Value")) > 1:', 'if False:', {'duplicate_value'}),
            'registered_set': ('if not (seen - {""}) <= set(names):', 'if False:', {'registered_missing'}),
        }
        for name, (old, new, want) in mutations.items():
            assert source.count(old) == 1, name
            mutant = tmp / (name + '.py'); mutant.write_text(source.replace(old, new))
            got = checks(mutant, tmp, root, env)
            if got != want:
                print('mutant exact-set mismatch', name, sorted(got), sorted(want)); return 1
            print('MUTANT', name, 'killed by exactly', ' '.join(sorted(want)))
    print('ctest_report structure ok')
    return 0


if __name__ == '__main__':
    sys.exit(main())
