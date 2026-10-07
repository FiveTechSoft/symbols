#!/usr/bin/env python3
"""S10: critical Value scalar shape, synthetic portable XML, exact source mutants.

This does not measure real Windows CTest reasons or XML generation.
"""
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import tempfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent.parent
TOOL = ROOT / 'tools/ctest_report.py'
BASE = '''<Site><Testing><Test Status="passed"><Name>pass</Name><Results>
<NamedMeasurement name="Completion Status"><Value>Completed</Value></NamedMeasurement>
<NamedMeasurement name="Exit Code"><Value>Completed</Value></NamedMeasurement>
<NamedMeasurement name="Exit Value"><Value>0</Value></NamedMeasurement>
<NamedMeasurement name="Execution Time"><Value>0.1</Value></NamedMeasurement>
</Results></Test></Testing></Site>'''
KEYS = ['Completion Status', 'Exit Code', 'Exit Value', 'Execution Time']


def checks(tool, tmp):
    spec = importlib.util.spec_from_file_location('report_values', tool)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    bad = set()

    def run(name, root, expected=None):
        xml = tmp / (name + '.xml')
        ET.ElementTree(root).write(xml)
        out, err = io.StringIO(), io.StringIO()
        try:
            with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
                rc = mod.main(['ctest_report.py', str(xml)])
            if expected is None:
                ok = rc == 2 and out.getvalue() == '' and err.getvalue().startswith('ctest_report:')
            else:
                d = json.loads(out.getvalue())
                ok = rc == 0 and err.getvalue() == '' and len(d['tests']) == 1 and \
                    d['tests'][0]['class'] == expected and d['counts'][expected] == 1 and \
                    sum(d['counts'].values()) == 1
        except Exception:
            ok = False
        if not ok:
            bad.add(name)

    run('valid_pass', ET.fromstring(BASE), 'passed')
    for key in KEYS:
        label = key.replace(' ', '_')
        for shape in ['missing', 'nested']:
            r = ET.fromstring(BASE)
            n = next(n for n in r.iter('NamedMeasurement') if n.get('name') == key)
            if shape == 'missing':
                n.remove(n.find('Value'))
            else:
                ET.SubElement(n.find('Value'), 'nested').text = 'contradiction'
            run(shape + '_' + label, r)
    r = ET.fromstring(BASE)
    r.find('Testing/Test/Results/NamedMeasurement/Value').text = 'Unknown reason'
    run('unknown_scalar', r, 'unknown')
    r = ET.fromstring(BASE)
    res = r.find('Testing/Test/Results')
    res.remove(res.find('NamedMeasurement'))
    run('absent_measurement', r, 'unknown')
    r = ET.fromstring(BASE)
    res = r.find('Testing/Test/Results')
    n = ET.SubElement(res, 'NamedMeasurement', {'name': 'Extra diagnostic'})
    ET.SubElement(ET.SubElement(n, 'Value'), 'nested').text = 'unconsumed'
    run('noncritical_unchanged', r, 'passed')
    return bad


def main():
    with tempfile.TemporaryDirectory() as td:
        tmp = Path(td)
        got = checks(TOOL, tmp)
        if got:
            print('baseline failed', sorted(got))
            return 1
        source = TOOL.read_text()
        mutants = {
            'missing': ('if not values:', 'if False:', {'missing_' + k.replace(' ', '_') for k in KEYS}),
            'nested': ('if len(values[0]):', 'if False:', {'nested_' + k.replace(' ', '_') for k in KEYS}),
        }
        for name, (old, new, want) in mutants.items():
            assert source.count(old) == 1, name
            mutant = tmp / (name + '.py')
            mutant.write_text(source.replace(old, new))
            got = checks(mutant, tmp)
            if got != want:
                print('mutant exact-set mismatch', name, sorted(got), sorted(want))
                return 1
            print('MUTANT', name, 'killed by exactly', ' '.join(sorted(want)))
    print('PASS critical Value shape: 12 cells, 2 exact-set mutants')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
