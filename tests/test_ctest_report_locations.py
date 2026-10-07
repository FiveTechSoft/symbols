#!/usr/bin/env python3
"""S12: critical measurement locations, synthetic portable XML, exact mutants.

This does not measure real Windows CTest XML or reason strings.
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
RESULT_CASES = {'missing_results', 'duplicate_results', 'nested_results', 'extra_nested_results'}


def checks(tool, tmp):
    spec = importlib.util.spec_from_file_location('report_locations', tool)
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
        for shape in ['direct_test', 'wrapper_results', 'wrapper_test']:
            r = ET.fromstring(BASE)
            t = r.find('Testing/Test')
            res = t.find('Results')
            n = next(n for n in res if n.get('name') == key)
            res.remove(n)
            if shape == 'direct_test':
                t.append(n)
            elif shape == 'wrapper_results':
                ET.SubElement(res, 'wrapper').append(n)
            else:
                ET.SubElement(t, 'wrapper').append(n)
            run(shape + '_' + key.replace(' ', '_'), r)
    for shape in sorted(RESULT_CASES):
        r = ET.fromstring(BASE)
        t = r.find('Testing/Test')
        res = t.find('Results')
        if shape == 'missing_results':
            t.remove(res)
        elif shape == 'duplicate_results':
            ET.SubElement(t, 'Results')
        elif shape == 'nested_results':
            t.remove(res)
            ET.SubElement(ET.SubElement(t, 'wrapper'), 'Results')
        else:
            ET.SubElement(ET.SubElement(t, 'wrapper'), 'Results')
        run(shape, r)
    r = ET.fromstring(BASE)
    r.find('Testing/Test/Results/NamedMeasurement/Value').text = 'Unknown reason'
    run('unknown_scalar', r, 'unknown')
    r = ET.fromstring(BASE)
    n = ET.SubElement(ET.SubElement(r.find('Testing/Test'), 'wrapper'), 'NamedMeasurement',
                      {'name': 'Extra diagnostic'})
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
            'location': ('if n not in direct_measurements:', 'if False:',
                         {shape + '_' + k.replace(' ', '_') for k in KEYS
                          for shape in ['direct_test', 'wrapper_results', 'wrapper_test']}),
            'results': ('if len(results) != 1 or len(list(t.iter("Results"))) != 1:',
                        'if False:', RESULT_CASES),
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
    print('PASS critical measurement locations: 19 cells, 2 exact-set mutants')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
